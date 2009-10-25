#include "StimPlugin.h"
#include "StimApp.h"
#include "GLWindow.h"
#include <QMessageBox>
#include <QDir>
#include <QGLContext>
#include "StimGL_LeoDAQGL_Integration.h"

StimPlugin::StimPlugin(const QString &name)
    : QObject(StimApp::instance()->glWin()), parent(StimApp::instance()->glWin()), gasGen(1, RNG::Gasdev), ran0Gen(1, RNG::Ran0)
{
    needNotifyStart = true;
    frameNum = 0x7fffffff;
    setObjectName(name);
    parent->pluginCreated(this);    
}

StimPlugin::~StimPlugin() 
{
    if (parent->runningPlugin() == this)  stop();
    StimApp::instance()->processEvents();
    parent->pluginDeleted(this);   
}

bool StimPlugin::init() { /* default impl. does nothing */  return true; }
void StimPlugin::cleanup() { /* default impl. does nothing */ }

bool StimPlugin::saveData(bool use_gui)
{
    if (!openOutputFile(use_gui)) return false;
    save(); // plugin-specific save
    writeGeneralInfo();
    Log() << "Saved data for " << name() << " to `" << outFile.fileName() << "'";
    closeOutputFile();    
    return true;
}

void StimPlugin::stop(bool doSave, bool useGui)
{
    endtime = QDateTime::currentDateTime();
    if (doSave) {
        saveData(useGui);
    }

    // Notify LeoDAQGL via a socket.. if possible..
    // Notify LeoDAQGL via a socket.. if possible..
    if (stimApp()->leoDAQGLNotifyParams.enabled) {
        notifySpikeGLAboutStop();
    }
    
    parent->pluginStopped(this);
    emit stopped();
    parent->makeCurrent();
    glClearColor(0.5, 0.5,  0.5, 1.);
    glColor4f(0.5, 0.5,  0.5, 1.);
    glClear( GL_COLOR_BUFFER_BIT );    
    cleanup();
    // restore original matrices from matrix stack
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    frameNum = 0;
}

void StimPlugin::start(bool startUnpaused)
{
    parent->pluginStarted(this);

    // Notify LeoDAQGL via a socket.. if possible..
    needNotifyStart = false;
    if (stimApp()->leoDAQGLNotifyParams.enabled) {
        if (startUnpaused) {
            notifySpikeGLAboutStart();
        } else {
            needNotifyStart = true;
        }
    }

    emit started();
    
    parent->makeCurrent();

    // start out with identity matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    frameNum = 0;
    fps = fpsAvg = 0;
    fpsMin = 9e9;
    fpsMax = 0;
    begintime = QDateTime::currentDateTime();
    endtime = QDateTime(); // set to null datetime
    missedFrames.clear();
    missedFrameTimes.clear();
    if (!missedFrames.capacity()) missedFrames.reserve(4096);
    if (!missedFrameTimes.capacity()) missedFrameTimes.reserve(4096);
    customStatusBarString = "";
    if ( !startUnpaused ) parent->pauseUnpause();
    if (!init()) { 
        stop(); 
        Error() << "Plugin " << name() << " failed to initialize."; 
        return; 
    }
}

unsigned StimPlugin::width() const
{
    return parent->width();
}

unsigned StimPlugin::height() const
{
    return parent->height();
}

void StimPlugin::computeFPS()
{
    double tDiff = (parent->tThisFrame - parent->tLastLastFrame)/2.; 
    if (tDiff > 0.0 && parent->tLastLastFrame > 0.) {
        fps = 1.0/tDiff;
    } else
        fps = 0.;
    static const unsigned minFrameNum = 20;
    static const unsigned nFramesAvg = 60;
    double nAvg = double(MIN(nFramesAvg, (frameNum-minFrameNum)));
    if (frameNum >= minFrameNum) {
        fpsAvg = fpsAvg * nAvg + fps;
        fpsAvg /= nAvg+1.;
        if (fpsMax < fps) fpsMax = fps;
        if (fpsMin > fps) fpsMin = fps;
    }
}

void StimPlugin::afterVSync(bool b) { (void)b; /* nothing.. */ }

bool StimPlugin::openOutputFile(bool use_gui)
{
    closeOutputFile();

    QString date = QDateTime(QDateTime::currentDateTime()).toString("yyyyMMdd");
    QDir dir;
    // make sure output directory is valid, if not keep asking the used
    // for a dir since this save operation is supposed to always succeed
    forever {
        dir.setPath(stimApp()->outputDirectory() + "/" + date);
        if (!dir.exists() && !dir.mkpath(dir.path())) {
            if (use_gui) {
                QMessageBox::critical(0, "Output directory doesn't exist",
                                      "Output directory doesn't exist!\nSpecify a valid directory");
                stimApp()->pickOutputDir();
                continue;
            } else return false;
        }
        break;
    }
   
    // try to find an unused filename by incrementing the index .. 
    outFile.setFileName("");
    for (int i = 1; !outFile.fileName().length() || outFile.exists(); ++i) {        
        QString fname = dir.path() + "/" + name() + "_" + QString::number(i) + ".log";
        outFile.setFileName(fname);
    }   
    // finally open the file
    if (!outFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        if (use_gui)
            QMessageBox::critical(0, "Error opening file",
                                  QString("Cannot open `") + outFile.fileName() + "' for writing.\nStim. log data will not be saved!\n");
        return false;
    }
    outStream.setDevice(&outFile);
    outStream.reset();
    return true;
}

void StimPlugin::writeGeneralInfo()
{
    outStream << "Stimulus: " << name() << " run by " << VERSION_STR << "\n"
              << "Run on host: " << getHostName() << "\n"
              << "Start: " << begintime.toString() << "\n"
              << "End: " << endtime.toString() << "\n" 
              << "Total number of frames: " << frameNum << "\n" 
              << "FPS Average: " << fpsAvg << "\n";
    outStream << "Number of doubled/skipped frames: " << missedFrames.size() << "\n";
    for (unsigned i = 0; i < missedFrames.size() && i < missedFrameTimes.size(); ++i )
        outStream << missedFrames[i] << "  " << missedFrameTimes[i] << "\n";    
}

void StimPlugin::putMissedFrame(unsigned cycleTimeMsecs)
{
    if (missedFrames.capacity() == missedFrames.size())
        missedFrames.reserve(missedFrames.size()*2); // make it 2x bigger
    if (missedFrameTimes.capacity() == missedFrameTimes.size())
        missedFrameTimes.reserve(missedFrameTimes.size()*2); // make it 2x bigger
    missedFrames.push_back(frameNum);
    missedFrameTimes.push_back(cycleTimeMsecs);
}

QByteArray StimPlugin::getFrameDump(unsigned num, GLenum datatype)
{
    unsigned long datasize = width()*height()*3; // R, G, B broken out per pix.
    switch (datatype) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE: 
        datasize *= sizeof(GLubyte); break;
    case GL_FLOAT:
        datasize *= sizeof(GLfloat); break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
        datasize *= sizeof(GLshort); break;
    case GL_INT:
    case GL_UNSIGNED_INT:
        datasize *= sizeof(GLuint); break;
    default:
        Error() << "Unsupported datatype `" << datatype << "' in StimPlugin::getFrameNum!";
        return QByteArray();
    }
    QByteArray ret(datasize, 0);
    if (parent->runningPlugin() != this) {
        Warning() << name() << " wasn't the currently-running plugin, stopping current and restarting with `" << name() << "'";        
        parent->runningPlugin()->stop();
        start(false);
    } else if (num < frameNum) {
        Warning() << "Got non-increasing read of frame # " << num << ", restarting plugin and fast-forwarding to frame # " << num << " (this is slower than a sequential read).";
        stop();
        start(false);
    } else if (!parent->isPaused()) {
        Warning() << "StimPlugin::getFrameNum() called with a non-paused parent!  This is not really supported!  FIXME!";
    }
    
    if (QGLContext::currentContext() != parent->context())
        parent->makeCurrent();
    double tFrame = 1./getHWRefreshRate();
    do  {
        double t0 = getTime();
        cycleTimeLeft = tFrame;
        drawFrame();
        if (frameNum == num) {
            GLint bufwas;
            glGetIntegerv(GL_READ_BUFFER, &bufwas);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, width(), height(), GL_RGB, datatype, ret.data());
            glReadBuffer(bufwas);
        }
        ++frameNum;
        cycleTimeLeft -= getTime()-t0;
        afterVSync(true);
    } while (frameNum <= num);
    glClear(GL_COLOR_BUFFER_BIT);
    return ret;
}

void StimPlugin::notifySpikeGLAboutStart()
{
    StimApp::LeoDAQGLNotifyParams & p(stimApp()->leoDAQGLNotifyParams);
    
    StimGL_LeoDAQGL_Integration::Notify_PluginStart(name(), getParams(),
                                                    0,
                                                    p.hostname,
                                                    p.port,
                                                    p.timeout_ms);
    needNotifyStart = false;
}

void StimPlugin::notifySpikeGLAboutStop()
{
        StimApp::LeoDAQGLNotifyParams & p(stimApp()->leoDAQGLNotifyParams);

        StimGL_LeoDAQGL_Integration::Notify_PluginEnd(name(), getParams(),
                                                        0,
                                                        p.hostname,
                                                        p.port,
                                                        p.timeout_ms);
        needNotifyStart = true;
}
