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
#include <QGraphicsView>
#include <QGraphicsScene>
#include "StimGL_SpikeGL_Integration.h"
#include "ui_SpikeGLIntegration.h"
#include "ui_ParamDefaultsWindow.h"
#include "ui_HotspotConfig.h"
#include "ui_WarpingConfig.h"
#include "DAQ.h"

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
    : QApplication(argc, argv, true), consoleWindow(0), glWindow(0), glWinHasFrame(true), debug(false), initializing(true), server(0), nLinesInLog(0), nLinesInLogMax(1000), glWinSize(DEFAULT_WIN_SIZE) /* default plugin size */, tmphs(0), tmpwc(0)
{
    if (singleton) {
        QMessageBox::critical(0, "Invariant Violation", "Only 1 instance of StimApp allowed per application!");
        std::exit(1);
    }
	setApplicationName("StimGL");
	setApplicationVersion(VERSION_STR);
#ifndef Q_OS_WIN
    refresh = 120;
#endif
    Connect(this, SIGNAL(aboutToQuit()), this, SLOT(quitCleanup()));
    singleton = this;
    if (!::init) ::init = new Init;
    loadSettings();
	glWinSize = QSize(globalDefaults.mon_x_pix, globalDefaults.mon_y_pix);

    installEventFilter(this); // filter our own events

    consoleWindow = new ConsoleWindow;
    defaultLogColor = consoleWindow->textEdit()->textColor();

    Log() << "Application started";

    consoleWindow->installEventFilter(this);
    consoleWindow->textEdit()->installEventFilter(this);

    createGLWindow(false);
	loadSettings(); /* NB we loadSettings again because it has a side-effect of 
					   setting some glWindow class properties, and the first time 
					   we ran loadSettings(), glWindow was NULL */
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
		
	setVSyncDisabled(isVSyncDisabled());

    glWindow->initPlugins();    

    initServer();

    createAppIcon();

#ifdef Q_OS_WIN
	Log() << "Hardware reports refresh rate is " << getHWRefreshRate() << "Hz";
#else
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

    if (globalDefaults.doHotspotCorrection) glWindow->setHotspot(GetHotspotImageXFormed(globalDefaults.hotspotImageFile,globalDefaults.hsAdj,glWinSize));
    if (globalDefaults.doWarping) glWindow->setWarp(parseWarpingFile(globalDefaults.warpingFile,glWinSize));
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
#else /* Windows */
void StimApp::calibrateRefresh() {}
void StimApp::calibratedRefresh(unsigned rate) { (void) rate; }
#endif

StimApp::~StimApp()
{
	if (glWindow) glWindow->criticalCleanup();
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
    return debug;
}

void StimApp::setDebugMode(bool d)
{
    debug = d;
    saveSettings();
}

void StimApp::setExcessiveDebugMode(bool d)
{
    Util::excessiveDebug = d;
}

bool StimApp::isSaveFrameVars() const
{
    return saveFrameVars;
}


bool StimApp::isSaveParamHistory() const
{
    return saveParamHistory;
}

void StimApp::setSaveFrameVars(bool b)
{
    saveFrameVars = b;
    saveSettings();
	if (glWindow->runningPlugin()) {
		QMessageBox::information(0, "Plugin Restart Required",
								 "The new Save Frame Vars setting requires a plugin unload/reload to take effect, or will automatically take effect for the next plugin to run."
								 );
	}
}

void StimApp::setSaveParamHistory(bool b)
{
    saveParamHistory = b;
    saveSettings();
	if (saveParamHistory)
		Log() << "Param history save enabeld: Saving to directory '" << outputDirectory() << "' on plugin stop.";
	else
		Log() << "Param history save disabled.";
}

void StimApp::setNoDropFrameWarn(bool b) {
	noDropFrameWarn = b;
	saveSettings();
}

bool StimApp::isNoDropFrameWarn() const {
	return noDropFrameWarn;
}

bool StimApp::isFrameDumpMode() const
{
    if (glWindow) return glWindow->debugLogFrames;
	return false;
}
void StimApp::setFrameDumpMode(bool b)
{
	if (glWindow) glWindow->debugLogFrames = b;
}

bool StimApp::isVSyncDisabled() const {
	return vsyncDisabled;
}
void StimApp::setVSyncDisabled(bool b) {
	
	if (glWindow) {
		glWindow->makeCurrent();
		Util::setVSyncMode(!b);
	}
	vsyncDisabled = b;
	saveSettings();
	
	if (consoleWindow && consoleWindow->vsyncDisabledAction) {
		consoleWindow->vsyncDisabledAction->blockSignals(true);
		consoleWindow->vsyncDisabledAction->setChecked(b);
		consoleWindow->vsyncDisabledAction->blockSignals(false);
	}
}

bool StimApp::eventFilter(QObject *watched, QEvent *event)
{
    int type = static_cast<int>(event->type());
    if (type == QEvent::KeyPress) {
        // globally forward all keypresses to the glwindow
        // if they aren't handled, then return false for normal event prop.
        QKeyEvent *k = dynamic_cast<QKeyEvent *>(event);
	    if (k 
			&& k->modifiers() == Qt::NoModifier // make sure CTRL/ALT/COMMAND not pressed so that CTRL-C works		
			&& (watched == consoleWindow || watched == consoleWindow->textEdit()))  
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
#if QT_VERSION >= 0x050000
        void incomingConnection(qintptr sock)
#else
        void incomingConnection(int sock)
#endif
        {
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
    debug = settings.value("debug", false).toBool();
	noDropFrameWarn = settings.value("noDropFrameWarn", false).toBool();
	saveFrameVars = settings.value("saveFrameVars", false).toBool();
	saveParamHistory = settings.value("saveParamHistory", false).toBool();
	vsyncDisabled = settings.value("noVSync", false).toBool();
    lastFile = settings.value("lastFile", "").toString();
    mut.lock();
#ifdef Q_OS_WIN
    outDir = settings.value("outDir", "c:/users/code/StimulusLogs").toString();
#else
    outDir = settings.value("outDir", QDir::homePath() + "/StimulusLogs").toString();
#endif
    mut.unlock();
	lastFMV = settings.value("lastFMV", outDir).toString();

    struct SpikeGLNotifyParams & leoDaq (spikeGLNotifyParams);
    leoDaq.enabled = settings.value("LeoDAQGL_Notify_Enabled", true).toBool();
    leoDaq.hostname = settings.value("LeoDAQGL_Notify_Host", "localhost").toString();
    leoDaq.port = settings.value("LeoDAQGL_Notify_Port",  SPIKE_GL_NOTIFY_DEFAULT_PORT).toUInt();
    leoDaq.timeout_ms = settings.value("LeoDAQGL_Notify_TimeoutMS", SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS ).toInt();    
	leoDaq.nloopsNotifyPerIter = settings.value("LeoDAQGL_Notify_NLoopsPerIter", false).toBool();

	struct GlobalDefaults & defs (globalDefaults);
	defs.mon_x_pix = settings.value("mon_x_pix", defs.mon_x_pix).toInt();
	defs.mon_y_pix = settings.value("mon_y_pix", defs.mon_y_pix).toInt();
	defs.Nblinks = settings.value("Nblinks", defs.Nblinks).toInt();
	defs.ftrackbox_x = settings.value("ftrackbox_x", defs.ftrackbox_x).toInt();
	defs.ftrackbox_y = settings.value("ftrackbox_y", defs.ftrackbox_y).toInt();
	defs.ftrackbox_w = settings.value("ftrackbox_w", defs.ftrackbox_w).toInt();
	defs.ftrack_track_color = settings.value("ftrack_track_color", defs.ftrack_track_color).toString();
	defs.ftrack_off_color = settings.value("ftrack_off_color", defs.ftrack_off_color).toString();
	defs.ftrack_change_color = settings.value("ftrack_change_color", defs.ftrack_change_color).toString();
	defs.ftrack_start_color = settings.value("ftrack_start_color", defs.ftrack_start_color).toString();
	defs.ftrack_end_color = settings.value("ftrack_end_color", defs.ftrack_end_color).toString();
    defs.hotspotImageFile = settings.value("hotspot_image_filename", defs.hotspotImageFile).toString();
    defs.doHotspotCorrection = settings.value("hotspot_correction_enabled", defs.doHotspotCorrection).toBool();
    qstrncpy(defs.color_order, settings.value("color_order", defs.color_order).toString().toUtf8().constData(), 4);
	defs.fps_mode = settings.value("fps_mode", defs.fps_mode).toInt();
	defs.DO_with_vsync = settings.value("DO_with_vsync", defs.DO_with_vsync).toString();
	if (!DAQ::DOChannelExists(defs.DO_with_vsync)) {
		defs.DO_with_vsync = "off";
	}
	defs.interTrialBg = settings.value("clearColor_interTrialBg", defs.interTrialBg).toString();

    defs.hsAdj.xrot  = settings.value("hs_xrot", 0.).toDouble();
    defs.hsAdj.yrot  = settings.value("hs_yrot", 0.).toDouble();
    defs.hsAdj.zrot  = settings.value("hs_zrot", 0.).toDouble();
    defs.hsAdj.zoom  = settings.value("hs_zoom", 1.).toDouble();
    defs.hsAdj.xtrans= settings.value("hs_xtrans", 0).toInt();
    defs.hsAdj.ytrans= settings.value("hs_ytrans", 0).toInt();

    defs.warpingFile = settings.value("warping_file", "").toString();
    defs.doWarping = settings.value("warp_enabled", false).toBool();
}

void StimApp::saveSettings()
{
    QSettings settings("janelia.hhmi.org", "StimulateOpenGL_II");

    settings.beginGroup("StimApp");
    settings.setValue("debug", debug);
	settings.setValue("noDropFrameWarn", noDropFrameWarn);
	settings.setValue("saveFrameVars", saveFrameVars);
	settings.setValue("saveParamHistory", saveParamHistory);
    settings.setValue("lastFile", lastFile);
	settings.setValue("lastFMV", lastFMV);
	settings.setValue("noVSync", vsyncDisabled);
    mut.lock();
    settings.setValue("outDir", outDir);
    mut.unlock();

    struct SpikeGLNotifyParams & leoDaq (spikeGLNotifyParams);
    settings.setValue("LeoDAQGL_Notify_Enabled", leoDaq.enabled);
    settings.setValue("LeoDAQGL_Notify_Host", leoDaq.hostname);
    settings.setValue("LeoDAQGL_Notify_Port",  leoDaq.port);
    settings.setValue("LeoDAQGL_Notify_TimeoutMS", leoDaq.timeout_ms);    
	settings.setValue("LeoDAQGL_Notify_NLoopsPerIter", leoDaq.nloopsNotifyPerIter);

	struct GlobalDefaults & defs (globalDefaults);
	settings.setValue("mon_x_pix", defs.mon_x_pix);
	settings.setValue("mon_y_pix", defs.mon_y_pix);
	settings.setValue("ftrackbox_x", defs.ftrackbox_x);
	settings.setValue("ftrackbox_y", defs.ftrackbox_y);
	settings.setValue("ftrackbox_w", defs.ftrackbox_w);
	settings.setValue("Nblinks", defs.Nblinks);
	settings.setValue("ftrack_track_color", defs.ftrack_track_color);
	settings.setValue("ftrack_off_color", defs.ftrack_off_color);
	settings.setValue("ftrack_change_color", defs.ftrack_change_color);
	settings.setValue("ftrack_start_color", defs.ftrack_start_color);
	settings.setValue("ftrack_end_color", defs.ftrack_end_color);
	settings.setValue("color_order", QString(defs.color_order));
	settings.setValue("fps_mode", defs.fps_mode);
	settings.setValue("DO_with_vsync", defs.DO_with_vsync);
	settings.setValue("clearColor_interTrialBg", defs.interTrialBg);
    settings.setValue("hotspot_image_filename", defs.hotspotImageFile);
    settings.setValue("hotspot_correction_enabled", defs.doHotspotCorrection);
    settings.setValue("hs_xrot", defs.hsAdj.xrot);
    settings.setValue("hs_yrot", defs.hsAdj.yrot);
    settings.setValue("hs_zrot", defs.hsAdj.zrot);
    settings.setValue("hs_zoom", defs.hsAdj.zoom);
    settings.setValue("hs_xtrans", defs.hsAdj.xtrans);
    settings.setValue("hs_ytrans", defs.hsAdj.ytrans);

    settings.setValue("warping_file", defs.warpingFile);
    settings.setValue("warp_enabled", defs.doWarping);
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

void StimApp::checkFMV()
{
	QString file = QFileDialog::getOpenFileName(0, "Choose a .FMV file to check", lastFMV);
	if (file.isNull()) return;
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(0, "Could not open", QString("Could not open '") + file + "' for reading.");
		return;
	}
	lastFMV = file;
	saveSettings();
	emit gotCheckFMV(file);
}

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
		bool isMovieFile = lastFile.toLower().endsWith(".fmv") || lastFile.toLower().endsWith(".gif");
		
		StimPlugin *p = 0;		
		QString pname;
		QStack<StimPlugin::ParamHistoryEntry> hist;
		
		if (!isMovieFile 
			&& StimPlugin::fileAppearsToBeValidParamHistory(lastFile) 
			&& StimPlugin::parseParamHistoryString(pname, hist, QString(f.readAll()))) {
			// It's a param history!  Huzzah!  Just read it in and set param history
			p = glWindow->pluginFind(pname);
			if (!p) {
				QMessageBox::critical(0, "Plugin not found", QString("Plugin '") + pname + "' not found.");
				return;
			}
			StimPlugin *prunning = 0;
			if ((prunning = glWindow->runningPlugin())) {
				prunning->stop(false, true);
			}
			if (p) p->setPendingParamHistory(hist);
			
		} else {
			// if we get here, it's a regular stim param file..
			
			QTextStream ts(&f);
			QString dummy;
			if (isMovieFile) {
				// turns out it was a movie file.  we support this 'feature', for
				// the movie plugin, for convenience.  Added 3/9/2015 by Calin because
				// I was annoyed at trying to open .fmv files and having to modify
				// the .txt file each time.. :)
				dummy = QString("movie\nfile=%1\n").arg(lastFile);
				ts.setString(&dummy);
			}
			QString line;
			do {
				line = ts.readLine().trimmed();
			} while (!line.length() && !ts.atEnd());
			Debug() << "stim plugin: " << line;
			StimPlugin *pfound = glWindow->pluginFind(line);
			p = glWindow->runningPlugin();
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
			{
				params.clear();
				GlobalDefaults & defs (globalDefaults);
				// set params from defaults
				params["mon_x_pix"] = defs.mon_x_pix;
				params["mon_y_pix"] = defs.mon_y_pix;
				params["ftrackbox_x"] = defs.ftrackbox_x;
				params["ftrackbox_y"] = defs.ftrackbox_y;
				params["ftrackbox_w"] = defs.ftrackbox_w;
				params["ftrack_track_color"] = defs.ftrack_track_color;
				params["ftrack_off_color"] = defs.ftrack_off_color;
				params["ftrack_change_color"] = defs.ftrack_change_color;
				params["ftrack_start_color"] = defs.ftrack_start_color;
				params["ftrack_end_color"] = defs.ftrack_end_color;
				params["fps_mode"] = defs.fps_mode == 0 ? "single" : (defs.fps_mode == 1 ? "double" : "triple");
				params["color_order"] = defs.color_order;
				params["DO_with_vsync"] = defs.DO_with_vsync;
				params["Nblinks"] = defs.Nblinks;
				params["nblinks"] = defs.Nblinks; // case insensitive?
			}
			params.fromString(paramsBuf, false);

			{
				QString devChan;
				if ( (devChan = params["DO_with_vsync"].toString()) != "off" && devChan.length()) {
					DAQ::WriteDO(devChan, false); // set it low initially..
				}
			}
			

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
					delete glWindow, glWindow = 0;
					glWinSize = desiredSize;                
					createGLWindow();
					glWindow->move(screenRect.x(), screenRect.y());
					glWindow->show();
			/* NB: this needs to be here because Qt OpenGL on Windows 
			acts funny when we recreate the window and use it immediately.
			I suspect some Qt event needs to fire to properly create the 
			window and the OpenGL context. */
					qApp->processEvents();

					p = pfound = glWindow->pluginFind(pname);
				}
			}			
			
			p->setParams(params);       

		}
		QFileInfo fi(lastFile);
		Log() << fi.fileName() << " loaded";
		QString stimfile = fi.fileName();
		
		if ( p->start() ) {
			glWindow->setWindowTitle(QString("StimulateOpenGL II - ") + stimfile);
		} else {
			QMessageBox::critical(0, QString("Plugin Failed to Start"), QString("Plugin ") + p->name() + " failed to start.  Check the console for errors.");
			p->stop();
		}
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
                       "\n\n(C) 2008-2016 Calin A. Culianu <calin.culianu@gmail.com>\n\n"
                       "Developed for the Anthony Leonardo lab at\n"
                       "Janelia Farm Research Campus HHMI\n\n"
                       "Software License: GPL v2 or later\n\n"
					   "Bitcoin Address: 1Ca1inQuedcKdyELCTmN8AtKTTehebY4mC\n"
					   "Git Repository: https://www.github.com/cculianu/StimulateOpenGL_II");
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
            delete glWindow; glWindow = 0;
            glWinHasFrame = !glWinHasFrame;
            createGLWindow();
            glWindow->aMode = !glWinHasFrame; // 'A' mode if we lack a frame
            glWindow->move(screenRect.x(), screenRect.y());
            glWindow->show();
        }
    }
}

void StimApp::spikeGLIntegrationDialog()
{
    QDialog dlg(consoleWindow);
    dlg.setWindowTitle("SpikeGL Integration Options");
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setModal(true);
    SpikeGLNotifyParams & p (spikeGLNotifyParams);

    Ui::SpikeGLIntegration controls;
    controls.setupUi(&dlg);
    controls.enabledGB->setChecked(p.enabled);
    controls.hostNameLE->setText(p.hostname);
    controls.portSB->setValue(p.port);
    controls.timeoutSB->setValue(p.timeout_ms);    
	controls.nloopsNotifyCB->setChecked(p.nloopsNotifyPerIter);
    if ( dlg.exec() == QDialog::Accepted ) {
        p.enabled = controls.enabledGB->isChecked();
        p.hostname = controls.hostNameLE->text();
        p.port = controls.portSB->value();
        p.timeout_ms = controls.timeoutSB->value();
		p.nloopsNotifyPerIter = controls.nloopsNotifyCB->isChecked();
    }
    saveSettings();
}

void StimApp::globalDefaultsDialog()
{
    QDialog dlg(consoleWindow);
    dlg.setWindowTitle("Global Parameter Defaults");
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setModal(true);
    GlobalDefaults & g (globalDefaults);

    Ui::ParamDefaultsWindow controls;
    controls.setupUi(&dlg);
	controls.cb_fps_mode->setCurrentIndex(g.fps_mode);
	controls.cb_color_order->setCurrentIndex(controls.cb_color_order->findText(g.color_order));
	controls.sb_mon_x_pix->setValue(g.mon_x_pix);
	controls.sb_mon_y_pix->setValue(g.mon_y_pix);
	controls.sb_nblinks->setValue(g.Nblinks);
	controls.sb_ftrackbox_x->setValue(g.ftrackbox_x);
	controls.sb_ftrackbox_y->setValue(g.ftrackbox_y);
	controls.sb_ftrackbox_w->setValue(g.ftrackbox_w);
	controls.le_ftrack_track->setText(g.ftrack_track_color);
	controls.le_ftrack_off->setText(g.ftrack_off_color);
	controls.le_ftrack_change->setText(g.ftrack_change_color);
	controls.le_ftrack_start->setText(g.ftrack_start_color);
	controls.le_ftrack_end->setText(g.ftrack_end_color);
	controls.le_inter_trial_bg->setText(g.interTrialBg);
    controls.chk_hotspot->setChecked(g.doHotspotCorrection);
    controls.warpingChk->setChecked(g.doWarping);

	DAQ::DeviceChanMap chanMap (DAQ::ProbeAllDOChannels());
	int selected = 0;
	for (DAQ::DeviceChanMap::const_iterator it = chanMap.begin(); it != chanMap.end(); ++it) {
		for (QStringList::const_iterator it2 = (*it).begin(); it2 != (*it).end(); ++it2) {			
			const QString & item (*it2);
			controls.cb_do_with_vsync->addItem(item);
			if (0 == item.compare(g.DO_with_vsync, Qt::CaseInsensitive)) {
				selected = controls.cb_do_with_vsync->count()-1;
			}
		}
	}
	controls.cb_do_with_vsync->setCurrentIndex(selected);
	
    Connect(controls.but_hotspot, SIGNAL(clicked()), this, SLOT(configureHotspotDialog()));
    Connect(controls.but_warping, SIGNAL(clicked()), this, SLOT(configureWarpingDialog()));

    QString saved_hsimg = g.hotspotImageFile, saved_warping = g.warpingFile;
    QImage saved_qimg = glWindow->hotspot(), saved_wimg = glWindow->warp();
    GlobalDefaults::HSAdjust saved_adj = g.hsAdj;

    if ( dlg.exec() == QDialog::Accepted ) {
		g.fps_mode = controls.cb_fps_mode->currentIndex();
		qstrncpy(g.color_order, controls.cb_color_order->currentText().toLatin1().constData(), 4);
		g.mon_x_pix = controls.sb_mon_x_pix->value();
		g.mon_y_pix = controls.sb_mon_y_pix->value();
		g.ftrackbox_x = controls.sb_ftrackbox_x->value();
		g.ftrackbox_y = controls.sb_ftrackbox_y->value();
		g.ftrackbox_w = controls.sb_ftrackbox_w->value();
		g.Nblinks = controls.sb_nblinks->value();
		g.DO_with_vsync = controls.cb_do_with_vsync->currentText();
		g.ftrack_track_color = controls.le_ftrack_track->text();
		g.ftrack_off_color = controls.le_ftrack_off->text();
		g.ftrack_change_color = controls.le_ftrack_change->text();
		g.ftrack_start_color = controls.le_ftrack_start->text();
		g.ftrack_end_color = controls.le_ftrack_end->text();
		g.interTrialBg = controls.le_inter_trial_bg->text();
        g.doHotspotCorrection = controls.chk_hotspot->isChecked();
        g.doWarping = controls.warpingChk->isChecked();
        if (glWindow) {
            glWindow->setClearColor(g.interTrialBg); // take effect right now!
            if (g.doHotspotCorrection)
                glWindow->setHotspot(GetHotspotImageXFormed(g.hotspotImageFile, g.hsAdj, glWinSize));
            else
                glWindow->clearHotspot();
            if (g.doWarping)
                glWindow->setWarp(parseWarpingFile(g.warpingFile, glWinSize));
            else
                glWindow->clearWarp();
        }
    } else {
        g.hotspotImageFile = saved_hsimg;
        g.hsAdj = saved_adj;       
        glWindow->setHotspot(saved_qimg);
        g.warpingFile = saved_warping;
        glWindow->setWarp(saved_wimg);
    }

    saveSettings();
}

void StimApp::configureHotspotDialog()
{
    QDialog dlg(consoleWindow);
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setModal(true);

    Ui::HotspotConfig h;
    h.setupUi(&dlg);

    GlobalDefaults & g(globalDefaults);

    h.lbl_windims->setText(QString("%1 x %2").arg(g.mon_x_pix).arg(g.mon_y_pix));
    h.lbl_filename->setText(g.hotspotImageFile.length() ? g.hotspotImageFile : "NONE SPECIFIED");
    h.dsb_xrot->setValue(g.hsAdj.xrot);
    h.dsb_yrot->setValue(g.hsAdj.yrot);
    h.dsb_zrot->setValue(g.hsAdj.zrot);
    h.dsb_zoom->setValue(g.hsAdj.zoom*100.0);
    h.sb_xtrans->setValue(g.hsAdj.xtrans);
    h.sb_ytrans->setValue(g.hsAdj.ytrans);

    tmphs = &h;
    QTimer::singleShot(100,this,SLOT(gotNewHSFile())); // need to call this from a timer becuase geometry is weird before dialog is shown..

    Connect(h.but_load, SIGNAL(clicked()), this, SLOT(loadHSClicked()));

    Connect(h.dsb_xrot, SIGNAL(valueChanged(double)), this, SLOT(hotspotAdjSlot()));
    Connect(h.dsb_yrot, SIGNAL(valueChanged(double)), this, SLOT(hotspotAdjSlot()));
    Connect(h.dsb_zrot, SIGNAL(valueChanged(double)), this, SLOT(hotspotAdjSlot()));
    Connect(h.dsb_zoom, SIGNAL(valueChanged(double)), this, SLOT(hotspotAdjSlot()));
    Connect(h.sb_xtrans, SIGNAL(valueChanged(int)), this, SLOT(hotspotAdjSlot()));
    Connect(h.sb_ytrans, SIGNAL(valueChanged(int)), this, SLOT(hotspotAdjSlot()));
    Connect(h.but_reset, SIGNAL(clicked()), this, SLOT(hotspotAdjResetSlot()));

    GlobalDefaults::HSAdjust saved_adj = g.hsAdj;
    QImage saved_img = glWindow->hotspot();

    StimPlugin *p = glWindow->runningPlugin(), *dummy = 0;

    if (!p) {
        // run this dummy plugin so we can get live previews of the hotspot image...
        p = glWindow->pluginFind("DummyPlugin");
        if (p) (dummy=p)->start(true);
    }

    if ( dlg.exec() == QDialog::Accepted ) {
        g.hotspotImageFile = h.lbl_filename->text();
    } else {
        g.hsAdj = saved_adj;
        glWindow->setHotspot(saved_img);
    }

    if (dummy && glWindow->runningPlugin() == dummy) dummy->stop();

    tmphs = 0;
}

void StimApp::gotNewHSFile()
{
    if (!tmphs) return;

    QImage img(tmphs->lbl_filename->text());
    int origW = img.width(), origH = img.height();
    QPixmap pm;
    QGraphicsScene *oldsc = tmphs->graphicsView->scene(), *newsc = new QGraphicsScene(tmphs->graphicsView);
    if (!img.isNull()) {

        img = GetHotspotImageXFormed(tmphs->lbl_filename->text(),globalDefaults.hsAdj,glWinSize);

        pm.convertFromImage(img);
        newsc->addPixmap(pm);
        tmphs->lbl_filedims->setText(QString("%1 x %2").arg(origW).arg(origH));

        // live preview
        glWindow->setHotspot(img);
    } else {
        tmphs->lbl_filedims->setText("<font color=red><b>Image file invalid/unspecified</b></font>");
    }
    tmphs->graphicsView->setTransform(QTransform());
    tmphs->graphicsView->setScene(newsc);
    if (oldsc) delete oldsc;
    QSize sz = tmphs->graphicsView->viewport()->size();
    QRectF r = newsc->sceneRect();
    qreal sx = r.width()/float(sz.width()), sy = r.height()/float(sz.height());
    if (sx > 0.f && sy > 0.f ) {
        if ( sx > 1.1 || sy > 1.1 || sx < 0.9 || sy < 0.9) {
            tmphs->graphicsView->scale(1./sx - 0.01,1./sy - 0.01);
        }
    }
}

void StimApp::loadHSClicked()
{
    if (!tmphs) return;

    QFileInfo fi(tmphs->lbl_filename->text());
    QString dir;
    if (fi.dir().exists()) dir = fi.dir().canonicalPath();
    QString fn = QFileDialog::getOpenFileName(glWindow, "Load Hotspot Image", dir);
    if (fn.length()) {
        tmphs->lbl_filename->setText(fn);
        gotNewHSFile();
    }
}

void StimApp::hotspotAdjSlot()
{
    if (!tmphs) return;
    GlobalDefaults::HSAdjust adj;
    adj.xrot = tmphs->dsb_xrot->value();
    adj.yrot = tmphs->dsb_yrot->value();
    adj.zrot = tmphs->dsb_zrot->value();
    adj.zoom = tmphs->dsb_zoom->value()/100.0;
    adj.xtrans = tmphs->sb_xtrans->value();
    adj.ytrans = tmphs->sb_ytrans->value();
    globalDefaults.hsAdj = adj;
    gotNewHSFile();
}

void StimApp::hotspotAdjResetSlot()
{
    if (!tmphs) return;
    tmphs->dsb_xrot->setValue(0.);
    tmphs->dsb_yrot->setValue(0.);
    tmphs->dsb_zrot->setValue(0.);
    tmphs->dsb_zoom->setValue(100.);
    tmphs->sb_xtrans->setValue(0);
    tmphs->sb_ytrans->setValue(0);
    hotspotAdjSlot();
}

/*static*/ QImage StimApp::GetHotspotImageXFormed(const QString & fn, const GlobalDefaults::HSAdjust & adj, const QSize & destSz)
{
  QImage img(fn);
  if (img.isNull()) {
      // invalid image.. so return a null image which is fine for glWindow class to basically turn hotspots off
      return img;
  }
  QImage destImg(destSz.width(),destSz.height(),QImage::Format_ARGB32);
  QTransform xf;
  QPainter p(&destImg);

  p.setPen(QColor(Qt::white));
  p.setBrush(QBrush(QColor(Qt::white)));
  p.fillRect(QRect(0,0,destSz.width(),destSz.height()),p.brush());
  xf.translate(destImg.width()/2.0,destImg.height()/2.0);
  xf.rotate(adj.zrot, Qt::ZAxis);
  xf.rotate(adj.yrot, Qt::YAxis);
  xf.rotate(adj.xrot, Qt::XAxis);
  xf.translate(-destImg.width()/2.0,-destImg.height()/2.0);
  p.setTransform(xf);
  if (!eqf(adj.zoom,1.0)) img = img.scaled(img.width()*adj.zoom, img.height()*adj.zoom, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  QRect drect = QRect(0,0,destImg.width(), destImg.height()), srect(-adj.xtrans,adj.ytrans,drect.width(),drect.height());
  p.setClipRegion(QRegion(0,0,destImg.width(),destImg.height()));
  p.drawImage(drect,img,srect);
  p.end();
  return destImg;
}

void StimApp::gotNewWarpingFile()
{
    if (!tmpwc) return;

    QImage img = parseWarpingFile(tmpwc->lbl_filename->text(), glWinSize);
    if (img.isNull()) {
        tmpwc->lbl_filedims->setText("<font color=red><b>File invalid/unspecified</b></font>");
    } else {
        tmpwc->lbl_filedims->setText(QString("%1 x %2").arg(img.width()).arg(img.height()));
    }
    glWindow->setWarp(img);
}

void StimApp::loadWarpClicked()
{
    if (!tmpwc) return;

    QFileInfo fi(tmpwc->lbl_filename->text());
    QString dir;
    if (fi.dir().exists()) dir = fi.dir().canonicalPath();
    QString fn = QFileDialog::getOpenFileName(glWindow, "Load Warping Config", dir);
    if (fn.length()) {
        tmpwc->lbl_filename->setText(fn);
        gotNewWarpingFile();
    }
}

void StimApp::configureWarpingDialog()
{
    QDialog dlg(consoleWindow);
    dlg.setWindowIcon(consoleWindow->windowIcon());
    dlg.setModal(true);

    GlobalDefaults & g(globalDefaults);
    Ui::WarpingConfig w;
    w.setupUi(&dlg);

    tmpwc = &w;

    QString saved_warpingfile = g.warpingFile;
    QImage saved_warp = glWindow->warp();

    w.lbl_filename->setText(g.warpingFile.length() ? g.warpingFile : "NONE SPECIFIED");
    w.lbl_windims->setText(QString("%1 x %2").arg(glWinSize.width()).arg(glWinSize.height()));
    w.lbl_filedims->setText("");

    gotNewWarpingFile();

    Connect(w.but_load, SIGNAL(clicked()), this, SLOT(loadWarpClicked()));

    StimPlugin *p = glWindow->runningPlugin(), *dummy = 0;
    StimParams orig_params;

    if (!p) {
        // run this dummy plugin so we can get live previews of the hotspot image...
        p = glWindow->pluginFind("DummyPlugin");
        if (p) {
            StimParams prm = (orig_params=p->getParams()); prm["grid_w"] = 10; p->setParams(prm);
            (dummy=p)->start(true);
        }
    }


    if ( dlg.exec() == QDialog::Accepted ) {
        g.warpingFile = w.lbl_filename->text();
    } else {
        g.warpingFile = saved_warpingfile;
        glWindow->setWarp(saved_warp);
    }

    if (dummy && glWindow->runningPlugin() == dummy) dummy->stop(), dummy->setParams(orig_params);

    tmpwc = 0;
}

/* static */ QImage StimApp::parseWarpingFile(const QString & fname, const QSize & destSize)
{
    QImage ret;
    QFile f(fname);
    QSize dims;
    const int col_width = destSize.width();
    if (destSize.width() > 0 && destSize.height() > 0 && f.exists() && f.open(QFile::ReadOnly)) {
        QVector<qreal> nums;  nums.reserve(col_width*2000);
        QTextStream ts(&f);

        while (ts.status() == QTextStream::Ok && !ts.atEnd()) {
            qreal n;
            ts >> n;
            if (ts.status() == QTextStream::Ok) nums.push_back(n);
            else break;
        }
        //Debug() << "read: " << nums.length() << " numbers from file...";
        if (nums.size() >= col_width*2) {
            dims.setWidth(col_width);
            dims.setHeight((nums.size()/2)/col_width);
            if (dims != destSize) {
                Warning() << "Warping config file size != gl window dimensions!";
            }
            ret = QImage(dims, QImage::Format_ARGB32);
            QVector<qreal>::iterator it = nums.begin();
            for (int r = 0; r < dims.height() && it != nums.end(); ++r) {
                QRgb *line = reinterpret_cast<QRgb *>(ret.scanLine(r));
                for (int c = 0; c < dims.width() && it != nums.end(); ++c) {
                    qreal x = *it++, y = *it++;
                    // scale values to 0->1.0 (sort of an s,t type of param), then fixed-point them onto 16-bit (0->65535)
                    x = (x / qreal(destSize.width())) * 65535.0;
                    y = (y / qreal(destSize.height())) * 65535.0;
                    if (x < 0.) x = 0.; if (x > 65535.) x = 65535.;
                    if (y < 0.) y = 0.; if (y > 65535.) y = 65535.;
                    unsigned int xx = qRound(x), yy = qRound(y);
                    line[c] = QRgb( QRgb((xx<<16)&0xffff0000) | QRgb(yy&0x0000ffff) );
                    /*{ // debug
                        if (c != qRound((double(xx)/65535.0)*destSize.width()) || r != qRound((double(yy)/65535.0)*destSize.height()))
                            Debug() << "x,y=" << c << "," << r << " -> " << xx << "," << yy << " -> " << qRound((double(xx)/65535.0)*destSize.width()) << "," << qRound((double(yy)/65535.0)*destSize.height());

                    }*/
                }
            }
        }
    }
    if (ret.isNull() && f.exists()) Warning() << "Parse error reading warping config file: " << fname;
    return ret;
}
