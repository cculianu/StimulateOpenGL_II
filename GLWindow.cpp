#include "GLHeaders.h"
#include "GLWindow.h"
#include <QCloseEvent>
#include <QResizeEvent>
#include "Util.h"
#include <math.h>
#include <QTimer>
//-- add your plugins here
#include "StimPlugin.h"
#include "DAQ.h"

#define WINDOW_TITLE "StimulateOpenGL II - GLWindow"

GLWindow::GLWindow(unsigned w, unsigned h, bool frameless)
    : QGLWidget((QWidget *)0,0,static_cast<Qt::WindowFlags>(
#ifdef Q_OS_WIN														
															Qt::MSWindowsOwnDC|
#endif
															(frameless ? Qt::FramelessWindowHint : 0))), aMode(false), running(0), paused(false), tooFastWarned(false),  lastHWFC(0), tLastFrame(0.), tLastLastFrame(0.), delayCtr(0), debugLogFrames(false)
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

GLWindow::~GLWindow() { 
    if (running) running->stop(); 
    // be sure to remove all plugins while we are still a valid GLWindow instance, to avoid a crash bug
    while (pluginsList.count())
        delete pluginsList.front(); // StimPlugin * should auto-remove itself from list so list will shrink..
}

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

    glClearColor( 0.5, 0.5, 0.5, 1.0 ); //set the clearing color to be gray
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


void GLWindow::drawEndStateBlankScreen(StimPlugin *p) {
	float color[4];
	GLboolean blend;
	const float graylevel = p->bgcolor;	
	glGetFloatv(GL_COLOR_CLEAR_VALUE, color);
	glGetBooleanv(GL_BLEND, &blend);
	
	glClearColor(graylevel, graylevel, graylevel, 1.0f);
	if (blend) glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);
	p->currentFTState = StimPlugin::FT_End;
	p->drawFTBox();
	if (blend) glEnable(GL_BLEND);
	glClearColor(color[0], color[1], color[2], color[3]);	
}

void GLWindow::drawEndStateBlankScreenImmediately(StimPlugin *p)
{	
	drawEndStateBlankScreen(p);
	swapBuffers(); ///< wait for vsync..
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

    if (tooSlow && !stimApp()->isNoDropFrameWarn()) {
        Warning() << "Dropped frame " << getHWFrameCount();
    }
               
    bool doBufSwap = false;

	if (running) {
		// unconditionally setup the clear color here
		switch(running->fps_mode) {
			case FPS_Dual: glClearColor(0.f, running->bgcolor, running->bgcolor, 1.0); break; // dual mode has blank RED channel (RED channel is first frame)
			default: glClearColor(running->bgcolor, running->bgcolor, running->bgcolor, 1.0); break;
		}
	} else
		glClearColor(0.5, 0.5, 0.5, 1.0);

	///bool dframe = false;
	
    if (!paused) {
        // NB: don't clear here, let the plugin do clearing as an optimization
        // glClear( GL_COLOR_BUFFER_BIT );

        if (running && running->initted) {
            if (tooSlow) 
                // indicate the frame was skipped
                running->putMissedFrame(static_cast<unsigned>((tThisFrame-tLastFrame)*1e3));
            running->cycleTimeLeft = 1.0/getHWRefreshRate();
            running->computeFPS();
				
			// if nFrames mode and frameNum >= nFrames.. loop plugin by stopping then restarting
			if ((running->nFrames && running->frameNum >= running->nFrames)
				|| (running->have_fv_input_file && running->frameVars->atEnd())) { /// or if reading framevar file and it ended..
				const unsigned loopCt = running->loopCt + 1, nLoops = running->nLoops;
				StimPlugin * const p = running;
				const bool doRestart = !nLoops || loopCt < nLoops;
				const bool hadDelay = (delayCtr = p->delay) > 0;
				p->stop(false,false,doRestart);
				if (doRestart) {
					if (delayCtr-- > 0) {
						// force screen clear *NOW* as per Anthony's specs, so that we don't hang on last frame forever..
						drawEndStateBlankScreenImmediately(p);
						/*
						/// XXX
						Debug() << "looped, drew delayframe, hwfc=" << getHWFrameCount();
						dframe = true;
						 */
					}
					const double t0 = getTime();
					const int saved_delayCtr = delayCtr;
					p->loopCt = loopCt;
					p->start(true);
					delayCtr = saved_delayCtr;
					const double tRestart = getTime() - t0;
					delayCtr -= int( tRestart * getHWRefreshRate() ); ///< this many frames have elapsed during startup, so reduce our delay Ctr by that much
					if (hadDelay && delayCtr < 0) {
						Warning() << "Inter-loop restart/setup time of " << tRestart << "s took longer than delay=" << p->delay << " frames!  Increase `delay' to avoid this situation!";
					}
					/*
					/// XXX
					Debug() << "reinitted, tRestart=" << tRestart << ", hwfc=" << getHWFrameCount() << ", delayCtr=" << delayCtr;
					dframe = true;
					 */
					
					p->loopCt = loopCt;
					p->frameVars->closeAndRemoveOutput(); /// remove redundant frame var file!
				} else
					delayCtr = 0;
			}
			if (running && delayCtr > 0) {
				drawEndStateBlankScreen(running); ///< this draws the FT box in the end state
				--delayCtr;
				doBufSwap = true;
			} else if (running) { // note: code above may have stopped plugin, check if it's still running
			
				running->drawFrame();
				
				if (running) { // NB: drawFrame may have called stop(), thus NULLing this pointer, so check it again
					running->advanceFTState(); // NB: this asserts FT_Start/FT_Change/FT_End flag, if need be, etc, and otherwise decides whith FT color to us.  Looks at running->nFrames, etc
					running->drawFTBox();
					if (debugLogFrames) running->logBackbufferToDisk();
					++running->frameNum;				
					doBufSwap = true;
				} 			
			}
        }
    }
	if (!running) {  
		// no plugin running, draw default .5 gray bg without ft box
        glClear( GL_COLOR_BUFFER_BIT );
        doBufSwap = true;		
    } else if (running && running->getFrameNum() < 0 && (paused || delayCtr <= 0)) { 
		// running but paused and before plugin started (and not delay mode because that's handled above!)
		// if so, draw plugin bg with ftrack_end box
		drawEndStateBlankScreen(running);
		doBufSwap = true;
	}

    tLastLastFrame = tLastFrame;
    tLastFrame = tThisFrame;

    if (doBufSwap) {// doBufSwap is normally true either if we don't have aMode or if we have a plugin and it is running and not paused

		swapBuffers();// should wait for vsync...   
		
		/// XXX
		/*if (dframe) {
			Debug() << " dframe, after buf swap hwfc=" << getHWFrameCount() << ", delayCtr=" << delayCtr;
		}*/
		
		QString devChan;
		if (running && !paused && running->frameNum == 1 && running->getParam("DO_with_vsync", devChan) && devChan != "off" && devChan.length()) {
			DAQ::WriteDO(devChan, true);
		}
		

    } else {
        // don't swap buffers here to avoid frame ghosts in 'A' Mode on Windows.. We get frame ghosts in Windows in 'A' mode when paused or not running because we didn't draw a new frame if paused, and so swapping the buffers causes previous frames to appear onscreen
    }

#ifdef Q_OS_WIN
	    //timer->start(timerpd);
	    update();
#else
	    timer->start(0);
#endif

    if (running && running->initted && !paused) {
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

void GLWindow::pluginDidFinishInit(StimPlugin *p) {
	delayCtr = p->delay;
}

void GLWindow::pluginStopped(StimPlugin *p)
{
	if (running != p)
		Error() << "pluginStopped() but running != p";
    if (running == p) {
		
        running = 0;
        paused = false;
        Log() << p->name() << " stopped.";
        setWindowTitle(WINDOW_TITLE);
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
#include "MovingGrating.h"
#include "CheckerFlicker.h"
#include "Flicker.h"
#include "Flicker_RGBW.h"
#include "Sawtooth.h"
#include "MovingObjects_Old.h"
#include "MovingObjects.h"
void GLWindow::initPlugins()
{
    Log() << "Initializing plugins...";

    // it's ok to new these objects -- they automatically attach themselves 
    // to this instance and will be auto-deleted when this object is deleted.
#ifndef Q_OS_WIN
    new CalibPlugin(); 
#endif
    new MovingGrating();  // experiment plugin.. the grid!
    new CheckerFlicker(); // experiment plugin.. the checkerboard!
	new Flicker();        // experiment plugin.. the flicker tester!
	new Flicker_RGBW();   // experiment plugin.. the old legacy flicker for the original BRGW-style projectors 
	new Sawtooth();       // experiment plugin.. the sawtooth generator!
    new MovingObjects_Old();  // experiment plugin.. bouncey square!
	new MovingObjects();

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
        //stimApp()->loadStim();
        QTimer::singleShot(1, stimApp(), SLOT(loadStim())); // ends up possibly deleting this object, so we don't want to run this from this event handler, thus we'll enqueue it.
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
        //stimApp()->alignGLWindow();
        QTimer::singleShot(1, stimApp(), SLOT(alignGLWindow())); // ends up deleting this object, so we don't want to run this from this event handler, thus we'll enqueue it.
        break;

    case Qt::Key_Escape: 
        //stimApp()->unloadStim();
        QTimer::singleShot(1, stimApp(), SLOT(unloadStim())); // may end up possibly deleting this object?

        break;
    }
}

void GLWindow::pauseUnpause()
{
    if (!running) return;
    paused = !paused;
    Log() << (paused ? "Paused" : "Unpaused");
    if (!paused && !running->frameNum && running->needNotifyStart
		&& ((stimApp()->leoDAQGLNotifyParams.nloopsNotifyPerIter || running->loopCt == 0)) ) 
        running->notifySpikeGLAboutStart();  
}

QList<QString> GLWindow::plugins() const
{
    QList<QString> ret;
    for(QList<StimPlugin *>::const_iterator it = pluginsList.begin(); it != pluginsList.end(); ++it) {
        ret.push_back((*it)->name());
    }
    return ret;
}

