#ifndef StimPlugin_H
#define StimPlugin_H
#include <QObject>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#  include <glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

// --- NB: these headers are included here so that plugin writers don't 
//         have to worry
#include <math.h>
#include "Util.h"
#include "StimApp.h"
#include "GLWindow.h"
#include "ConsoleWindow.h"
#include "RNG.h"
#include "Version.h"
#include "StimParams.h"
#include <QString>
#include <QDateTime>
#include <vector>
#include <QFile>
#include <QTextStream>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QList>
#include "FrameVariables.h"

enum FPS_Mode {
	FPS_Single = 0, FPS_Dual, FPS_Triple, FPS_Quad = FPS_Triple,
	FPS_N_Mode
};

/**
   \brief Abstract class  -- inherit from this class to create a plugin.  

   See MovingObjects, MovingGrating, and CheckerFlicker for some pre-existing plugins.

   Once you inherit from this class, you need to implement at least the the 
   drawFrame() method to generate frames.  Your method is called for you from
   the GLWindow::paintGL() fuction, once per vblank while your plugin is running.
   
   You may put any setup code you wish to execute on plugin startup in
   your reimplementation of the init() method.

   Any cleanup code should go in your reimplementation of the cleanup() 
   method.

   Access your plugin's configuration parameters with getParam().

   Note that in order for plugins you write to appear in the main application,
   they need to be also created (new'd) inside GLWindow::initPlugin().
*/
class StimPlugin : public QObject
{
    Q_OBJECT

    friend class GLWindow;

protected:
    StimPlugin(const QString & name); ///< c'tor -- it's QObject parent is automatically StimApp::instance() and the name should be passed to this c'tor from child classes

public:
    virtual ~StimPlugin(); 

    /// iff 0, this plugin runs as fast as possible (up to monitor refresh rate)
    virtual unsigned preferredFPS() const { return 0; }

    /// returns width in pixels of the opengl 2d area
    unsigned width() const;
    /// returns height in pixels of the opengl 2d area
    unsigned height() const;

    /// start the plugin -- marks the plugin as 'running' with the GLWindow and calls init(), then emits 'started' signal.  If reimplementing, call super.
    virtual bool start(bool startUnpaused = false);
    /// stop the plugin -- marks the plugin as 'not running' with the GLWindow, calls cleanup(), then emits 'stopped' signal.  If reimplementig, call super
    virtual void stop(bool doSave = false, bool use_gui = false, bool dontDoGLClearOnStop = false);

    /// just calls QObject::objectName()
    QString name() const { return objectName(); }
    /// returns a string description of the plugin useful for a UI.  Reimplement to add a custom description of your plugin.
    virtual QString description() const { return "A Stim Plugin."; }

    /// Set the plugin's configuration parameters.  ConnectonThread calls this method for example when the client wants to define experiment parameters for a plugin.
    void setParams(const StimParams & p) { QMutexLocker l(&mut); params = p; }
    /// We return an implicitly shared copy of the parameters.  Due to multithreading concerns, implicit sharing is a good thing!
    StimParams getParams() const { QMutexLocker l(&mut); return params; }

    /// templatized function for reading parameters
    template <typename T> bool getParam(const QString & name, T & out) const;
	/// current param suffix context -- defaults to ""
	QString paramSuffix() const;

	/// use this function to implicitly add suffix to all param names requested with getParam.  Default stack is empty.
	void paramSuffixPush(const QString & suffix);
	/// use this function to undo the state of a previous call to paramSuffixPush()
	void paramSuffixPop();
	
    /// Returns the average FPS for this plugin over the last 1-2 seconds
    double getFps() const { return fpsAvg; }
    /// Returns the latest FPS for this plugin (over the last 2 frames)    
    double getFpsCur() const { return fps; }
    /// Returns the smallest FPS value encountered since plugin start()
    double getFpsMin() const { return fpsMin; }
    /// Returns the largest FPS value encounteres since plugin start()
    double getFpsMax() const { return fpsMax; }
    /// Returns the frame number of the next frame to be rendered
    unsigned getNextFrameNum() const { return frameNum; }
    /// Returns the frame number of the last frame that was rendered (or -1 if no frame has ever been rendered)
    int getFrameNum() const { return static_cast<int>(frameNum)-1; }
    /// Returns the status bar string that this plugin would like the UI to display
    const QString & getSBString() const { return customStatusBarString; }
    /// Returns the number of missed frames that the plugin has encountered thus far
    unsigned getNumMissedFrames() const { return missedFrames.size(); }

	/// \brief Inform calling code if this plugin is initializing or not
	/// If true, the plugin is ready, if false, need to wait
	bool isInitialized() const { return initted; }

    /// Returns the time that start() was called for this plugin
    QDateTime getBeginTime() const { return begintime; }
    
    /// \brief Save pluign date to a file.
    ///
    /// Save data -- called for you on if plugin stop(true).
    /// Returns false on error (usually failure to open output files).
    /// Use gui parameter controls whether to use the GUI for reporting/
    /// recovering from errors (or to just silently return false on error).
    bool saveData(bool use_gui_for_errors = false);

	/// \brief Grab arbitrary frame data by number from a plugin.
    /// 
    //  Can replay an experiment -- arbitrarily reproduces any frames
    /// from an experiment.  Note that it's best to call this with
    /// num increasing each time (that is, sequentially) as otherwise
    /// you may experience long delays while the plugin recomputes all
    /// frames up to num.
	///
	/// Optionally, you may retrieve a sub-rectangle of the plugin's window.
	/// To do this, specify the rectOrigin and rectSize parameters.
	///
	/// In addition, the returned data may be down-sampled to every i,j'th pixel
	/// by specifying the downsample_pix_factor vector.  This downsampling is applied
	/// last (thus the rectOrigin and rectSize parameters should be in pre-downsampled-coordinates).
    /// 
    /// @param num the frame number to generate/dump
	/// @param numframs the number of frames to retrieve
	/// @param rectOrigin The origin of the sub-rectangle of the window area to dump (basically, the crop origin).  0,0 is at bottom left corner of window, and is the default.
	/// @param rectSize The size of the sub-rectangle of the window area to dump (basically the crop size).  The rectSize should not exceed the size of the plugin window.  Default is the entire area of the window starting at the cropOrigin.
	/// @param downsample_pix_factor The default, 1,1, produces a pixel-by-pixel copy of the window area.  To downsample the returned pixels to every i,j'th pixel (for example, becuse it is the CheckerFlicker plugin and you have stixel sizes >1) specify a vector with component values >= 1.  Note that downsampling is applied last after cropping.
    /// @param data_type the OpenGL data type of the generated data.  Note that the default is good for most users so no need to change it unless you know what you are doing.
	/// @return a list of the frames.  Note that a short frame count may be returned on error, out of memory conditions, etc.
    QList<QByteArray> getFrameDump(unsigned num, unsigned numframes = 1, 
								   const Vec2i & rectOrigin = Vec2iZero, 
								   const Vec2i & rectSize = Vec2iZero,
								   const Vec2i & downsample_pix_factor = Vec2iUnit, /* unit vector */
								   GLenum data_type = GL_UNSIGNED_BYTE);
	
	/// Frame Variables -- use this object in your pushFrameVars() method!
	FrameVariables *frameVars;
	bool have_fv_input_file; ///< defaults to false, true indicates plugin is using a frameVars input file
	
	/// Debugging feature: 
	/// Opens a file in the output dir called PLUGIN_NAME_DebugFrame_X.png (where X is frameNum) which contains the entire contents
	/// of the backbuffer
	void logBackbufferToDisk() const;

signals:
    void started(); ///< emitted when plugin starts
    void finished(); ///< emitted when plugin finishes naturally
    void stopped(); ///< emitted whenever plugin stops
    
protected: 
    /** Process keypresses.  Reimplement this in your plugin to process keys:
        Return true if key processed (accepted) or false otherwise.
        NB: It's important to return false if you want to ignore the key
        so other widgets can receive it. If you return true no other widgets
        are notified of the keypress.  */
    virtual bool processKey(int key) { (void) key; return false; }

    /** \brief Called as a result of start() when plugin is started. 

        You should reimplement this in your plugin to add initialization code 
        for class variables, etc.  It's important to properly initialize 
        all variables that affect your drawFrame() function so that 
        subsequent runs of the same plugin are identical and not 
        polluted by crufty old state variables.
        Default implementation does nothing. 
        Return false if you want to indicate init failure for some reason. */
    virtual bool init();
    /** \brief Called as a result of stop() when plugin is stopped. 

        Reimplement to clean up any permanent state that you fear might
        interfere with other plugins.  In general this would only be 
        the OpenGL state that needs to be undone (glEnable/glDisable, etc). 
        Default implementation does nothing. */
    virtual void cleanup();

    /// The meat of every plugin -- re-implement this function to draw the actual frame.  Class variable frameNum holds the frame number.
    virtual void drawFrame() = 0;

	/// Normally no need to re-implement.  Just draws the frame track box for the PD sensor. Only drawn if the box has dimensions (see ftrackbox_[xyw])
	virtual void drawFTBox();

	/// Convenience method that just calls drawFrame() and drawFTBox() for you, in that order
	inline void renderFrame() { drawFrame(); drawFTBox(); }

    /// \brief Called immediately after a vsync (if not paused).  
    ///
    /// Reimplement this in your plugin to do some work  after the vsync, 
    /// which may be a good time to do slower computations in preparation
    /// for the next frame, etc, since we have the maximal amount of time 
    /// at this point before the next frame is to be drawn.
    /// If isSimulated = true this means we are in getFrameNum() call
    /// and not actually drawing to screen. 
    virtual void afterVSync(bool isSimulated = false);

	/// \brief Called during plugin start to ask the plugin how much of a delay it needs before it is considered initialized.
	///
	/// Reimplent this in your plugin if you wish to introduce a pre-delay before a plugin is considered 'initialized'
	/// CheckerFlicker makes use of this because it is buggy if run right after init (frame doubling bug in A mode).
	/// Return the delay you wish for your plugin, in milliseconds.  Default implementation returns 0.
	virtual unsigned initDelay(void);

    GLWindow *parent;

    volatile bool initted;
    unsigned frameNum; ///< starts at -1, 0 is first frame, gets incremented for each frame (each time drawFrame() is called)
	unsigned nFrames, /**< the number of frames per loop cycle.  
					       If nFrames == 0, infinite number of frames (no loop cycle)
	                       If nFrames >0, then we display nFrames frames, then we loop that nLoops times.  Each
						   loops will assert the FT_End frametrack flag (each loop begin will assert the 
					       FT_Start frametrack flag). */
	         nLoops, ///< the number of times we do the nFrames above.  0 is infinite, 1 means once, etc
	         loopCt; ///< counter for above
	
    double fps, fpsAvg, fpsMin, fpsMax;
    double cycleTimeLeft; ///< the number of seconds left in this cycle -- updated by glWindow before calling afterVSync
    bool needNotifyStart; ///< iff true, we will notify LeoDAQGL of plugin start on unpause
	int ftrackbox_x, ftrackbox_y, ftrackbox_w;
	FPS_Mode fps_mode; ///< one of FPS_Single, FPS_Dual, FPS_Triple, FPS_Quad (this currently means triple!)
	float bgcolor;
	char color_order[3]; ///< "bgr" or "rgb" or "brg" or "gbr" or "grb" or "rbg"  -- defaults to "rgb" if not specified.
	
	int b_index, r_index, g_index; ///< index of brg values in above color_order param.  

	enum FTState {
		// NB: if changing order of these or number of these please update the 
		//     'ftColorParamNames' local variable in the StimPlugin::start() 
		//     function in StimPlugin.cpp!
		FT_Track = 0,
		FT_Off,
		FT_Change,
		FT_Start,
		FT_End,
		N_FTStates
	};
	
	Vec3 ftStateColors[N_FTStates];
	FTState currentFTState;
	bool ftAssertions[N_FTStates]; ///< child plugins assert these flags for a particular frame to override default off/on behavior. These flags only last for 1 frame.
	int ftChangeEvery; ///< if > -1, auto-assert FT_Change when (frameNum % ftChangeEvery) == 0
	
	/// handle FTState transitions.. called right before drawing the frame track box.  Takes into account ftAssertions
	virtual void advanceFTState(); 
	
	QList<QString> paramSuffixStack; ///< used in conjunction with getParam, setParam to add suffixes to the params passed to getParam()..
	
    void notifySpikeGLAboutStart();
    void notifySpikeGLAboutStop();

    /// plugin-level mutex -- used in a couple of functions and you may reuse it yourself as well
    mutable QMutex mut; 

    /// a list of skipped/missed frames -- the id number is in relative frame #
    /// or frameNum (as opposed to abs frame # which is what getHWFrameCount() 
    /// would return)
    std::vector<unsigned> missedFrames;
    /// a list of cycle times for skipped/missed frames, in msecs
    std::vector<unsigned> missedFrameTimes;

    /// \brief Generic random number generator provided here as a convenience.
    ///
    /// This generator is of type RNG::Ran1 and as such is similar to the
    /// ran1() function specified in Numerical Recipes.
    RNG ran1Gen, 

    /// \brief Generic gaussian random number generator provided here as a convenience.
    ///
    /// This generator is of type RNG::Gasdev and as such is similar to the
    /// gasdev() function specified in Numerical Recipes.
        gasGen, 

    /// \brief Generic random number generator provided here as a convenience.
    ///
    /// This generator is of type RNG::Ran0 and as such is similar to the
    /// ran0() function specified in Numerical Recipes.
        ran0Gen;

    /// \brief Write to this variable to optionally inform the application that you want a custom message printed to the status bar.
    ///
    /// Update this periodically to have the ConsoleWindow status bar include additional
    /// plugin-specific information -- may or may not appear right away
    /// typically the status bar is refreshed every 250ms by StimApp.cpp
    QString customStatusBarString;

    /// the output stream you should use in your "save()" method reimplementation -- already opened for you if your save() method is being called
    QTextStream outStream;
    /// Write to this file when saving data.
    QFile outFile;
   
    virtual void save() {} ///< default implementation does nothing, reimplement -- called for you on plugin exit if user elected to save data.  In this method you should write to the StimPlugin::outStream member.

    /// used to indicate when start() and stop() were called for this plugin, 
    /// used by writeGeneralInfo() as an FYI for the log data file
    QDateTime begintime, endtime;

    /// called by GLWindow when a frameskip is detected..
    void putMissedFrame(unsigned cycleTimeMsecs);

private:
    StimParams params;
    void computeFPS();

    /// called by framework right before save() method
    bool openOutputFile(bool use_gui_for_errors = false);
    /// called by framework right after save() method
    void closeOutputFile() { outStream.flush(); outFile.close(); }
    /// Called by application to write general info to an already-open output 
    /// file.  This is called for you immediately after your save() method, 
    /// so don't call it yourself (not that you can anyway, it's private).
    void writeGeneralInfo();

	/// static helper function for getFrameDump(). Low-level function to just dump the backbuffer to a bytearray given
	/// an origin and a cropregion.
	static bool readBackBuffer(QByteArray & dest, const Vec2i & cropOrigin, const Vec2i & cropRegionSize, GLenum datatype);
	
private slots:
	/// Sets initted = true, calls LeoDAQGL notify
	void initDone();

};

namespace {
        /// A helper class for parsing parameters
        struct MyTxtStream : public QTextStream
        {
            MyTxtStream(QString *s) : QTextStream(s, QIODevice::ReadOnly), str(*s) {}
            const QString & str;
        };
        template <typename T> MyTxtStream & operator>>(MyTxtStream &s, T & t)
            {
                static_cast<QTextStream &>(s) >> t;
                return s; 
            }
        template <> MyTxtStream & operator>>(MyTxtStream &s, bool & b) {
            if (!QString::compare(s.str.trimmed(), "true", Qt::CaseInsensitive)) b = true;
            else if (!QString::compare(s.str.trimmed(), "false", Qt::CaseInsensitive)) b = false;
            else {
                int i;
                static_cast<QTextStream &>(s) >> i;
                b = i;
            }
            return s;
        }
}


// specialization for strings
template <> bool StimPlugin::getParam<QString>(const QString & name, QString & out) const;

// templatized functions for reading parameters
template <typename T> 
bool StimPlugin::getParam(const QString & name, T & out) const
{        
        QString suffix = paramSuffix();
        QMutexLocker l(&mut);
        StimParams::const_iterator it;
        for (it = params.begin(); it != params.end(); ++it)
            if (QString::compare(it.key(), name + suffix, Qt::CaseInsensitive) == 0)
                break;

        if (it != params.end()) { // found it!
            //if (it->canConvert<T>()) { out = it->value<T>(); return true; }
            //else { // fallback to ghetto method
                QString s(it->toString());
                MyTxtStream txt(&s);
                T tmp;
                if ( (txt >> tmp).status() == 0 ) {
                    out = tmp;
                    return true;
                }
                //}
        }
        return false;        
}

#endif
#include "GridPlugin.h"
