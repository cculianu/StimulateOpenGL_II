#include "ConnectionThread.h"
#include "Util.h"
#include "StimApp.h"
#include "GLWindow.h"
#include "StimPlugin.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QRegExp>
#include <QTextStream>
#include <QEvent>
#include <QSemaphore>
#include <QVariant>
#include <QByteArray>
#include <QDateTime>
namespace {
    enum EventTypes {
        StartPluginEventType = QEvent::User+32,
        StopPluginEventType,
        GetHWFrameCountEventType,
        GetFrameEventType,
        IsConsoleHiddenEventType,
        ConsoleHideEventType,
        ConsoleUnHideEventType
    };

    struct StartStopPluginEvent : public QEvent
    {
        /// if this c'tor is used, the event type will be a plugin start cmd,
        /// and the named plugin will be started
        StartStopPluginEvent(const QString &plugin, bool startUnpaused)
            : QEvent(static_cast<QEvent::Type>(StartPluginEventType)),  plugin(plugin), flag(startUnpaused)  {}
        
        /// if this c'tor isused, the event type will be a plugin stop cmd,
        /// and whichever plugin is currently running (if any) will be stopped
        StartStopPluginEvent(bool doSave) 
            : QEvent(static_cast<QEvent::Type>(StopPluginEventType)), flag(doSave) {}
        
        QString plugin;
        bool flag;    
    };
    
    struct GetSetData
    {
        QSemaphore replySem, deleteOkSem;
        QVariant req;
        QVariant datum;

        template <typename T> void setReply(const T & t) {
            datum.setValue(t);
            replySem.release();
            deleteOkSem.acquire();
        }
        template <typename T> T getReply() {
            T ret;
            replySem.acquire();
            ret = datum.value<T>();
            deleteOkSem.release();
            return ret;
        }

    };

    struct GetSetEvent : public QEvent
    {
        GetSetEvent(int t) : QEvent(static_cast<QEvent::Type>(t)) {d = new GetSetData; deleteData = false; }
        ~GetSetEvent() { if (deleteData) delete d; d = 0; }

        GetSetData *d;
        bool deleteData;
    };

    struct GetHWFrameCountEvent : public GetSetEvent
    {
        GetHWFrameCountEvent() : GetSetEvent(GetHWFrameCountEventType) {}
    };

    struct GetFrameEvent : public GetSetEvent
    {
        GetFrameEvent(unsigned framenum, int datatype) : GetSetEvent(GetFrameEventType) { d->req.setValue(framenum); d->datum.setValue(datatype); }
    };

    struct IsConsoleHiddenEvent : public GetSetEvent
    {
        IsConsoleHiddenEvent() : GetSetEvent(IsConsoleHiddenEventType) {}
    };

    struct ConsoleHideEvent : public QEvent
    {
        ConsoleHideEvent() : QEvent(static_cast<QEvent::Type>(ConsoleHideEventType)) {}
    };

    struct ConsoleUnHideEvent : public QEvent
    {
        ConsoleUnHideEvent() : QEvent(static_cast<QEvent::Type>(ConsoleUnHideEventType)) {}
    };
}

ConnectionThread::ConnectionThread(int sfd, QObject *parent)
    : QThread(parent), sockdescr(sfd)
{
    installEventFilter(this);
}

void ConnectionThread::run()
{
    Debug() << "Thread started.";
    QTcpSocket sock;
    socketNoNagle(sockdescr);
    if (!sock.setSocketDescriptor(sockdescr)) {
        Error() << sock.errorString();
        return;
    }
    QString remoteHostPort = sock.peerAddress().toString() + ":" + QString::number(sock.peerPort());
    Log() << "Connection from peer " << remoteHostPort;
    QString line;
    forever {
        if (sock.canReadLine() || sock.waitForReadyRead()) {
            line = sock.readLine().trimmed();
            while (StimApp::instance()->busy()) {
                // special case case, stimapp is busy so keep polling with 1s intervals                
                Debug() << "StimApp busy, cannot process command right now.. trying again in 1 second";
                sleep(1); // keep sleeping 1 second until the stimapp is no longer busy .. this keeps us from getting given commands while we are still initializing
            }
            // normal case case, stimapp not busy,  proceed normally
            QString resp;
            resp = processLine(sock, line);
            if (sock.state() != QAbstractSocket::ConnectedState) {
                Debug() << "processLine() closed connection";
                break;
            }
            if (!resp.isNull()) {
                if (resp.length()) {
                    Debug() << "Sending: " << resp;
                    if (!resp.endsWith("\n")) resp += "\n";
                    sock.write(resp.toUtf8());
                }
                Debug() << "Sending: OK";
                sock.write("OK\n");
            } else {
                Debug() << "Sending: ERROR";
                sock.write("ERROR\n");
            }            
        } else {
            if (sock.error() != QAbstractSocket::SocketTimeoutError 
                || sock.state() != QAbstractSocket::ConnectedState) {
                Debug() << "Socket error: " << sock.error() << " Socket state: " << sock.state();
                break;
            }
        }
    }
    Log() << "Connection ended (peer: " << remoteHostPort << ")";
    Debug() << "Thread exiting.";
}

QString ConnectionThread::processLine(QTcpSocket & sock,
                                      const QString & line)
{
    Debug() << "Got Line: " << line;
    QStringList toks = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
    if (toks.size() < 1) return false;
    QString cmd = toks.front();
    toks.pop_front();
    cmd = cmd.toUpper();
    if (cmd == "NOOP") {
        return ""; // noop command alwasy prints OK -- used to test server connectivity and for keepaliveage, etc
    } else if (cmd == "GETTIME") {
        return QString::number(getTime(), 'f', 9);
    } else if (cmd == "GETFRAMENUM") {
        StimPlugin *p = stimApp()->glWin()->runningPlugin();
        if (p) {
            return QString::number(p->getFrameNum());
        } else {
            Error() << "GETFRAMENUM command received but no plugin was running.";
        }
    } else if (cmd == "GETHWFRAMENUM") {
        GetHWFrameCountEvent *e = new GetHWFrameCountEvent();
        GetSetData *d = e->d;
        stimApp()->postEvent(this, e);
        unsigned hwfc = d->getReply<unsigned>();
        delete d;
        return QString::number(hwfc);
    } else if (cmd == "GETREFRESHRATE") {
        unsigned rate = stimApp()->refreshRate();
        return QString::number(rate);
    } else if (cmd == "GETWIDTH") {
        return QString::number(stimApp()->glWin()->width());
    } else if (cmd == "GETHEIGHT") {
        return QString::number(stimApp()->glWin()->height());
    } else if (cmd == "GETFRAME" && toks.size()) {
        bool ok;
        unsigned framenum = toks[0].toUInt(&ok);
        toks.pop_front();
        int datatype = GL_UNSIGNED_BYTE;
        if (toks.size()) {
            QString s = toks.join(" ").toUpper().trimmed();
            if (s == "BYTE") datatype = GL_BYTE;
            else if (s == "UNSIGNED BYTE") datatype = GL_UNSIGNED_BYTE;
            else if (s == "SHORT") datatype = GL_SHORT;
            else if (s == "UNSIGNED SHORT") datatype = GL_UNSIGNED_SHORT;
            else if (s == "INT") datatype = GL_INT;
            else if (s == "UNSIGNED INT") datatype = GL_UNSIGNED_INT;
            else if (s == "FLOAT") datatype = GL_FLOAT;
            else {
                Error() << "GETFRAME command invalid datatype `" << s << "'.";
                return QString::null;
            }
        }
        if (ok) {
            GetFrameEvent *e = new GetFrameEvent(framenum, datatype);
            GetSetData *d = e->d;
            stimApp()->postEvent(this, e);
            QByteArray framedata = d->getReply<QByteArray>();
            delete d;
            if (!framedata.isNull()) {
                sock.write((QString("BINARY DATA ") + QString::number(framedata.size()) + "\n").toUtf8());
                sock.write(framedata);
                return "";
            }
        }
    } else if (cmd == "LIST") {
        QList<QString> lst = stimApp()->glWin()->plugins();
        QString ret;
        QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Truncate);
        for(QList<QString>::const_iterator it = lst.begin(); it != lst.end(); ++it) {
            ts << (*it) << "\n";
        }
        ts.flush();
        return ret;
    } else if (cmd == "GETSTATS") {
        QString theStr("");
        QTextStream strm(&theStr, QIODevice::WriteOnly/*|QIODevice::Text*/);
        GetSetEvent *e = new GetHWFrameCountEvent();
        GetSetData *d = e->d;
        stimApp()->postEvent(this, e);
        unsigned hwfc = d->getReply<unsigned>();
        delete d;
        e = new IsConsoleHiddenEvent();
        d = e->d;
        stimApp()->postEvent(this, e);
        bool isConsoleHidden = d->getReply<bool>();
        delete d;
        StimPlugin *p = stimApp()->glWin()->runningPlugin();

        strm.setRealNumberPrecision(3);
        strm.setRealNumberNotation(QTextStream::FixedNotation);
        strm << "runningPlugin = " << (p ? p->name() : "") << "\n"
             << "isPaused = " << (stimApp()->glWin()->isPaused() ? "1" : "0") << "\n"
             << "isConsoleWindowHidden = " << (isConsoleHidden ? "1" : "0") << "\n"
             << "statusBarString = " << (p ? p->getSBString() : "") << "\n"
             << "currentTime = " << QDateTime::currentDateTime().toString() << "\n"
             << "beginTime = " << (p ? p->getBeginTime().toString() : "") << "\n"
             << "width = " << stimApp()->glWin()->width() << "\n"
             << "height = " << stimApp()->glWin()->height() << "\n"
             << "fpsAvg = " << (p ? p->getFps() : 0.) << "\n"
             << "fpsMin = " << (p ? p->getFpsMin() : 0.) << "\n"
             << "fpsMax = " << (p ? p->getFpsMax() : 0.) << "\n"
             << "fpsLast = " << (p ? p->getFpsCur() : 0.) << "\n"
             << "frameNum = " << (p ? p->getFrameNum() : -1) << "\n"
             << "hardwareFrameCount = " << hwfc << "\n"
             << "haAccurateHWFrameCount = " << (hasAccurateHWFrameCount() ? "1" : "0") << "\n"
             << "calibraredRefreshRate = " << stimApp()->refreshRate() << "\n"
             << "hwRefreshRate = " << getHWRefreshRate() << "\n"
             << "hasAccurateHWRefreshRate = " << (hasAccurateHWRefreshRate() ? 1 : 0) << "\n"
             << "numMissedFrames = " << (p ? p->getNumMissedFrames() : 0) << "\n";
        QDateTime now(QDateTime::currentDateTime()), beginTime(p ? p->getBeginTime() : now);
        double secs = beginTime.secsTo(now), fskipsPerSec = 0.;
        if (p && secs > 0.) {
            fskipsPerSec = p->getNumMissedFrames() / secs;
        }
        strm << "missedFramesPerSec = " << fskipsPerSec << "\n"
             << "saveDirectory = " << stimApp()->outputDirectory() << "\n"
             << "pluginList = ";
        QList<QString> plugins = stimApp()->glWin()->plugins();
        for (QList<QString>::const_iterator it = plugins.begin(); it != plugins.end(); ++it) {
            if (it != plugins.begin()) strm << ", ";
            strm << *it;
        }
        strm << "\n"
             << "programUptime = " << getTime() << "\n"
             << "nProcessors = " << getNProcessors() << "\n"
             << "hostName = " << getHostName() << "\n"
             << "uptime = " << getUpTime() << "\n";
        
        strm.flush();
        return theStr;
    } else if (cmd == "GETPARAMS" && toks.size()) {
        QString pluginName = toks.join(" ");
        StimPlugin *p;
        if ( (p = stimApp()->glWin()->pluginFind(pluginName)) ) {
            return p->getParams().toString();
        }
    } else if (cmd == "SETPARAMS" && toks.size()) {
        QString pluginName = toks.join(" ");
        StimPlugin *p;
        if ( (p = stimApp()->glWin()->pluginFind(pluginName))
             && (stimApp()->glWin()->runningPlugin() != p) ) {
            Debug() << "Sending: READY";
            sock.write("READY\n");
            QString paramstr ("");
            QTextStream paramts(&paramstr, QIODevice::WriteOnly/*|QIODevice::Text*/);
            QString line;
            while ( sock.canReadLine() || sock.waitForReadyRead() ) {
                line = sock.readLine().trimmed();
                if (!line.length()) break;
                Debug() << "Got Line: " << line;
                paramts << line << "\n";
            }
            paramts.flush();
            p->setParams(paramstr);
            return "";
        } else if (p && stimApp()->glWin()->runningPlugin() == p) {
            Error() << "SETPARAMS cannot be called on a plugin that is running";
        } else if (!p) {
            Error() << "SETPARAMS called on a non-existant plugin";
        }
    } else if (cmd == "RUNNING") {
        StimPlugin *p = stimApp()->glWin()->runningPlugin();
        if (p) {
            return p->name();
        }
        return "";
    } else if (cmd == "ISPAUSED") {
        return QString::number(stimApp()->glWin()->isPaused());
    } else if (cmd == "PAUSE") {
        if (!stimApp()->glWin()->isPaused())
            stimApp()->glWin()->pauseUnpause();
        return "";
    } else if (cmd == "UNPAUSE") {
        if (stimApp()->glWin()->isPaused())
            stimApp()->glWin()->pauseUnpause();
        return "";
    } else if (cmd == "ISCONSOLEHIDDEN") {
        GetSetEvent *e = new IsConsoleHiddenEvent;
        GetSetData *d = e->d;
        stimApp()->postEvent(this, e);
        QString ret = QString::number(int(d->getReply<bool>()));
        delete d;
        return ret;
    } else if (cmd == "CONSOLEHIDE") {
        stimApp()->postEvent(this, new ConsoleHideEvent());
        return "";
    } else if (cmd == "CONSOLEUNHIDE") {
        stimApp()->postEvent(this, new ConsoleUnHideEvent());
        return "";
    } else if (cmd == "START" && toks.size()) {
        // this commands needs to be executed in the main thread
        // to avoid race conditions and also to have a valid opengl context
        QString pluginName = toks.front();
        bool startUnpaused = toks.size() > 1 && toks[1].toInt();
        if ( (stimApp()->glWin()->pluginFind(pluginName)) ) {
            stimApp()->postEvent(this, new StartStopPluginEvent(pluginName, startUnpaused));
            return "";
        }
    } else if (cmd == "STOP") {
        // this commands needs to be executed in the main thread
        // to avoid race conditions and also to have a valid opengl context
        bool doSave = toks.join("").toInt();
        if (stimApp()->glWin()->runningPlugin())
            stimApp()->postEvent(this, new StartStopPluginEvent(doSave)); 
        return "";
    } else if (cmd == "GETSAVEDIR") {
        return stimApp()->outputDirectory();
    } else if (cmd == "SETSAVEDIR") {
        QString dir = toks.join(" ");
        return stimApp()->setOutputDirectory(dir) ? QString("") : QString::null;
    } else if (cmd == "GETVERSION") {
        return VERSION_STR;
    } else if (cmd == "BYE") {
        sock.close();
    } 
    // add more cmds here
    return QString::null;
}

/// reimplemented from QObject so that we can post events to main thread
bool ConnectionThread::eventFilter(QObject *watched, QEvent *event)
{
    int type = static_cast<int>(event->type());
    if (watched == this) {
        switch (type) {
        case StartPluginEventType: {
            // Start a plugin received
            StartStopPluginEvent *e = dynamic_cast<StartStopPluginEvent *>(event);
            if (e) {
                StimPlugin *p = stimApp()->glWin()->pluginFind(e->plugin);
                if (p) p->start(e->flag);            
            }
        }
            return true;

        case StopPluginEventType: {
            // Stop a plugin received
            StartStopPluginEvent *e = dynamic_cast<StartStopPluginEvent *>(event);
            if (e) {
                StimPlugin *p = stimApp()->glWin()->runningPlugin();
                if (p) p->stop(e->flag, false);
            }
        }
            return true;

        case GetHWFrameCountEventType: {
            GetSetEvent *e = dynamic_cast<GetSetEvent *>(event);
            if (e) {
                e->d->setReply(getHWFrameCount());
            }
        }
            return true;
        case GetFrameEventType: {
            GetSetEvent *e = dynamic_cast<GetSetEvent *>(event);
            if (e) {
                unsigned num = e->d->req.toUInt();
                int datatype = e->d->datum.toInt();
                StimPlugin *p;
                if ((p=stimApp()->glWin()->runningPlugin()) && stimApp()->glWin()->isPaused()) {
                    e->d->setReply(p->getFrameDump(num, datatype));
                } else
                    e->d->setReply(QByteArray());
            }
        }
            return true;

        case IsConsoleHiddenEventType: {
            GetSetEvent *e = dynamic_cast<GetSetEvent *>(event);
            if (e && stimApp() && stimApp()->console()) {
                e->d->setReply(stimApp()->console()->isHidden());
            }
        }
            return true;

        case ConsoleHideEventType: 
            if (stimApp() && stimApp()->console() && !stimApp()->console()->isHidden())
                stimApp()->console()->hide();
            return true;

        case ConsoleUnHideEventType: 
            if (stimApp() && stimApp()->console() && stimApp()->console()->isHidden())
                stimApp()->console()->show();
            return true;

        } // end switch
    }
    return QThread::eventFilter(watched, event);
}
    
