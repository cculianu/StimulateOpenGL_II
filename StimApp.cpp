#include <QTextEdit>
#include <QTcpServer>
#include <QMessageBox>
#include "StimApp.h"
#include "ConsoleWindow.h"
#include "Util.h"
#include "GLWindow.h"
#include <qglobal.h>
#include <QEvent>
#include "ConnectionThread.h"
#include <cstdlib>
#include <QSettings>
#include <QMetaType>
#include "StimPlugin.h"
#include <QTcpSocket>
#include <QStatusBar>
#include <QTimer>
#include <QKeyEvent>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QRadialGradient>
#include <QPainter>
#include <QDir>
#include <QDesktopWidget>
#include "StimGL_LeoDAQGL_Integration.h"
#include "ui_LeoDAQGLIntegration.h"

#define DEFAULT_WIN_SIZE QSize(800,600)

Q_DECLARE_METATYPE(unsigned);

namespace {
    struct Init {
        Init() {
            qRegisterMetaType<unsigned>("unsigned");
        }
    };

    Init * volatile init = 0;


    class LogLineEvent : public QEvent
    {
    public:
        LogLineEvent(const QString &str, const QColor & color)
            : QEvent((QEvent::Type)StimApp::LogLineEventType), str(str), color(color)
        {}
        QString str;
        QColor color;
    };

    class StatusMsgEvent : public QEvent
    {
    public:
        StatusMsgEvent(const QString &msg, int timeout)
            : QEvent((QEvent::Type)StimApp::StatusMsgEventType), msg(msg), timeout(timeout)
        {}
        QString msg;
        int timeout;
    };

};

StimApp * StimApp::singleton = 0;

StimApp::StimApp(int & argc, char ** argv)
    : QApplication(argc, argv, true), consoleWindow(0), glWindow(0), glWinHasFrame(true), debug(false), initializing(true), server(0), nLinesInLog(0), nLinesInLogMax(1000), glWinSize(DEFAULT_WIN_SIZE) /* default plugin size */
{
    if (singleton) {
        QMessageBox::critical(0, "Invariant Violation", "Only 1 instance of StimApp allowed per application!");
        std::exit(1);
    }
#ifndef Q_OS_WIN
    refresh = 120;
#endif
    Connect(this, SIGNAL(aboutToQuit()), this, SLOT(quitCleanup()));
    singleton = this;
    if (!::init) ::init = new Init;
    loadSettings();

    installEventFilter(this); // filter our own events

    consoleWindow = new ConsoleWindow;
    defaultLogColor = consoleWindow->textEdit()->textColor();

    Log() << "Application started";

    consoleWindow->installEventFilter(this);
    consoleWindow->textEdit()->installEventFilter(this);

    createGLWindow(false);
    glWindow->show();

    getHWFrameCount(); // forces error message to print once if frame count func is not found
    consoleWindow->setWindowTitle("StimulateOpenGL II");
    consoleWindow->resize(800, 300);
    int delta = 0;
#ifdef Q_WS_X11
    delta += 22; // this is a rough guesstimate to correct for window frame size being unknown on X11
#endif
    consoleWindow->move(0,glWindow->frameSize().height()+delta);
    consoleWindow->show();

    if (getNProcessors() > 2)
        setProcessAffinityMask(0x2|0x4); // set it to core 2 and core 3
    

    setRTPriority();
    setVSyncMode();

    glWindow->initPlugins();    

    initServer();

    createAppIcon();

#ifndef Q_OS_WIN
    if (getenv("NOCALIB")) {
#endif
        initializing = false;
        Log() << "Application initialized";    
#ifndef Q_OS_WIN
    } else
        calibrateRefresh();
#endif

    QTimer *timer = new QTimer(this);
    Connect(timer, SIGNAL(timeout()), this, SLOT(updateStatusBar()));
    timer->setSingleShot(false);
    timer->start(247); // update status bar every 247ms.. i like this non-round-numbre.. ;)
}

void StimApp::createGLWindow(bool initPlugs)
{
    glWindow = new GLWindow(glWinSize.width(), glWinSize.height(), !glWinHasFrame);
    glWindow->move(0,0);
    
    if (initPlugs) glWindow->initPlugins();
}

#ifndef Q_OS_WIN
void StimApp::calibrateRefresh()
{
    StimPlugin *p = glWindow->pluginFind("calibplugin"), *r = 0;
    if (!p) {
        QMessageBox::critical(0,
                              "Fatal Error", 
                              "Could not find refresh rate calibration plugin",
                              QMessageBox::Abort);
        std::exit(1);
    } else {
		if ((r=glWindow->runningPlugin()) && r != p) {
			unloadStim();
			r = glWindow->runningPlugin();
			if (r && r != p) return; // user must have aborted the operation
		}
        Log() << "Calibrating refresh rate...";
        refresh = 120;

        // temporarily suppress this notification of leodaqgl for this calib plugin
        p->start(true); // note this plugin ends up calling the calibratedRefresh() slot which then continues with initialization
    }
}

void StimApp::calibratedRefresh(unsigned rate)
{
#if defined(Q_OS_WIN) && !defined(Q_WS_X11)
    Log() << "Calibrated refresh: " << rate << "Hz, hardware says: " << (refresh=getHWRefreshRate()) << "Hz";
    if (refresh != rate) {
        Warning() << "Measured framerate differs from hardware rate.  Possible dropped frames?";
    }
	refresh = rate;
    //refresh = MIN(rate, refresh);
    Log() << "Using refresh rate of " << refresh << "Hz";
#else 
    refresh = rate;
    Log() << "Calibrated refresh: " << refresh << "Hz";
#endif

    if (initializing) {
        initializing = false;
        Log() << "Application initialized";
    }
}
#endif

StimApp::~StimApp()
{
    Log() << "Deleting Tcp server and closing connections..";
    delete server;
    saveSettings();
    singleton = 0;
}

void StimApp::quitCleanup()
{
    // NB: should delete these because they may have child objects with non-trivial destructors..
    if (glWindow) delete glWindow, glWindow = 0;
    if (consoleWindow) delete consoleWindow, consoleWindow = 0;
}

bool StimApp::isDebugMode() const
{
    // always true for now..
    return debug;
}

void StimApp::setDebugMode(bool d)
{
    debug = d;
    saveSettings();
}

bool StimApp::eventFilter(QObject *watched, QEvent *event)
{
    int type = static_cast<int>(event->type());
    if (type == QEvent::KeyPress) {
        // globally forward all keypresses to the glwindow
        // if they aren't handled, then return false for normal event prop.
        QKeyEvent *k = dynamic_cast<QKeyEvent *>(event);
        if (k && (watched == consoleWindow || watched == consoleWindow->textEdit())) 
            // force events for consoleWindow or textedit to the glwindow
            // note that the glWindow will also end up in this function
            // but will propagate down to QApplication::eventFilter() at bottom
            return sendEvent(glWindow, k);
    } 
    ConsoleWindow *cw = dynamic_cast<ConsoleWindow *>(watched);
    if (cw) {
        if (type == LogLineEventType) {
            LogLineEvent *evt = dynamic_cast<LogLineEvent *>(event);
            if (evt && cw->textEdit()) {
                QTextEdit *te = cw->textEdit();
                QColor origcolor = te->textColor();
                te->setTextColor(evt->color);
                te->append(evt->str);

                // make sure the log textedit doesn't grow forever
                // so prune old lines when a threshold is hit
                nLinesInLog += evt->str.split("\n").size();
                if (nLinesInLog > nLinesInLogMax) {
                    const int n2del = MAX(nLinesInLogMax/10, nLinesInLog-nLinesInLogMax);
                    QTextCursor cursor = te->textCursor();
                    cursor.movePosition(QTextCursor::Start);
                    for (int i = 0; i < n2del; ++i) {
                        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
                    }
                    cursor.removeSelectedText(); // deletes the lines, leaves a blank line
                    nLinesInLog -= n2del;
                }

                te->setTextColor(origcolor);
                te->moveCursor(QTextCursor::End);
                te->ensureCursorVisible();
                return true;
            } else {
                return false;
            }
        } else if (type == StatusMsgEventType) {
            StatusMsgEvent *evt = dynamic_cast<StatusMsgEvent *>(event);
            if (evt && cw->statusBar()) {
                cw->statusBar()->showMessage(evt->msg, evt->timeout);
                return true;
            } else {
                return false;
            }
        }
    }
    if (watched == this) {
        if (type == QuitEventType) {
            quit();
            return true;
        }
    }
    // otherwise do default action for event which probably means
    // propagate it down
    return QApplication::eventFilter(watched, event);
}

void StimApp::logLine(const QString & line, const QColor & c)
{
    qApp->postEvent(consoleWindow, new LogLineEvent(line, c.isValid() ? c : defaultLogColor));
}

void StimApp::initServer()
{
    class MyTcpServer : public QTcpServer
    {
    public:
        MyTcpServer(StimApp *parent) : QTcpServer(parent) {}
    protected:
        void incomingConnection(int sock) {
            QThread *thr = new ConnectionThread(sock, this);
            Connect(thr, SIGNAL(finished()), thr, SLOT(deleteLater()));
            thr->start();
            thr->setPriority(QThread::LowPriority);
        }
    };

    server = new MyTcpServer(this);
    static const unsigned short port = 4141;
    if (!server->listen(QHostAddress::Any, port)) {
        Error() << "Tcp server could not listen on port " << port;
        int but = QMessageBox::critical(0, "Network Listen Error", "Tcp server could not listen on port " + QString::number(port) + "\nAnother copy of this program might already be running.\nContinue anyway?", QMessageBox::Abort, QMessageBox::Ignore);
        if (but == QMessageBox::Abort) postEvent(this, new QEvent((QEvent::Type)QuitEventType)); // quit doesn't work here because we are in appliation c'tor
    } else {
        Log() << "Tcp server started, listening for connections on port " << server->serverPort();
    }
}

void StimApp::loadSettings()
{
    QSettings settings("janelia.hhmi.org", "StimulateOpenGL_II");

    settings.beginGroup("StimApp");
    debug = settings.value("debug", true).toBool();
    lastFile = settings.value("lastFile", "").toString();
    mut.lock();
#ifdef Q_OS_WIN
    outDir = settings.value("outDir", "c:/users/code/StimulusLogs").toString();
#else
    outDir = settings.value("outDir", QDir::homePath() + "/StimulusLogs").toString();
#endif
    mut.unlock();

    struct LeoDAQGLNotifyParams & leoDaq (leoDAQGLNotifyParams);
    leoDaq.enabled = settings.value("LeoDAQGL_Notify_Enabled", true).toBool();
    leoDaq.hostname = settings.value("LeoDAQGL_Notify_Host", "localhost").toString();
    leoDaq.port = settings.value("LeoDAQGL_Notify_Port",  LEODAQ_GL_NOTIFY_DEFAULT_PORT).toUInt();
    leoDaq.timeout_ms = settings.value("LeoDAQGL_Notify_TimeoutMS", LEODAQ_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS ).toInt();    
}

void StimApp::saveSettings()
{
    QSettings settings("janelia.hhmi.org", "StimulateOpenGL_II");

    settings.beginGroup("StimApp");
    settings.setValue("debug", debug);
    settings.setValue("lastFile", lastFile);
    mut.lock();
    settings.setValue("outDir", outDir);
    mut.unlock();

    struct LeoDAQGLNotifyParams & leoDaq (leoDAQGLNotifyParams);
    settings.setValue("LeoDAQGL_Notify_Enabled", leoDaq.enabled);
    settings.setValue("LeoDAQGL_Notify_Host", leoDaq.hostname);
    settings.setValue("LeoDAQGL_Notify_Port",  leoDaq.port);
    settings.setValue("LeoDAQGL_Notify_TimeoutMS", leoDaq.timeout_ms);    
}


void StimApp::lockMouseKeyboard()
{
    if (StimApp::instance()) {
        StimApp::instance()->console()->grabMouse(Qt::WaitCursor);
        StimApp::instance()->console()->grabKeyboard();
    }
}
void StimApp::releaseMouseKeyboard()
{
    if (StimApp::instance()) {
        StimApp::instance()->console()->releaseMouse();
        StimApp::instance()->console()->releaseKeyboard();
    }
}

void StimApp::statusMsg(const QString &msg, int timeout)
{
    qApp->postEvent(consoleWindow, new StatusMsgEvent(msg, timeout));
}

void StimApp::updateStatusBar()
{
    StimPlugin *p = 0;
    Status sb;
    if (glWindow && (p = glWindow->runningPlugin())) {
        sb << p->name() << " " << (glWindow->isPaused() ? "paused " : "running ");
        sb << " - Frame " << p->getFrameNum();
        sb << " - FPS Avg/Min/Max: " << p->getFps() << "/" << p->getFpsMin() << "/" << p->getFpsMax();
        if (p->getSBString().length())
            sb << " - " << p->getSBString();
    } else {
        sb << "No plugins running (hit L to load a plugin)";
    }
}

/** \brief A helper class that helps prevent reentrancy into certain functions.

    Mainly StimApp::loadStim(), StimApp::unloadStim(), and StimApp::pickOutputDir() make use of this class to prevent recursive calls into themselves. 

    Functions that want to be mutually exclusive with respect to each other
    and non-reentrant with respect to themselves need merely construct an 
    instance of this class as a local variable, and
    then reentrancy into the function can be guarded by checking against
    this class's operator bool() function.
*/
struct ReentrancyPreventer
{
    static volatile int ct;
    /// Increments a global counter.
    /// The global counter is 1 if only 1 instance of this class exists throughout the application, and >1 otherwise.
    ReentrancyPreventer() { ++ct; }
    /// Decrements the global counter.  
    /// If it reaches 0 this was the last instance of this class and there are no other ones active globally.
    ~ReentrancyPreventer() {--ct; }
    /// Returns true if the global counter is 1 (that is, only one globally active instance of this class exists throughout the application), and false otherwise.  If false is returned, you can then abort your function early as a reentrancy condition has been detected.
    operator bool() const { return ct == 1; }
};
volatile int ReentrancyPreventer::ct = 0;

void StimApp::loadStim()
{
    unloadStim();
    ReentrancyPreventer rp; if (!rp) return;

    QString lf;
    if ( !(lf = QFileDialog::getOpenFileName(0, "Choose a protocol spec to open", lastFile)).isNull() ) { 
        lastFile = lf;
        saveSettings(); // just to remember the file *now*
        QFile f(lastFile);
        if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            QMessageBox::critical(0, "Could not open", QString("Could not open '") + lastFile + "' for reading.");
            return;
        }
        QTextStream ts(&f);
        QString line;
        line = ts.readLine().trimmed();
        Debug() << "stim plugin: " << line;
        StimPlugin *pfound = glWindow->pluginFind(line), *p = glWindow->runningPlugin();
        if (!pfound) {
            QMessageBox::critical(0, "Plugin not found", QString("Plugin '") + line + "' not found.");
            return;
        }
        if (p && p->getFrameNum() > -1) {
            bool doSave = 0; //QMessageBox::question(0, "Save data?", QString("Save data for currently-running '") + p->name() + "'?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes;
            if (p != glWindow->runningPlugin()) {
                QMessageBox::information(0, "Already unloaded", 
                                         "Plugin already unloaded in the meantime!");
            } else {
                p->stop(doSave, true);
            }
        }
        p = pfound;

        StimParams params;
        QString paramsBuf;  
        QTextStream tsparams(&paramsBuf, QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text);
        // now parse remaining lines which should be name/value pairs
        while ( !(line = ts.readLine()).isNull() ) {
            tsparams << line << "\n";
        }
        tsparams.flush();
        params.fromString(paramsBuf);

        {
            // custom window size handling: mon_x_pix and mon_y_pix
            QSize desiredSize(DEFAULT_WIN_SIZE);
            if (params.contains("mon_x_pix")) 
                desiredSize.setWidth(params["mon_x_pix"].toUInt());
            if (params.contains("mon_y_pix")) 
                desiredSize.setHeight(params["mon_y_pix"].toUInt());
            
            if (desiredSize != glWinSize && !desiredSize.isEmpty()) {
                Log() << "GLWindow size changed to: " << desiredSize.width() << "x" << desiredSize.height();
                QString pname = p->name();
                // need to (re)create the gl window with the desired size!
                QDesktopWidget desktop;
                const QRect screenRect(desktop.screenGeometry(glWindow));
                delete glWindow;
                glWinSize = desiredSize;                
                createGLWindow();
                glWindow->move(screenRect.x(), screenRect.y());
                glWindow->show();
                p = pfound = glWindow->pluginFind(pname);
            }
        }

        QFileInfo fi(lastFile);
        Log() << fi.fileName() << " loaded";
        p->setParams(params);       
        p->start();
        glWindow->setWindowTitle(QString("StimulateOpenGL II - ") + fi.fileName());
    }
}

void StimApp::unloadStim()
{
    ReentrancyPreventer rp; if (!rp) return;
    StimPlugin *p = glWindow->runningPlugin();
    if (p) { // if there is a running plugin and it actually ran
        int b = QMessageBox::question(0, "Really stop plugin?", QString("Are you sure you wish to stop '") + p->name() + "'?", QMessageBox::Ok, QMessageBox::Cancel);
        if (b == QMessageBox::Ok) {
            if (p != glWindow->runningPlugin())
                QMessageBox::information(0, "Already unloaded", 
                                         "Plugin already unloaded in the meantime!");
            else if (/*p->getFrameNum() > -1*/true) {
                bool doSave = 0; //QMessageBox::question(0, "Save data?", QString("Save data for '") + p->name() + "'?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes;
                if (p != glWindow->runningPlugin()) {
                    QMessageBox::information(0, "Already unloaded", 
                                             "Plugin already unloaded in the meantime!");
                } else
                    p->stop(doSave, true);
                
            }
        }
    }
}

void StimApp::about()
{
    QMessageBox::about(consoleWindow, "About StimulateOpenGL II", 
                       VERSION_STR 
                       "\n\n(C) 2008 Calin A. Culianu <cculianu@yahoo.com>\n\n"
                       "Developed for the Anthony Leonardo lab at\n"
                       "Janelia Farm Research Campus, HHMI\n\n"
                       "Software License: GPL v2 or later");
}

bool StimApp::setOutputDirectory(const QString & dpath)
{
    QDir d(dpath);
    if (!d.exists()) return false;
    mut.lock();
    outDir = dpath;
    mut.unlock();
    return true;
}

void StimApp::pickOutputDir()
{
    ReentrancyPreventer rp; if (!rp) return;
    mut.lock();
    QString od = outDir;
    mut.unlock();
    if ( !(od = QFileDialog::getExistingDirectory(0, "Choose a directory to which to save output files", od, QFileDialog::DontResolveSymlinks|QFileDialog::ShowDirsOnly)).isNull() ) { 
        mut.lock();
        outDir = od;
        mut.unlock();
        saveSettings(); // just to remember the file *now*
    }
}

void StimApp::createAppIcon()
{
    QPixmap pm(QSize(128, 128));
    pm.fill(Qt::transparent);
    QRadialGradient gradient(50, 50, 50, 50, 50);
    gradient.setColorAt(0, QColor::fromRgbF(1, 0, 0, 1));
    gradient.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
    QPainter painter(&pm);
    painter.fillRect(0, 0, 128, 128, gradient);
    painter.end();
    consoleWindow->setWindowIcon(QIcon(pm));
    
    pm.fill(Qt::transparent);
    painter.begin(&pm);
    
    gradient.setColorAt(0, QColor::fromRgbF(0, 0, 1, 1));
    gradient.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));

    painter.fillRect(0, 0, 128, 128, gradient);

    glWindow->setWindowIcon(QIcon(pm));
}

void StimApp::pauseUnpause() // forwards to glWindow->pauseUnpause
{
    if (glWindow) glWindow->pauseUnpause();
}

void StimApp::hideUnhideConsole()
{
    if (consoleWindow) {
        if (consoleWindow->isHidden()) consoleWindow->show();
        else {
            bool hadfocus = ( focusWidget() == consoleWindow );
            consoleWindow->hide();
            if (glWindow && hadfocus) glWindow->setFocus(Qt::OtherFocusReason);
        }
    }
}

void StimApp::alignGLWindow()
{
    if (glWindow) {
        if (glWindow->runningPlugin()) {
            Warning() << "Align of GLWindow not possible while a plugin is loaded.  Please unload the current Stim Plugin then try to align again.";
        } else {
            QDesktopWidget desktop;
            const QRect screenRect(desktop.screenGeometry(glWindow));
            delete glWindow;
            glWinHasFrame = !glWinHasFrame;
            createGLWindow();
            glWindow->aMode = !glWinHasFrame; // 'A' mode if we lack a frame
            glWindow->move(screenRect.x(), screenRect.y());
            glWindow->show();
        }
    }
}

void StimApp::leoDAQGLIntegrationDialog()
{
    QDialog dlg(0);
    dlg.setWindowTitle("LeoDAQGL Integration Options");
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setModal(true);
    LeoDAQGLNotifyParams & p (leoDAQGLNotifyParams);

    Ui::LeoDAQGLIntegration controls;
    controls.setupUi(&dlg);
    controls.enabledGB->setChecked(p.enabled);
    controls.hostNameLE->setText(p.hostname);
    controls.portSB->setValue(p.port);
    controls.timeoutSB->setValue(p.timeout_ms);    
    if ( dlg.exec() == QDialog::Accepted ) {
        p.enabled = controls.enabledGB->isChecked();
        p.hostname = controls.hostNameLE->text();
        p.port = controls.portSB->value();
        p.timeout_ms = controls.timeoutSB->value();
    }
    saveSettings();
}
