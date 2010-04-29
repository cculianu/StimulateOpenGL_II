#ifndef CalibPlugin_H
#define CalibPlugin_H
#include "StimPlugin.h"

/** \brief The Refresh Rate Calibration plugin.  Calibrates screen refresh rate.  Used on Linux only.
    
    This plugin is run by the application on startup to calibrate (determine)
    the screen refresh rate.  On Windows it is not run becasue Windows 
    is able to accurately report refresh rate.

    Refresh rate is determined by simply drawing trivial stuff to the screen
    (in this case, a big rectangle) and measuring the amount of 
    time in between frames.  The assumption is that VSync mode is enabled 
    (which is the case in this application).

    Once this plugin finishes executing (after 240 frames) it calls stop()
    on itself and then emits the refreshRateCalibrated() signal which
    is caught by the StimApp instance.
*/

class CalibPlugin : public StimPlugin
{
    Q_OBJECT

    friend class GLWindow;
    CalibPlugin(); ///< c'tor -- it's QObject parent is automatically stimApp()->glWin()

public:
    
    QString description() const { return "Calibrates the refresh rate by drawing frames on the screen as fast as possible with vsync enabled."; }

    /// Reimplemented from super to suppress SpikeGL notify
    bool start(bool startUnpaused = false);
    /// Reimplemented from super to suppress SpikeGL notify
    void stop(bool doSave = false, bool use_gui = false);

protected:
    /// Draws a random background color to the screen.
    void drawFrame();
    /// Reimplemented from super.
    bool init();
    /// Reimplemented from super.
    void cleanup();
signals:
    /// Emitted once the plugin has finished calibrating refresh rate.  StimApp::caribratedRefresh() is normally connected to this signal (Linux only).
     void refreshRateCalibrated(unsigned);
private:
     bool wasDebugMode;
};

#endif
