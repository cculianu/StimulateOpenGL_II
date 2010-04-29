#include "CalibPlugin.h"
#ifdef _MSC_VER
static double round(double d) { return qRound(d); }
#endif

CalibPlugin::CalibPlugin()
    : StimPlugin("CalibPlugin")
{
    Connect(this, SIGNAL(refreshRateCalibrated(unsigned)), StimApp::instance(), SLOT(calibratedRefresh(unsigned)));
}

void CalibPlugin::stop(bool doSave, bool use_gui)
{
    const bool wasEnabled = stimApp()->spikeGLNotifyParams.enabled;
    stimApp()->spikeGLNotifyParams.enabled = false; // suppress for this plugin
    StimPlugin::stop(doSave, use_gui);
    stimApp()->spikeGLNotifyParams.enabled = wasEnabled;
}

bool CalibPlugin::start(bool startUnpaused)
{
    const bool wasEnabled = stimApp()->spikeGLNotifyParams.enabled;
    stimApp()->spikeGLNotifyParams.enabled = false; // suppress for this plugin
    StimPlugin::start(startUnpaused);
    stimApp()->spikeGLNotifyParams.enabled = wasEnabled;
	return true;
}

void CalibPlugin::drawFrame()
{
    // calibration code -- no trivial painting done of num x num random rectangles
    const float meanintensity = 0.5f, variance = 0.05f;
    const int num = 1;

    int w = width(), h = height(), x = 0, y = 0, xincr = w/num, yincr = h/num;
    for (;;) {
        glColor3f( meanintensity + ran1Gen.range(-variance,variance), meanintensity + ran1Gen.range(-variance,variance), meanintensity + ran1Gen.range(-variance,variance) );
        glRecti( x, y, x+xincr, y+yincr );
        if ( (x += xincr) + xincr > w ) x = 0, y += yincr;
        if ( y + yincr > h ) break;
    }
    static const unsigned calibTot = 240;
    Debug() << "Frame " << frameNum << " fpsnow " << fps << " avg " << fpsAvg << " hwframe " << getHWFrameCount();
    if (frameNum >= calibTot) {
        stop();
        emit refreshRateCalibrated(static_cast<unsigned>(::round(fpsAvg)));
        emit finished();
    }
}

bool CalibPlugin::init()
{
#if 0
    if (!StimApp::instance()->busy()) {
        Error() << "Calib plugin can only be started once, at app initialization. Sorry!";
        return false;
    }
#endif
    wasDebugMode = StimApp::instance()->isDebugMode();
    StimApp::instance()->setDebugMode(false);  // force debug mode off to minimize console printing
    StimApp::lockMouseKeyboard();
    Log() << "Calib plugin locked mouse and keyboard to minimize paint events (please wait)";
    return true;
}

void CalibPlugin::cleanup()
{
    StimApp::releaseMouseKeyboard();
    
    StimApp::instance()->setDebugMode(wasDebugMode);  // re-enable debug mode if it was set
    Log() << "Calib plugin released mouse and keyboard";
}
