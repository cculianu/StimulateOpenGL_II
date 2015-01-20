/*! \mainpage StimulateOpenGL II
 *
 * \section intro_sec Introduction
 *
 * This is documentation for the StimulateOpenGL II program.  It is mainly
 * sourcecode documentation.  However, additional documentation exists as 
 * follows: 
 * \arg \subpage install  "Installation Documentation" 
 * 
 * \arg \subpage plugin_params "Plugin Parameter Documentation"
 *
 * \arg \subpage matlab_api "Matlab API Documentation"
 *
 * \page install Installation
 * \verbinclude Install.txt
 * 
 * \page plugin_params Plugin Paramter Documentation
 * \verbinclude Plugin_Parameter_Docs.txt
 *
 * \page matlab_api Matlab API Documentation
 * \verbinclude Contents.m
 *
 */

#ifndef StimApp_H
#define StimApp_H

#include <QApplication>
#include <QColor>
#include <QMutex>
#include <QMutexLocker>
#include <QSize>
#include "Util.h"
class QTextEdit;
class ConsoleWindow;
class GLWindow;
class QTcpServer;
/**
   \brief The central class to the program that more-or-less encapsulates most objects and data in the program.

   This class inherits from QApplication for simplicity.  It is sort of a 
   central place that other parts of the program use to find settings,
   pointers to other windows, and various other application-wide data.
*/   
class StimApp : public QApplication
{
    Q_OBJECT

    friend int main(int, char **);
    StimApp(int & argc, char ** argv);  ///< only main can constuct us
public:

    /// Returns a pointer to the singleton instance of this class, if one exists, otherwise returns 0
    static StimApp *instance() { return singleton; }

    virtual ~StimApp();

    /// Some parameters related to the external SpikeGL program notification
    struct SpikeGLNotifyParams {
        bool enabled;  ///< if true, SpikeGL is notified on plugin start/stop
		bool nloopsNotifyPerIter;
        QString hostname;
        unsigned short port;
        int timeout_ms;
    } spikeGLNotifyParams;

	struct GlobalDefaults {
		int mon_x_pix, mon_y_pix, ftrackbox_x, ftrackbox_y, ftrackbox_w, Nblinks;
		char color_order[4];
		int fps_mode;
		QString 
			ftrack_track_color, 
			ftrack_off_color,
			ftrack_change_color,
			ftrack_start_color,
			ftrack_end_color,
		    interTrialBg;
		
		QString DO_with_vsync;

		GlobalDefaults() : mon_x_pix(800), mon_y_pix(600), ftrackbox_x(0), ftrackbox_y(0), ftrackbox_w(0), Nblinks(1), fps_mode(2), DO_with_vsync("off") {
			qstrncpy(color_order, "brg", 4);
			ftrack_track_color = ftrack_change_color = ftrack_start_color = ftrack_end_color = "1, 1, 1";
			ftrack_off_color = "0, 0, 0";			
			interTrialBg = ".5, .5, .5";
		}
	} globalDefaults;
    
    /// Returns a pointer to the application-wide GLWindow instance.
    GLWindow *glWin() const { return const_cast<GLWindow *>(glWindow); }

    /// Returns a pointer to the application-wide ConsoleWindow instance
    ConsoleWindow *console() const { return const_cast<ConsoleWindow *>(consoleWindow); }

    /// Returns true iff the application's console window has debug output printing enabled
    bool isDebugMode() const;

	bool isNoDropFrameWarn() const;
	
	bool isSaveFrameVars() const;

	bool isSaveParamHistory() const;
	
	bool isFrameDumpMode() const;
	
	bool isVSyncDisabled() const;
	
    /// Returns the directory under which all plugin data files are to be saved.
    QString outputDirectory() const { QMutexLocker l(&mut); return outDir; }
    /// Set the directory under which all plugin data files are to be saved. NB: dpath must exist otherwise it is not set and false is returned
    bool setOutputDirectory(const QString & dpath);

    /// Thread-safe logging -- logs a line to the log window in a thread-safe manner
    void logLine(const QString & line, const QColor & = QColor());

    /// Display a message to the status bar
    void statusMsg(const QString & message, int timeout_msecs = 0);

    /// Used to catch various events from other threads, etc
    bool eventFilter ( QObject * watched, QEvent * event );

    enum EventsTypes {
        LogLineEventType = QEvent::User, ///< used to catch log line events see StimApp.cpp 
        StatusMsgEventType, ///< used to indicate the event contains a status message for the status bar
        QuitEventType, ///< so we can post quit events..
    };
#ifndef Q_OS_WIN
    /// Returns the refresh rate as calibrated by CalinPlugin
    unsigned refreshRate() const { return refresh; }
#else
    /// Returns the refresh rate for the monitor that the GLWindow is currently  sitting in, as reported by Windows
    unsigned refreshRate() const { return getHWRefreshRate(); }
#endif
    /// Use this function to completely lock the mouse and keyboard -- useful if performing a task that must not be interrupted by the user.  Use sparingly!
    static void lockMouseKeyboard();
    /// Undo the damage done by a lockMouseKeyboard() call
    static void releaseMouseKeyboard();

    /// Returns true if and only if the application is still initializing and not done with its startup.  This is mainly used by the socket connection code to make incoming connections stall until the application is finished initializing.
    bool busy() const { return initializing; }

	void loadSettings();
    void saveSettings();

public slots:    
    /// Set/unset the application-wide 'debug' mode setting.  If the application is in debug mode, Debug() messages are printed to the console window, otherwise they are not
    void setDebugMode(bool); 
    void setExcessiveDebugMode(bool); ///< access this with the global var Util::excessiveDebug

	void setNoDropFrameWarn(bool);
	
	void setSaveFrameVars(bool);

	void setSaveParamHistory(bool);
	
	void setFrameDumpMode(bool);	
	
	void setVSyncDisabled(bool);
	
    /// Pops up a UI so that the user can select a stim. config file to load.
    void loadStim();
    /// Unloads/stops the currently running plugin and may prompt the user to save
    void unloadStim();
    /// Pops up the application "About" dialog box
    void about();
	/// Pops up a file dialog for an .FMV file to check for errors...
	void checkFMV();
    /// \brief Prompts the user to pick a save directory.
    /// @see setOutputDirectory 
    /// @see outputDirectory
    void pickOutputDir();
    void pauseUnpause(); ///< Calls GLWindow::pauseUnpause()

	/// A NOOP in Windows, otherwise initiates the refresh rate calibration
    void calibrateRefresh();

    /// Toggles the console window hidden/shown state
    void hideUnhideConsole();

    /// Aligns the GLWindow to the screen's top-left corner. ('A' hotkey in the UI.)
    void alignGLWindow();

    /// Pops up the SpikeGL integration options dialog and sets the params based on this Window
    void spikeGLIntegrationDialog();

    /// Pops up the Global Default Params dialog
    void globalDefaultsDialog();

signals:
	void gotCheckFMV(const QString & file);
	
protected:

protected slots:
	/// A NOOP in Windows, otherwise called by calibrateRefresh() when it finishes
    void calibratedRefresh(unsigned);
	
    /// Called from a timer every ~250 ms to update the status bar at the bottom of the console window
    void updateStatusBar();

    void quitCleanup();

private:
    void initServer();
    void initPlugins();
    void createAppIcon();
    void createGLWindow(bool initPlugins = true);

    mutable QMutex mut; ///< used to lock outDir param for now
    ConsoleWindow *consoleWindow;
    GLWindow *glWindow;
    bool glWinHasFrame, debug, noDropFrameWarn, saveFrameVars, saveParamHistory, vsyncDisabled;
    QString lastFile, lastFMV;
    volatile bool initializing;
    QColor defaultLogColor;
    QTcpServer *server;
    QString outDir;
#ifndef Q_OS_WIN
    unsigned refresh;
#endif
    static StimApp *singleton;
    unsigned nLinesInLog, nLinesInLogMax;
    QSize glWinSize;
};

#endif
