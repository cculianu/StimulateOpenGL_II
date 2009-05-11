#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "GLWindow.h"
#include <QCloseEvent>
#include <QResizeEvent>
#include "Util.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#else
#  include <GL/gl.h>
#endif
#include <math.h>
#include <QTimer>
//-- add your plugins here
#include "StimPlugin.h"

#define WINDOW_TITLE "StimulateOpenGL II - GLWindow"

GLWindow::GLWindow(unsigned w, unsigned h)
    : QGLWidget((QWidget *)0), running(0), paused(false), tooFastWarned(false), lastHWFC(0), tLastFrame(0.), tLastLastFrame(0.)
{
    QSize s(w, h);
    setMaximumSize(s);
    setMinimumSize(s);
    resize(w, h); // static size for now    
    setAutoBufferSwap(false);
    QGLFormat f = format();
    f.setDoubleBuffer(true);
#if QT_VERSION >= 0x040200
    f.setSwapInterval(1); 
#else
#   error Need Qt 4.2 or above to enable GL swapinterval!
#endif

    setFormat(f);
    setMouseTracking(false);
    timer = new QTimer(this);
    Connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));
    timer->setSingleShot(true);
    timer->start(0);
    setWindowTitle(WINDOW_TITLE);
}

GLWindow::~GLWindow() { if (running) running->stop(); }

void GLWindow::closeEvent(QCloseEvent *evt)
{
    if (stimApp()->console()->isHidden()) {
        // let's accept the close event and close the app
        evt->accept();
        qApp->quit();
    } else
        // the console is open so closing this window should not be allowed
        evt->ignore();
}

void GLWindow::resizeEvent(QResizeEvent *evt)
{
    if (evt->size().width() <= maximumSize().width()
        && evt->size().height() <= maximumSize().height()
        && evt->size().width() >= minimumSize().width()
        && evt->size().height() >= minimumSize().height()) 
            evt->accept();
    else 
            evt->ignore();
}

// Set up the rendering context, define display lists etc.:
void GLWindow::initializeGL()
{
    Debug() << "initializeGL()";
    // to make the system fast, make sure that a lot of stuff that OpenGL is capable of is disabled
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_LIGHT0 );
    glDisable( GL_STENCIL_TEST );
    glDisable( GL_DITHER );
    glDrawBuffer( GL_BACK_LEFT );

    glClearColor( 0.5, 0.5, 0.5, 0.0 ); //set the clearing color to be gray
    glShadeModel( GL_FLAT );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
}

// setup viewport, projection etc.:
void GLWindow::resizeGL(int w, int h)
{
    Debug() << "resizeGL(" << w << ", " << h << ")";

    // set the viewport to be the entire window
    glViewport(0, 0,(GLsizei)w, (GLsizei)h );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluOrtho2D( 0.0, (GLdouble)w, 0.0, (GLdouble) h );

}

// draw each frame
void GLWindow::paintGL()
{
    tThisFrame = getTime();
    bool tooFast = false, tooSlow = false;

    if (timer->isActive()) return; // this was a spurious paint event
	//    unsigned timerpd = 1000/getHWRefreshRate()/2;
    unsigned timerpd = 1000/stimApp()->refreshRate()/2;
    if (stimApp()->busy()) timerpd = 0;

#ifdef Q_OS_WIN
    if (getNProcessors() < 2 && tThisFrame - tLastFrame <= timerpd*1e3) {
        // we got called again quickly, yield CPU to prevent locking machine
        SwitchToThread();
        tThisFrame = getTime();
    }
#endif

    if (hasAccurateHWFrameCount()) {
        unsigned hwfc = getHWFrameCount();

        if (lastHWFC && hwfc==lastHWFC) {
            tooFast = true;
        } else if (lastHWFC && hwfc-lastHWFC > 1) {
            tooSlow = true;
        }
        lastHWFC = hwfc;
    } else if (!stimApp()->busy()/*hasAccurateHWRefreshRate()*/ && tLastLastFrame > 0.) {
        double diff = tThisFrame-tLastFrame/*, diff2 = tThisFrame-tLastLastFrame*/;
        double tFrames = 1.0/getHWRefreshRate();
        if (diff > tFrames*2.) tooSlow = true;
        /*else if (diff2 < tFrames) tooFast = true;*/
    }

    if (tooFast) {
        if (!tooFastWarned) {
            //Debug() << "Frame " << getHWFrameCount() << " too fast, will render later";
            tooFastWarned = true;
        }
        timer->start(timerpd);
        return;
    }
    tooFastWarned = false;

    if (tooSlow) {
        Warning() << "Dropped frame " << getHWFrameCount();
    }
               
    if (!paused) {
        // NB: don't clear here, let the plugin do clearing as an optimization
        // glClear( GL_COLOR_BUFFER_BIT );

        if (running) {
            if (tooSlow) 
                // indicate the frame was skipped
                running->putMissedFrame(static_cast<unsigned>((tThisFrame-tLastFrame)*1e3));
            running->cycleTimeLeft = 1.0/getHWRefreshRate();
            running->computeFPS();            
            running->drawFrame();
            // NB: running ptr may be made null if drawFrame() called stop()
            if (running) ++running->frameNum;
        }
    }

    tLastLastFrame = tLastFrame;
    tLastFrame = tThisFrame;

    swapBuffers();// should wait for vsync...   

#ifdef Q_OS_WIN
    //timer->start(timerpd);     
    update();
#else
    timer->start(0);     
#endif

    if (running && !paused) {
        running->cycleTimeLeft -= getTime()-tThisFrame;
        running->afterVSync();
    }
    
}

void GLWindow::pluginCreated(StimPlugin *p)
{
    if (pluginsList.indexOf(p) < 0) {
        pluginsList.push_back(p);
    }
}

void GLWindow::pluginDeleted(StimPlugin *p)
{
    pluginsList.removeAll(p);
    if (running == p) { running = 0; }
}

void GLWindow::pluginStarted(StimPlugin *p)
{
    if (running) { running->stop(); }
    running = p;
    Log() << p->name() << " started";

}

void GLWindow::pluginStopped(StimPlugin *p)
{
    if (running == p) {
        running = 0;
        paused = false;
        Log() << p->name() << " stopped.";
        setWindowTitle(WINDOW_TITLE);
    } else {
        Error() << "pluginStopped() but running != p";
    }
}


StimPlugin *GLWindow::pluginFind(const QString &name, bool casesensitive)
{
    Qt::CaseSensitivity cs = casesensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    for(QList<StimPlugin *>::iterator it = pluginsList.begin(); it != pluginsList.end(); ++it) {
        if ((*it)->name().compare(name, cs) == 0) {
            return *it;
        }
    }
    return 0; // not found!
}


//--- put new StimPlugins here, and then modify GLWindow::initPlugins()
#include "CalibPlugin.h"
#include "MovingObjects.h"
#include "MovingGrating.h"
#include "CheckerFlicker.h"
void GLWindow::initPlugins()
{
    Log() << "Initializing plugins...";

    // it's ok to new these objects -- they automatically attach themselves 
    // to this instance and will be auto-deleted when this object is deleted.
#ifndef Q_OS_WIN
    new CalibPlugin(); 
#endif
    new MovingObjects();  // experiment plugin.. bouncey square!
    new MovingGrating();  // experiment plugin.. the grid!
    new CheckerFlicker(); // experiment plugin.. the checkerboard!

    // TODO: more plugins here


    Log() << "Initialized " << pluginsList.size() << " plugins.";
}

// overrides QWidget methoed -- catch keypresses
void GLWindow::keyPressEvent(QKeyEvent *event)
{
    makeCurrent(); // just in case they do opengl commands in their plugin
    if (running && running->processKey(event->key())) {
        event->accept();
        Debug() << "Key '" << char(event->key()) << "' accepted.";
        return;
    }
    switch (event->key()) {
    case ' ': // space bar, pause/unpause
        stimApp()->pauseUnpause();
        break;
    case 'L':
    case 'l':
        stimApp()->loadStim();
        break;
    case 'd':
    case 'D':
        stimApp()->setDebugMode(!stimApp()->isDebugMode());
        break;
#ifndef Q_OS_WIN
    case 'r':
    case 'R':
        stimApp()->calibrateRefresh();
        break;
#endif
    case 'c':
    case 'C':
        stimApp()->hideUnhideConsole();
        break;
    case 'a':
    case 'A':
        stimApp()->alignGLWindow();
        break;

    case Qt::Key_Escape: 
        stimApp()->unloadStim();
        break;
    }
}

void GLWindow::pauseUnpause()
{
    if (!running) return;
    paused = !paused;
    Log() << (paused ? "Paused" : "Unpaused");
}

QList<QString> GLWindow::plugins() const
{
    QList<QString> ret;
    for(QList<StimPlugin *>::const_iterator it = pluginsList.begin(); it != pluginsList.end(); ++it) {
        ret.push_back((*it)->name());
    }
    return ret;
}
