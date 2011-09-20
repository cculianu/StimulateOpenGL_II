#ifndef GLWindow_H
#define GLWindow_H

#include <QGLWidget>
#include <QList>
#include "Util.h"
class StimPlugin;
class QTimer;
class StimApp;


/**
   \brief The main Open GL display window.

   This class encapsulates the main OpenGL window that the application
   uses to render graphics to.

   It also manages plugins and plugin execution via the pluginFind(), 
   runningPlugin(), isPaused() and plugins() methods.

   This class also handles all keyboard hotkeys (see keyPressEvent())
*/
   
class GLWindow : public QGLWidget
{
     Q_OBJECT        // must include this if you use Qt signals/slots

     friend class StimPlugin;
     friend class StimApp;

     GLWindow(unsigned width = 800, unsigned height = 600, bool frameless = false);

public:
     ~GLWindow();

     /// Find a plugin by name.  Returns a pointer to the plugin if found, or NULL if not found.
     StimPlugin *pluginFind(const QString &name, bool casesensitive = false);
     
     /// The currently running plugin, if any.  Returns NULL if no plugin
     /// is running.
     StimPlugin *runningPlugin() const { return const_cast<StimPlugin *>(running); }
     /// Returns true if plugin execution is in the paused state, false otherwise.
     bool isPaused() const { return paused; }

     /// Returns a list of all the plugin names that are currently loaded (but not necessarily running).
     QList<QString> plugins() const;


     /// Iff true, then the GLWindow is in 'a' mode, that is, fullscreen
     volatile bool aMode;
	
	 /// Used to set the inter-trial bg color and/or the default clear color
	 void setClearColor(const Util::Vec3 & c) { clearColor = c; }
	 Util::Vec3 getClearColor() const { return clearColor; }

public slots:
    /// Toggles the paused/unpaused state of the plugin execution engine.
    void pauseUnpause();

protected:
    /// Plugins call this when they want to tell the execution engine that they have been created 
    void pluginCreated(StimPlugin *plugin);
    /// Plugins call this method when they want to tell the execution engine that they have been started
    void pluginStarted(StimPlugin *plugin);
	/// Plugins call this method at the end of their start() method to signal that they have just finished initialization
	void pluginDidFinishInit(StimPlugin *plugin);
    /// Plugins call this method when they want to tell the execution engine that they have been stopped
    void pluginStopped(StimPlugin *plugin);
    /// Plugins call this method to inform the execution engine that they are being deleted and should be dereferenced from any internal data structures.
    void pluginDeleted(StimPlugin *plugin);
 
protected:

     /// Sets up the rendering context, clears the screen, sets/unsets various opengl parameters
     void initializeGL();

     /// Sets up the viewport and projection etc.:
     void resizeGL(int w, int h);

     /** \brief Called once per vblank to draw each frame.  

         Updates some statistics, then calls the 
         currently active plugin's StimPlugin::drawFrame() virtual function.
         Note that if no plugin is running or if plugin execution is paused
         the plugin's drawFrame() function is not called. */
     void paintGL();

     /// override close events -- prevent window from being closed!
     void closeEvent(QCloseEvent *event);

     /// overrides QWidget method -- prevent window resize
     void resizeEvent(QResizeEvent *event);

     /// overrides QWidget methoed -- catch keypresses -- all keyboard handling happens in this class
     void keyPressEvent(QKeyEvent *event);

private:
	bool blockPaint;
	StimPlugin *running;
	QList<StimPlugin *> pluginsList;
	QTimer *timer;
	QByteArray blinkBuf;

	void copyBlinkBuf();
	void drawBlinkBuf();

	void initPlugins(); 
	void drawEndStateBlankScreen(StimPlugin *, bool isBlankBGFrame);
	void drawEndStateBlankScreenImmediately(StimPlugin *, bool isBlankBGFrame);

	bool paused, tooFastWarned;
	unsigned lastHWFC; ///< last hardware frame count, only iff platform has an accurate hwfc
	double tThisFrame, tLastFrame, tLastLastFrame;
	int delayCtr;
	double delayt0, delayFPS;
	
	bool debugLogFrames;
	Util::Vec3 clearColor;
};

#endif
