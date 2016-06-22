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

#include <QOpenGLShaderProgram>
#include <QOpenGLFrameBufferObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QImage>
#include <QColor>

#define WINDOW_TITLE "StimulateOpenGL II - GLWindow"

GLWindow::GLWindow(unsigned w, unsigned h, bool frameless)
    : QGLWidget((QWidget *)0,0,static_cast<Qt::WindowFlags>(
#ifdef Q_OS_WIN														
															Qt::MSWindowsOwnDC|
#endif
															(frameless ? Qt::FramelessWindowHint : (Qt::WindowFlags)0))), 
       aMode(false), running(0), paused(false), tooFastWarned(false),  
       lastHWFC(0), tLastFrame(0.), tLastLastFrame(0.), delayCtr(0), delayt0(0.), 
       delayFPS(0.), debugLogFrames(false), clearColor(0.5,0.5,0.5), fs_w(0), fs_h(0), fs_pbo_ix(0), fs_delay_ctr(1.0f), shader(0), fbo(0), hotspotTex(0), hotspotImg(1,1,QImage::Format_ARGB32)

{
    hotspotImg.setPixelColor(QPoint(0,0),QColor(255,255,255,255));
    memset(fs_pbo, 0, sizeof fs_pbo);
	memset(fs_bytesz, 0, sizeof fs_bytesz);
	if (fshare.shm) {
		Log() << "GLWindow: " << (fshare.createdByThisInstance ? "Created" : "Attatched to pre-existing") <<  " SpikeGL 'frame share' memory segment, size: " << (double(fshare.size())/1024.0/1024.0) << "MB.";
		fshare.lock();
		const bool already_running = fshare.shm->stimgl_pid;
		fshare.unlock();
		if (!already_running || fshare.warnUserAlreadyRunning()) {
			fshare.lock();
			fshare.shm->do_box_select = 0; ///< clear possibly-stale value
			fshare.shm->stimgl_pid = Util::getPid();
			fshare.unlock();
		} else {
			Warning() << "Possible duplicate instance of StimGL, disabling 'frame share' in this instance.";
			fshare.detach();	
		}
	} else {
		Error() << "INTERNAL ERROR: Could not attach to SpikeGL 'frame share' shared memory segment! FIXME!";
	}
	
	boxSelector = new GLBoxSelector(this);
	boxSelector->loadSettings();
	blockPaint = false;
	win_width = w;
	win_height = h;
	clrImg_tex = clrImg_w = clrImg_h = 0;
	hw_refresh = getHWRefreshRate();
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
    //setMouseTracking(false);
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
	criticalCleanup();
}

void GLWindow::setupShaders()
{
    delete shader; shader = 0;

    shader = new QOpenGLShaderProgram(this);
    shader->addShaderFromSourceFile(QOpenGLShader::Fragment,":/Shaders/frag_shader.frag");
    if (!shader->link()) {
        Error() << ">>>>>>  POSSIBLY FATAL: shader link error:" << shader->log();
    }
}

void GLWindow::shaderApplyAndDraw()
{
    if (!fbo || !shader) return;
    if (!fbo->isValid()) {
        Error() << "INTERNAL: FBO is invalid in shaderApplyAndDraw()";
    }

    const int texUnit = 7, hotspotUnit = 8;

    shader->bind();
    shader->setUniformValue("srcTex", texUnit);
    shader->setUniformValue("hotspots", hotspotUnit);


    /* draw the texture... applying hotspots */
    {
        const int w = win_width, h = win_height;
        const GLint v[] = {
            0,0, w,0, w,h, 0,h
        }, t[] = {
            0,0, w,0, w,h, 0,h
        };
        QOpenGLContext::currentContext()->functions()->glActiveTexture(GL_TEXTURE0+hotspotUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, hotspotTex->textureId());
        QOpenGLContext::currentContext()->functions()->glActiveTexture(GL_TEXTURE0+texUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, fbo->texture());
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // render our vertex and coord buffers which don't change.. just the texture changes
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        GLfloat c[4];
        glGetFloatv(GL_CURRENT_COLOR, c);
        glColor4f(1.f,1.f,1.f,1.f);
        glVertexPointer(2, GL_INT, 0, v);
        glTexCoordPointer(2, GL_INT, 0, t);
        glDrawArrays(GL_QUADS, 0, 4);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        QOpenGLContext::currentContext()->functions()->glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        glColor4f(c[0],c[1],c[2],c[3]);
    }

    shader->release();
}

void GLWindow::criticalCleanup() { 
	if (fshare.shm) { fshare.lock(); fshare.shm->stimgl_pid = 0; fshare.unlock(); }
	if (fs_pbo[0]) { glDeleteBuffers(N_PBOS, fs_pbo); memset(fs_pbo, 0, sizeof fs_pbo); }
	if (clrImg_tex) glDeleteTextures(1, &clrImg_tex), clrImg_tex = clrImg_w = clrImg_h = 0; 
    delete shader, shader = 0;
    delete fbo, fbo = 0;
}

void GLWindow::setClearColor(const QString & c)
{
	QVector<double> csv = parseCSV(c);
	if (csv.size() >= 3) {
		setClearColor(Vec3(csv[0], csv[1], csv[2]));
	} else {
		bool ok;
		double d = c.toDouble(&ok);
		if (ok) setClearColor(Vec3(d, d, d));
	}
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

void GLWindow::moveEvent(QMoveEvent *evt)
{
	hw_refresh = getHWRefreshRate();
	QGLWidget::moveEvent(evt);
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
    glDrawBuffer( GL_BACK );

    glClearColor( clearColor.r, clearColor.g, clearColor.b, 1.0 ); //set the clearing color to be gray
    glShadeModel( GL_FLAT );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	boxSelector->init();

    setupShaders();
}

// setup viewport, projection etc.:
void GLWindow::resizeGL(int w, int h)
{
    Debug() << "resizeGL(" << w << ", " << h << ")";

    // set the viewport to be the entire window
    glViewport(0, 0,(GLsizei)w, (GLsizei)h );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0.0, (GLdouble)w, 0.0, (GLdouble) h, -1e6, 1e6 );
	win_width = w;
	win_height = h;
	hw_refresh = getHWRefreshRate();
    fs_rect = fs_rect_saved = boxSelector->getBox();

    // scale the hotspot correction image to the new size...
    if (hotspotTex) delete hotspotTex;
    hotspotTex = new QOpenGLTexture(QOpenGLTexture::TargetRectangle);
    hotspotTex->create();
    hotspotTex->setData(hotspotImg.scaled(QSize(w,h), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    if (fbo) delete fbo;
    fbo = new QOpenGLFramebufferObject(w, h, GL_TEXTURE_RECTANGLE);
    if (!fbo->isValid()) {
        Error() << "FBO IS INVALID! WHY?!";
    }
    QOpenGLFramebufferObject::bindDefault(); // why do i need to call this???

}

void GLWindow::clearScreen()
{
	if (clrImg_tex) {
		/* draw the texture... */ 
		{
			const int w = win_width, h = win_height;
			const int v[] = {
				0,0, w,0, w,h, 0,h
			}, t[] = {
				0,0, clrImg_w,0, clrImg_w,clrImg_h, 0,clrImg_h
			};
			bool wasEnabled = glIsEnabled(GL_TEXTURE_RECTANGLE_ARB);
			if (!wasEnabled) glEnable(GL_TEXTURE_RECTANGLE_ARB);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, clrImg_tex);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			// render our vertex and coord buffers which don't change.. just the texture changes
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			GLfloat c[4];
			glGetFloatv(GL_CURRENT_COLOR, c);
			glColor4f(1.,1.,1.,1.);
			glVertexPointer(2, GL_INT, 0, v);
			glTexCoordPointer(2, GL_INT, 0, t);
			glDrawArrays(GL_QUADS, 0, 4);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);			
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);	
			glColor4f(c[0],c[1],c[2],c[3]);
			if (!wasEnabled) glDisable(GL_TEXTURE_RECTANGLE_ARB);			
		}		
	} else {
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void GLWindow::drawEndStateBlankScreen(StimPlugin *p, bool isBlankBG) {
	if (isBlankBG) {
		glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0);
		clearScreen();
	} else if (p) {		
		float color[4];
		GLboolean blend;
		const float graylevel = p->bgcolor;	
		glGetFloatv(GL_COLOR_CLEAR_VALUE, color);
		glGetBooleanv(GL_BLEND, &blend);
		
		glClearColor(graylevel, graylevel, graylevel, 1.0f);
		if (blend) glDisable(GL_BLEND);
		clearScreen();
	/* Apr. 10 Change: FT_Off on plugin load always
     if (p->delay > 0 && (delayCtr > 0 || !p->initted) && !paused)
    */
        p->currentFTState = StimPlugin::FT_Off;
	/*	else
            p->currentFTState = StimPlugin::FT_End;
     */
		p->drawFTBox();
		if (blend) glEnable(GL_BLEND);
		glClearColor(color[0], color[1], color[2], color[3]);	
	} else { // !p
		clearScreen();
		StimPlugin::drawFTBoxStatic();
	}
}

void GLWindow::drawEndStateBlankScreenImmediately(StimPlugin *p, bool isBlankBG)
{	
	drawEndStateBlankScreen(p, isBlankBG);
	swapBuffers(); ///< wait for vsync..
}

static void adjustRefreshToKnownPresets(double & delayFPS) {
	static const double presets[] = { 60.0, 70.0, 75.0, 80.0, 82.0, 85.0, 90.0, 100.0, 120.0, 125.0, 130.0, 140.0, 150.0, 160.0, 180.0, 240.0, 360.0, -1.0 };
	int found=-1;
	double lastDiff = 1e6;
	for (int i = 0; presets[i] > 0.; ++i) {
		const double diff = fabs(presets[i]-delayFPS);
		if (found < 0 || diff < lastDiff) {
			found = i;
			lastDiff = diff;
		}
		if (diff <= 1.0) {
			break;
		}
	}
	if (lastDiff > 5.0) {
		// if we get here.. something's wrong and chances are we will have some error...
		Warning() << "Measured refresh of " << delayFPS << " is likely wrong and it's not close enough to a known refresh rate for us to be comfortable forcing it to a known value.";
	} else 
		delayFPS = presets[found];
}


// draw each frame
void GLWindow::paintGL()
{
	if (blockPaint) return;
	
    tThisFrame = getTime();
    bool tooFast = false, tooSlow = false, signalDIOOn = false;
	
    if (timer->isActive()) return; // this was a spurious paint event
    unsigned timerpd = 1000/MAX(stimApp()->refreshRate(),120)/2;
    if (stimApp()->busy()) timerpd = 0;

#ifdef Q_OS_WIN
    if (getNProcessors() < 2 && tThisFrame - tLastFrame <= timerpd*1e3) {
        // we got called again quickly, yield CPU to prevent locking machine
        SwitchToThread();
        tThisFrame = getTime();
    }
#endif

    if (hasAccurateHWFrameCount() /* this is now always pretty much false */) {
        unsigned hwfc = getHWFrameCount();

        if (lastHWFC && hwfc==lastHWFC) {
            //tooFast = true;
        } else if (lastHWFC && hwfc-lastHWFC > 1) {
            tooSlow = true;
        }
        lastHWFC = hwfc;
    } else if (!stimApp()->busy()/*hasAccurateHWRefreshRate()*/ && tLastLastFrame > 0.) {
        double diff = tThisFrame-tLastFrame/*, diff2 = tThisFrame-tLastLastFrame*/;
        double tFrames = 1.0/hw_refresh;
        if (diff > tFrames*2.) tooSlow = true;
        /*else if (diff2 < tFrames) tooFast = true;*/
		lastHWFC = getHWFrameCount();
    } else
		lastHWFC = getHWFrameCount();

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
		unsigned fr = getHWFrameCount(); ///< some drivers don't return a hardware frame count!
		if (!fr) {
			StimPlugin *p = runningPlugin();
			if (p) fr = p->getFrameNum();
		} 
		if (fr)
			Warning() << "Dropped frame " << fr;
		else 
			Warning() << "Dropped a frame";
    }
               
    bool doBufSwap = false;

	if (running) {
		// unconditionally setup the clear color here
		switch(running->fps_mode) {
			case FPS_Dual: glClearColor(0.f, running->bgcolor, running->bgcolor, 1.0); break; // dual mode has blank RED channel (RED channel is first frame)
			default: glClearColor(running->bgcolor, running->bgcolor, running->bgcolor, 1.0); break;
		}
	} else
		glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0);

	bool dframe = false;
	int *blinkCt = running ? &running->blinkCt : 0;
	const int *Nblinks = running ? &running->Nblinks : 0;
	const bool haveBlinkBuf = running && *Nblinks > 1 && *blinkCt > 0;
	const bool saveBlinkBuf = !haveBlinkBuf && running && *Nblinks > 1 && *blinkCt == 0;
    bool drewEndStateBlankScreen = false;
	
    if (!paused && (!blinkCt || !(*blinkCt) || saveBlinkBuf)) {
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
				
				p->customStatusBarString.sprintf("Delay counter: %d", delayCtr);
				
				// NB: Need to draw this here as StimPlugin::stop() could potentially take FOREVER (checkerflicker!!)
				//     So we will do it here -- put the BG frame up as quickly as possible then worry about calling stop.
				//     We need to put the BG frame up in cases where we are stopping for good (in which case it's a gray
				//     bg, no ft box) or in cases where there are delay frames and we are looping -- in which case we put
				//     up the delay frame!
				if (!doRestart || hadDelay) {
					// force screen clear *NOW* as per Anthony's specs, so that we don't hang on last frame forever..
					drawEndStateBlankScreenImmediately(p, !doRestart);
                    drewEndStateBlankScreen = true;
					/**/
					 /// XXX
					 Debug() << "looped, drew delayframe, hwfc=" << getHWFrameCount() << ", delayCtr=" << delayCtr;
					 dframe = true;
					 //*/
				}
				--delayCtr;

				const double t0 = getTime();
				const int saved_delayCtr = delayCtr;
				p->stop(false,false,doRestart);
				if (doRestart) {
					timer->stop();
					blockPaint = true;
					p->loopCt = loopCt;
					p->start(true);
					p->waitForInitialization();
					blockPaint = false;
					timer->start(timerpd);					
					delayCtr = saved_delayCtr;
					p->customStatusBarString.sprintf("Delay counter: %d", delayCtr);
					p->loopCt = loopCt;
					// BEGIN UGLY HACK #7812
					if (!p->dontCloseFVarFileAcrossLoops) 
						p->frameVars->closeAndRemoveOutput(); /// remove redundant frame var file, unless we are moving object and we are doing random trials
					// END UGLY HACK #7812

					double tRestart = getTime() - t0;
					int frameFudge = (int)qRound( tRestart * delayFPS ); ///< this many frames have elapsed during startup, so reduce our delay Ctr by that much
					if (frameFudge > 0 && delayCtr > 0) {
						// force a frame so as to re-synch us to the vsync signal so that the fudge factor becomes more accurate
						drawEndStateBlankScreenImmediately(p, false);
                        drewEndStateBlankScreen = true;
					}
					// now we are synched to the vsync signal, so hopefully this fudging is more accurate 
					tRestart = getTime() - t0;
					frameFudge = (int)qRound( tRestart * delayFPS );
					delayCtr -= frameFudge; 
					if (hadDelay && delayCtr < 0) {
						Warning() << "Inter-loop restart/setup time of " << tRestart << "s took longer than delay=" << p->delay << " frames!  Increase `delay' to avoid this situation!";
					}
					
					/// XXX
					Debug() << "reinitted, tRestart=" << tRestart << ", hwfc=" << getHWFrameCount() << ", delayCtr=" << delayCtr;

					/**/
					 /// XXX
					 dframe = true;
					 //*/					
				} else
					delayCtr = 0;
			}
			if (running && delayCtr > 0) {
				drawEndStateBlankScreen(running, false); ///< this draws the FT box in the end state
				if (delayCtr == running->delay) {
					signalDIOOn = true;
					if (delayt0 <= 0.) delayt0 = getTime();
				}
				--delayCtr;
				running->customStatusBarString.sprintf("Delay counter: %d", delayCtr);
				doBufSwap = true;
			} else if (running) { // note: code above may have stopped plugin, check if it's still running
			
				if (!running->pluginDoesOwnClearing) {
					running->clearScreen();
				}
				
				glEnable(GL_SCISSOR_TEST); /// < the frame happens within our scissor rect, (lmargin, etc support)
                if (fbo &&
                    !fbo->bind())
                    Error() << "FBO bind() returned false!";
				running->drawFrame();
                if (fbo && fbo->isBound()) fbo->release();
				glDisable(GL_SCISSOR_TEST);                
                shaderApplyAndDraw(); // renders the above FBO to screen, having applied the shader to it
				
				if (running) { // NB: drawFrame may have called stop(), thus NULLing this pointer, so check it again
					running->advanceFTState(); // NB: this asserts FT_Start/FT_Change/FT_End flag, if need be, etc, and otherwise decides whith FT color to us.  Looks at running->nFrames, etc
					running->drawFTBox();
					running->afterFTBoxDraw();
					if (debugLogFrames) running->logBackbufferToDisk();
					++running->frameNum;
					if (running->delay <= 0 && running->frameNum == 1)
						signalDIOOn = true;
					doBufSwap = true;
				} 
				
			}
        }
		if (saveBlinkBuf) copyBlinkBuf();
    }	
	if (!paused && blinkCt && ++(*blinkCt) >= *Nblinks) *blinkCt = 0; 
	if (!running) {  
        if (!drewEndStateBlankScreen) {
            // no plugin running, draw default .5 gray bg without ft box
            drawEndStateBlankScreen(0, false);
            doBufSwap = true;
        }
    } else if (running && running->getFrameNum() < 0 && (paused/*|| delayCtr <= 0*/)) { 
		// running but paused and before plugin started (and not delay mode because that's handled above!)
		// if so, draw plugin bg with ftrack_end box
		drawEndStateBlankScreen(running, false);
		doBufSwap = true;
	} else if (running && !paused && haveBlinkBuf) {
		drawBlinkBuf();
		doBufSwap = true;
	}

    tLastLastFrame = tLastFrame;
    tLastFrame = tThisFrame;

    if (doBufSwap) {// doBufSwap is normally true either if we don't have aMode or if we have a plugin and it is running and not paused

		boxSelector->draw();

		glFlush();
        processFrameShare(GL_BACK);

		swapBuffers();// should wait for vsync...   

		if (running && running->delay > 0 && delayFPS <= 0. && delayt0 > 0. && 0==delayCtr && !paused) {
			const double tElapsed = (getTime() - delayt0);
			if (tElapsed > 0.) 	delayFPS =running->delay / tElapsed;
			else delayFPS = 120.;
			adjustRefreshToKnownPresets(delayFPS);
			Log() << "Used plugin delay to calibrate refresh rate to: " << delayFPS << " Hz";
		}
		
		/// XXX
		if (dframe) {
			Debug() << " dframe, after buf swap hwfc=" << getHWFrameCount() << ", delayCtr=" << delayCtr;
		}
		
		if (running && !paused) {
			// Do DO and AO writes immediately after the vsync..
			QString devChan; bool did_do_vsync = false;
			if (signalDIOOn && running->getParam("DO_with_vsync", devChan) && devChan != "off" && devChan.length())
				DAQ::WriteDO(devChan, true), did_do_vsync=true;
			// do pending writes specified by params set[AD]Olines,set[AD]Ostates for this frame...
			for (QVector<StimPlugin::PendingDAQWrite>::const_iterator it = running->pendingDOWrites.begin(); it != running->pendingDOWrites.end(); ++it) {
				const StimPlugin::PendingDAQWrite & w = *it;
				if (did_do_vsync && w.devChanString == devChan) 
					Warning() << "Specified to setDOline " << w.devChanString << " conflicts with DO_with_vsync line! Ignoring...";
				else DAQ::WriteDO(w.devChanString, !eqf(0.0, w.volts));
			}
			for (QVector<StimPlugin::PendingDAQWrite>::const_iterator it = running->pendingAOWrites.begin(); it != running->pendingAOWrites.end(); ++it) {
				const StimPlugin::PendingDAQWrite & w = *it;
				DAQ::WriteAO(w.devChanString, w.volts);
			}

			running->pendingDOWrites.clear();
			running->pendingAOWrites.clear();
		}
		

    } else {
		// TODO FIXME XXX.. what to do about ghosting here if we aren't swapping buffers?! HALP!!
		if (boxSelector->draw(GL_FRONT)) 
			glFlush(); // flush required when drawing to the front buffer..

            processFrameShare(GL_FRONT);

        // don't swap buffers here to avoid frame ghosts in 'A' Mode on Windows.. We get frame ghosts in Windows in 'A' mode when paused or not running because we didn't draw a new frame if paused, and so swapping the buffers causes previous frames to appear onscreen
    }
	
	
	
#ifdef Q_OS_WIN
	    //timer->start(timerpd);
	    update();
#else
	    timer->start(0);
#endif

    if (running && running->initted) {
		if (!paused) {
			running->cycleTimeLeft -= getTime()-tThisFrame;
			running->afterVSync();			
		}

		// pending param history & realtime param update support here
		if (running)
			running->doRealtimeParamUpdateHousekeeping();
    }
    
}

void GLWindow::processFrameShare(GLenum which_colorbuffer)
{
    double funcStart = -1.;
	bool doClear = false;
	if (fshare.shm) {
		fs_delay_ctr -= 1.0f;
		const bool grabThisFrame = (fs_delay_ctr <= 0.0001f);
		if (fshare.shm->enabled) {
            if (excessiveDebug) funcStart = getTime();
			const bool dfw = fshare.shm->dump_full_window;
			const unsigned w = dfw ? win_width : fs_rect.v3, h = dfw ? win_height : fs_rect.v4, sz = w*h*4;
			if (!fs_pbo[0] || fs_w != win_width || fs_h != win_height) {
				if (fs_pbo[0]) glDeleteBuffers(N_PBOS, fs_pbo);
				glGenBuffers(N_PBOS, fs_pbo);
				fs_pbo_ix = 0;	fs_q1.clear(); fs_q2.clear();
				for (int i = 0; i < N_PBOS; ++i) {
					glBindBuffer(GL_PIXEL_PACK_BUFFER, fs_pbo[i]);
					glBufferData(GL_PIXEL_PACK_BUFFER, win_width*win_height*4, 0, GL_DYNAMIC_READ);
				}
				fs_w = win_width, fs_h = win_height;
			} else if (!fs_q2.empty()) {
				for (QList<int>::const_iterator it = fs_q2.begin(); it != fs_q2.end(); ++it) {
					int ix = *it;
					bool throwaway = ix < 0;
					if (throwaway) ix = -ix;
					--ix; // offset back..
					double t0 = getTime();
					// data from last frame should be ready
					glBindBuffer(GL_PIXEL_PACK_BUFFER, fs_pbo[ix]);
					if (excessiveDebug) Debug() << "glBindBuffer of pbo# " << ix << " took: " << (getTime()-t0)*1000. << "ms";
					t0 = getTime();
					const void *fs_mem = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
					if (excessiveDebug) Debug() << "glMapBuffer of pbo# " << ix << " took: " << (getTime()-t0)*1000. << "ms";
					if (!throwaway && fs_mem && fshare.lock()) {
						fshare.shm->frame_num = fs_lastHWFC[ix];
						fshare.shm->frame_abs_time_ns = fs_abs_times[ix];
						if (excessiveDebug) pushFSTSC(fshare.shm->frame_abs_time_ns);
						fshare.shm->w = w;
						fshare.shm->h = h;
						fshare.shm->fmt = GL_BGRA;
						static const unsigned fssds(FRAME_SHARE_SHM_DATA_SIZE);
						fshare.shm->sz_bytes = fs_bytesz[ix] < fssds ? fs_bytesz[ix] : fssds;
						fshare.shm->box_x = fs_rect.x/float(win_width);
						fshare.shm->box_y = fs_rect.y/float(win_height);
						fshare.shm->box_w = fs_rect.v3/float(win_width);
						fshare.shm->box_h = fs_rect.v4/float(win_height);
						memcpy((void *)fshare.shm->data, fs_mem, fshare.shm->sz_bytes);
						fshare.unlock();
					}
					if (fs_mem) {
						t0 = getTime();
						glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
						if (excessiveDebug) Debug() << "glUnmapBuffer " << (throwaway ? "(throwaway) " : "(realgrab) ") << "of pbo#" << ix << " took: " << (getTime()-t0)*1000. << "ms";
					}
					glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // unbind...
				}
				fs_q2.clear();
			}
			if (excessiveDebug) Debug() << "FSShare: Last 2 frame FPS=" << (1.0/getFSAvgTimeLastN(2));
			fs_q2.append(fs_q1);
			fs_q1.clear();
            if (/*true*/grabThisFrame) {
				const int ix(fs_pbo_ix % N_PBOS);
				fs_lastHWFC[ix] = lastHWFC;
				fs_bytesz[ix] = sz;
				fs_abs_times[ix] = getAbsTimeNS();
				glBindBuffer(GL_PIXEL_PACK_BUFFER, fs_pbo[ix]);
				GLint bufwas;
				glGetIntegerv(GL_READ_BUFFER, &bufwas);
				glReadBuffer(which_colorbuffer);
				double t0 = getTime();
				glReadPixels(dfw?0:fs_rect.x,dfw?0:fs_rect.y,w,h,GL_BGRA,GL_UNSIGNED_BYTE,0);
                if (excessiveDebug) Debug() << "glReadPixels of pbo#" << (ix) << " took: " << (getTime()-t0)*1000. << "ms";
				glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
				glReadBuffer(bufwas);
				fs_q1.push_back(grabThisFrame ? (ix+1) : -(ix+1)); // offset up so negative takes effect
				++fs_pbo_ix;
			}
			
			//Debug() << "Entire fshare routine took: " << ((getTime()-t0)*1000.) << "ms";
		} else { // !fshare.shm->enabled
			doClear = true;
		}
		if (fshare.shm->do_box_select) {
			fshare.lock();
			fshare.shm->do_box_select = 0;
			fshare.unlock();
			fs_rect_saved = fs_rect = boxSelector->getBox();
			boxSelector->setEnabled(true);
			boxSelector->setHidden(false);
			activateWindow();
			raise();
			Log() << "SpikeGL requested a 'frame-share' clipping rectangle definition...\n";
			Log() << "Use the mouse cursor to adjust the rectangle, ENTER to accept it, or ESC to cancel."; 
		}
		if (grabThisFrame) {
			//if (excessiveDebug) Debug() << "fs_delay_ctr= " << fs_delay_ctr << ", would have written frame# " << lastHWFC;
			unsigned frl = fshare.shm->frame_rate_limit;
			if (frl > hw_refresh || !frl) frl = hw_refresh;
			fs_delay_ctr += hw_refresh/float(frl);
		}
		//if (excessiveDebug) Debug() << "fs_delay_ctr: " << fs_delay_ctr;
	} else // !fshare.shm
		doClear = true;
	if (doClear) {
		fs_pbo_ix = 0; // make sure to zero out the fs_pbo_ix always because we want to "get rid of" old/stale PBOs when user toggles enable/disable 
		fs_q1.clear();
		fs_q2.clear();		
    }
    if (excessiveDebug && funcStart > 0.) Debug() << "processFrameShare total time: " << (getTime()-funcStart)*1000. << "ms";
}

void GLWindow::copyBlinkBuf() 
{
	if (running) {
		blinkBuf.resize(width()*height()*3);
		running->readBackBuffer(blinkBuf, Vec2i(0,0), Vec2i(width(), height()), GL_UNSIGNED_BYTE);
	}
}

void GLWindow::drawBlinkBuf() 
{
	glDrawPixels(width(), height(), GL_RGB, GL_UNSIGNED_BYTE, (const void *)(blinkBuf.constData()));
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
	hw_refresh = getHWRefreshRate();
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
		
		if (!p->softCleanup) {
			delayFPS = delayt0 = 0.;	
		}
        running = 0;
        paused = false;
		hw_refresh = getHWRefreshRate();
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
#include "Movie.h"

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
	new MovingObjects();   // experiment plugin.. bouncey square, ellipses, and spheres on steroids!
    new Movie(); // plays GIF animation movies!
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

	case Qt::Key_Enter: 
	case Qt::Key_Return:
			if (!boxSelector->isHidden() && boxSelector->isEnabled()) {
				fs_rect = boxSelector->getBox();
				if (fs_rect.x >= 0 && fs_rect.y >= 0 
					&& (fs_rect.x+fs_rect.v3) <= width()
					&& (fs_rect.y+fs_rect.v4) <= height()
					&& fs_rect.v3 > 10 && fs_rect.v4 > 10) {
					Log() << "User defined new 'frame share' clipping rectangle at " << fs_rect.x << "," << fs_rect.y << " size: " << fs_rect.v3 << "x" << fs_rect.v4;
					boxSelector->setHidden(true);
					boxSelector->setEnabled(false);
					unsetCursor();
					boxSelector->saveSettings();
				} else {
					Warning() << "User-defined 'frame share' clipping rectangle is invalid or too small.. try again!";
					fs_rect = fs_rect_saved;
					boxSelector->setBox(fs_rect);
				}
			}
			break;
	case Qt::Key_Escape: 
		if (!boxSelector->isHidden() && boxSelector->isEnabled()) {
			// undo!
			fs_rect = fs_rect_saved;
			boxSelector->setBox(fs_rect);
			boxSelector->setHidden(true); boxSelector->setEnabled(false);
			unsetCursor();
			Warning() << "User cancelled 'frame share' clipping definition, reverting to previous rectangle.";
		} else {
			//stimApp()->unloadStim();
			QTimer::singleShot(1, stimApp(), SLOT(unloadStim())); // may end up possibly deleting this object?
		}

        break;
    }
}

void GLWindow::pauseUnpause()
{
    if (!running) return;
    paused = !paused;
    Log() << (paused ? "Paused" : "Unpaused");
    if (!paused && !running->frameNum && running->needNotifyStart
            && ((stimApp()->spikeGLNotifyParams.nloopsNotifyPerIter || running->loopCt == 0)) ) {
        if (stimApp()->spikeGLNotifyParams.enabled)
            running->notifySpikeGLAboutStart();
        else
            running->notifySpikeGLAboutParams();
    }
}

QList<QString> GLWindow::plugins() const
{
    QList<QString> ret;
    for(QList<StimPlugin *>::const_iterator it = pluginsList.begin(); it != pluginsList.end(); ++it) {
        ret.push_back((*it)->name());
    }
    return ret;
}

double GLWindow::getFSAvgTimeLastN(unsigned n_frames)
{
	if (n_frames > (unsigned)fs_frame_tscs.size()) n_frames = fs_frame_tscs.size();
	if (n_frames <= 1) return 0.;
	unsigned i = 0;
	double avg = 0.0, fact = 1.0/double(n_frames-1);
	quint64 last = 0;
	
	for (QList<quint64>::const_iterator it = fs_frame_tscs.begin(); it != fs_frame_tscs.end() && i < n_frames; ++i, ++it) {
		if (last) {
			avg += (last-(*it))/fact;
		}
		last = *it;
	}
	return avg / 1e9;
}
void GLWindow::pushFSTSC(quint64 tsc)
{
	static const int max = 120;
	fs_frame_tscs.push_front(tsc);
	if (fs_frame_tscs.size() > int(max*1.25)) fs_frame_tscs = fs_frame_tscs.mid(0,max);
}


