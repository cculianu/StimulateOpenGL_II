#include "StimPlugin.h"
#include "StimApp.h"
#include "GLWindow.h"
#include <QMessageBox>
#include <QDir>
#include <QGLContext>
#include <QTimer>
#include "StimGL_SpikeGL_Integration.h"
#include <iostream>
#include <math.h>
#include <QImageWriter>
#include "DAQ.h"

StimPlugin::StimPlugin(const QString &name)
    : QObject(StimApp::instance()->glWin()), parent(StimApp::instance()->glWin()), ftrackbox_x(0), ftrackbox_y(0), ftrackbox_w(0), 
softCleanup(false), dontCloseFVarFileAcrossLoops(false), gotNewParams(false), pluginDoesOwnClearing(false), lmargin(0), rmargin(0), bmargin(0), tmargin(0), mut(QMutex::Recursive), gasGen(1, RNG::Gasdev), ran0Gen(1, RNG::Ran0)
{
	frameVars = 0;
    needNotifyStart = true;
    initted = false;
    frameNum = 0x7fffffff;
	loopCt = 0;
    setObjectName(name);
    parent->pluginCreated(this);   
	needToSaveParamHistory = false;
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

void StimPlugin::stop(bool doSave, bool useGui, bool softStop)
{
	// Next, write to DO that we stopped...
	QString devChan;
	if (!softStop && getParam("DO_with_vsync", devChan) && devChan != "off" && devChan.length()) {
		DAQ::WriteDO(devChan, false);
	}
	
	// clear the pending param history as we are *done*
	if (!softStop) { 
		QMutexLocker l(&mut);
		pendingParamHistory.clear();
		
		// also, save the actual param history if that's turned-on
		if (stimApp()->isSaveParamHistory() && needToSaveParamHistory && paramHistory.size() > 1) {
			saveParamHistoryToFile();
		}
		needToSaveParamHistory = false;
	}
	
	softCleanup = softStop;
    endtime = QDateTime::currentDateTime();
    if (doSave) {
        saveData(useGui);
    }

	if(!dontCloseFVarFileAcrossLoops || !softCleanup)
		frameVars->finalize();

    // Notify SpikeGL via a socket.. if possible..
    if (stimApp()->spikeGLNotifyParams.enabled
		&& (!softStop ||  stimApp()->spikeGLNotifyParams.nloopsNotifyPerIter	|| loopCt+1 >= nLoops) ) {
        notifySpikeGLAboutStop();
    }
	    
    parent->pluginStopped(this);
    emit stopped();
    parent->makeCurrent();
	if (!softCleanup) {
		glClearColor(0.5, 0.5,  0.5, 1.);
		glColor4f(0.5, 0.5,  0.5, 1.);
		glClear( GL_COLOR_BUFFER_BIT );    
	}
    cleanup();
    // restore original matrices from matrix stack
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    frameNum = 0;
	loopCt = 0; // we clear it here so it's always at 0 for next restart, unless GLWindow.cpp set it to the previous counter.  Confusing!!
    initted = false;
}

bool StimPlugin::initFromParams()
{
	getParam("lmargin", lmargin);
	getParam("rmargin", rmargin);
	getParam("bmargin", bmargin);
	getParam("tmargin", tmargin);
	
	if (lmargin > 0 || rmargin > 0 || bmargin > 0 || tmargin > 0) {
		glScissor(lmargin, bmargin, width()-(rmargin+lmargin), height() - (bmargin+tmargin));
	} else {
		glScissor(0,0,width(),height());
	}
	
	// setup ft state colors initially to be all white for all states, except off where it's black
	const char *ftColorParamNames[N_FTStates] = {
		// NB: order of these MUST match the FTState enum!!
		"ftrack_track_color",  // FT_Track
		"ftrack_off_color",    // FT_Off
		"ftrack_change_color", // FT_Change
		"ftrack_start_color",  // FT_Start
		"ftrack_end_color",    // FT_End		
	};
	for (int i = 0; i < N_FTStates; ++i) {
		QString n = ftColorParamNames[i];
		QVector<double> cv;
		QString c;
		if ( ! getParam(n, cv) ) {
			// no, we didn't have a param in config file.. do default which iff Off is 0, everything else is 1.
			cv = (i == FT_Off) 
				 ? parseCSV("0,0,0")
		         : parseCSV("1,1,1");
		}
		if (cv.size() != 3) {
			if (cv.size() == 1) {
				cv.resize(3);
				cv[2] = cv[1] = cv[0];
			} else {
				QString c = (i == FT_Off ? "0,0,0" : "1,1,1");
				Warning() << "Error parsing `" << n << "', defaulting to " << c;
				cv = parseCSV(c);
			}
		}
		c = joinCSV(cv, ",");
		Debug() << "Got " << n << " of " << c;
		for (int j = 0; j < 3; ++j) {
			float thisc = cv[j];
			if (thisc >= 1.01f) // oops, they specified a value from 0->255, scale it back from 0->1
				thisc /= 255.f;
			reinterpret_cast<float *>(&ftStateColors[i])[j] = thisc;
		}
		ftAssertions[i] = false;
	}
	if (!getParam("ft_change_frame_cycle", ftChangeEvery) && !getParam("ftrack_change_frame_cycle",ftChangeEvery) 
		&& !getParam("ftrack_change_cycle",ftChangeEvery) && !getParam("ftrack_change",ftChangeEvery)) 
		ftChangeEvery = -1;
	
	// frametrack box info
	if(!getParam("ftrackbox_x" , ftrackbox_x) || ftrackbox_x < 0)  ftrackbox_x = 0;
	if(!getParam("ftrackbox_y" , ftrackbox_y) || ftrackbox_y < 0)  ftrackbox_y = 10;
	if(!getParam("ftrackbox_w" , ftrackbox_w) || ftrackbox_w < 0)  ftrackbox_w = 40;
	QString corder;
	if(!getParam("color_order", corder)) corder = "rgb";
	corder = corder.toLower();
	if (corder != "rgb" && corder != "rbg" && corder != "bgr" && corder != "brg" && corder != "grb" && corder != "gbr") {
		Error() << "color_order must be one of rgb, brg, bgr, etc..";
		return false;
	}
	b_index = corder.indexOf('b');
	r_index = corder.indexOf('r');
	g_index = corder.indexOf('g');
	memcpy(color_order, corder.toAscii(), sizeof(color_order));
	if (b_index < 0 || r_index < 0 || g_index < 0) {
		Error() << "color_order parameter that was specified is invalid!";
		return false;
	}
	
	QString fpsParm;
	if (!getParam("fps_mode", fpsParm)) {
		Log() << "fps_mode param not specified, defaulting to `single'";
		fpsParm = "single";
		fps_mode = FPS_Single;
	}
	
	if (fpsParm.startsWith("s" ,Qt::CaseInsensitive)) fps_mode = FPS_Single;
	else if (fpsParm.startsWith("d", Qt::CaseInsensitive)) fps_mode = FPS_Dual;
	else if (fpsParm.startsWith("t", Qt::CaseInsensitive)
		     ||fpsParm.startsWith("q", Qt::CaseInsensitive)) fps_mode = FPS_Triple;
	else {
		bool ok;
		int m = fpsParm.toInt(&ok);
		if (ok) {
			switch(m) {
				case FPS_Single:
				case FPS_Dual: 
				case FPS_Triple: fps_mode = (FPS_Mode)m; break;
				default:
					ok = false;
			}
		} 
		if (!ok) {
			Error() << "Invalid fps_mode param specified: " << fpsParm << ", please specify one of single, dual, or triple!";
			return false;
		}
	}
	if(!getParam( "bgcolor" , bgcolor))	bgcolor = 0.5;
	QString tmps;
	if ( getParam("clearColor", tmps) || getParam("interTrialBg", tmps) ) {
		StimApp::instance()->globalDefaults.interTrialBg = tmps;
		StimApp::instance()->saveSettings();
	}
	parent->setClearColor(StimApp::instance()->globalDefaults.interTrialBg);
	
	if (bgcolor > 1.0) bgcolor /= 255.0f; // deal with 0->255 values
	QString tmp;
	if ((getParam("quad_fps", tmp) && (tmp="quad_fps").length()) || (getParam("dual_fps", tmp) && (tmp="dual_fps").length())) {
		Error() << "Param `" << tmp << "' is deprecated.  Use fps_mode = single|dual|triple instead!";
		return false;
	}
	
	getParam( "nFrames", nFrames );
	getParam( "nLoops", nLoops );
	getParam( "delay", delay);
	
	if ( !getParam( "Nblinks", Nblinks) || !getParam("nblinks", Nblinks) ) Nblinks = 1;
	if ( Nblinks < 1 ) Nblinks = 1;	
	
	return true;
}

bool StimPlugin::start(bool startUnpaused)
{
    initted = false;
	
    parent->pluginStarted(this);

    emit started();
	if (frameVars && (!softCleanup || !dontCloseFVarFileAcrossLoops)) 
		delete frameVars, frameVars = 0;
	if (!softCleanup || !dontCloseFVarFileAcrossLoops)
		frameVars = new FrameVariables(FrameVariables::makeFileName(stimApp()->outputDirectory() + "/" + name()));
	
    parent->makeCurrent();

    // start out with identity matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    frameNum = 0;
	nLoops = 0;
	nFrames = 0;
	//loopCt = 0; // NB: don't set this here!  we need to keep this variable around for plugin restart stuff
    fps = fpsAvg = 0;
    fpsMin = 9e9;
    fpsMax = 0;
	delay = 0;
	lmargin = rmargin = bmargin = tmargin = 0;
    begintime = QDateTime::currentDateTime();
    endtime = QDateTime(); // set to null datetime
    missedFrames.clear();
    missedFrameTimes.clear();
	if (!softCleanup) {
		QMutexLocker l(&mut);
		paramHistory.clear();
		previous_params.clear();
		previous_previous_params.clear();
		if (pendingParamHistory.size() && pendingParamHistory.head().frameNum == 0) {
			// force use pending param history first params ...
			QMutexLocker l (&mut);
			params = pendingParamHistory.dequeue().params;
		}
	}
    if (!missedFrames.capacity()) missedFrames.reserve(4096);
    if (!missedFrameTimes.capacity()) missedFrameTimes.reserve(4096);	
    customStatusBarString = "";
	softCleanup = false;
	gotNewParams = false;
	
	/* Reads all params, does setup from them.  Note this is factored-out 
	   because we call it also from realtime param update code */
	if ( !initFromParams() )
		return false;
	
    blinkCt = 0;
	
	QString fvfile;
	if (getParam("frame_vars", fvfile)) {
		if (!frameVars->readInput(fvfile)) {
			Error() << "Could not parse frameVar input file: " << fvfile;
			return false;
		}
		have_fv_input_file = true;
	} else
		have_fv_input_file = false;
	
	if (!stimApp()->isSaveFrameVars()) {
		frameVars->closeAndRemoveOutput(); // suppress frame var output if disabled in GUI
	}
	
    if ( !startUnpaused ) parent->pauseUnpause();
    if (!(init())) { 
        stop(); 
        Error() << "Plugin " << name() << " failed to initialize."; 
        return false; 
    }
	
	if (initDelay()) {
		needNotifyStart = false; // suppress temporarily until initDone() runs
		QTimer::singleShot(initDelay(), this, SLOT(initDone()));
	} else {
		initDone(); 
	}
	
	parent->pluginDidFinishInit(this);

	return true;
}

void StimPlugin::initDone()
{
	initted = true;

    // Notify SpikeGL via a socket.. if possible..
    needNotifyStart = false;
    if (!parent->isPaused() && (stimApp()->spikeGLNotifyParams.nloopsNotifyPerIter || loopCt == 0)) {
        if (stimApp()->spikeGLNotifyParams.enabled) 
			notifySpikeGLAboutStart();
		else
			notifySpikeGLAboutParams();
    } else
		needNotifyStart = true;
	
	paramHistoryPush(); // first entry in history is frameNum 0's initial params
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

void StimPlugin::advanceFTState()
{
	if (nFrames && frameNum+1 >= nFrames) {
		// if we are the last frame in the loop, assert FT_End
		ftAssertions[FT_End] = true;
		Log() << "FrameTrack End asserted for frame " << frameNum;
	} else if (ftChangeEvery > 0 && frameNum && 0 == frameNum % ftChangeEvery ) {
		// if ftChangeEvery is defined, assert FT_Change every ftChangeEvery frames
		ftAssertions[FT_Change] = true;
		Log() << "FrameTrack Change asserted for frame " << frameNum;
	} else if (0 == frameNum) {
		// on frameNum == 0, always assert FT_Start
		ftAssertions[FT_Start] = true;
		Log() << "FrameTrack Start asserted for frame " << frameNum;
		/// XXX
		static double ts = getTime();
		double tnow = getTime();
		/// XXX
		if (loopCt > 0) {
			Debug() << "Time between restarts: " << (tnow - ts) << " = " << qRound(((tnow-ts)*parent->delayFPS)) << " frames.";
		}
		ts = tnow;
	}
	
	// detect first asserted ft flag, remember it, and clear them all
	int ftAsserted = -1;
	for (int i = 0; i < N_FTStates; ++i) {
		if (ftAsserted < 0 && ftAssertions[i])
			ftAsserted = i;
		ftAssertions[i] = false; // clear flags
	}
	// if an ft flag was asserted, use it, otherwise do the normal 'track' on even, 'off' on odd
	if (ftAsserted > -1) 
		currentFTState = static_cast<FTState>(ftAsserted);
	else
		currentFTState = (!(frameNum % 2)) ? FT_Track : FT_Off;
}

/* static */
void StimPlugin::drawFTBoxStatic()
{
	StimApp::GlobalDefaults & defs = stimApp()->globalDefaults;
	const int ftrackbox_w = defs.ftrackbox_w, ftrackbox_y = defs.ftrackbox_y, ftrackbox_x = defs.ftrackbox_x;
	const QString & color = defs.ftrack_off_color;
	QStringList comps = color.split(",");
	if (comps.count() >= 3) { 
		GLfloat cols[3] = { comps[0].toFloat(), comps[1].toFloat(), comps[2].toFloat() }; 
		if (ftrackbox_w) {		
			glColor3fv(reinterpret_cast<GLfloat *>(cols));
			glRecti(ftrackbox_x, ftrackbox_y, ftrackbox_x+ftrackbox_w, ftrackbox_y+ftrackbox_w);
		}
	}
}

void StimPlugin::drawFTBox()
{
//	if (!initted) return;
	if (ftrackbox_w) {		
		glColor3fv(reinterpret_cast<GLfloat *>(&ftStateColors[currentFTState]));
		glRecti(ftrackbox_x, ftrackbox_y, ftrackbox_x+ftrackbox_w, ftrackbox_y+ftrackbox_w);
	}
}

/* 

 // OLD ft code, here for historical purposes so that new code emulates its behavior, plus taking into account ft states.
 
void StimPlugin::drawFTBox()
{
	if (!initted) return;
	if (ftrackbox_w) {
		// draw frame tracking flicker box at bottom of grid
		if (!(frameNum % 2)) glColor4f(1.f, 1.f, 1.f, 1.f);
		else glColor4f(0.f, 0.f, 0.f, 1.f);
		glRecti(ftrackbox_x, ftrackbox_y, ftrackbox_x+ftrackbox_w, ftrackbox_y+ftrackbox_w);
	}
}
*/

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
    missedFrames.push_back((unsigned)frameNum);
    missedFrameTimes.push_back(cycleTimeMsecs);
}

static unsigned long dataTypeToSize(GLenum datatype) {
	unsigned long typeSize = 0;
	switch (datatype) {
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:			
			typeSize = sizeof(GLubyte); break;
		case GL_FLOAT:
			typeSize = sizeof(GLfloat); break;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
			typeSize = sizeof(GLshort); break;
		case GL_INT:
		case GL_UNSIGNED_INT:
			typeSize = sizeof(GLuint); break;
		default:
			Error() << "Unsupported datatype `" << datatype << "' in dataTypeToSize!";
    }
	return typeSize;
}

bool StimPlugin::readBackBuffer(QByteArray & dest, const Vec2i & o, const Vec2i & cs, GLenum datatype)
{
	if (int(dest.size()) < int(cs.w*cs.h*3*dataTypeToSize(datatype))) {
		Error() << "StimPugin::readBackBuffer was given a destination buffer that is too small!";
		return false;
	}
	GLint bufwas;
	glGetIntegerv(GL_READ_BUFFER, &bufwas);
	glReadBuffer(GL_BACK);
	///          orgn x,y  width of read (x,y)
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, cs.w);
	glReadPixels(o.x, o.y, cs.w, cs.h, GL_RGB, datatype, dest.data());
	glPixelStorei(GL_PACK_ALIGNMENT, 0); // set them back
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);// etc...
	glReadBuffer(bufwas);	
	return true;
}

void StimPlugin::useBGColor() const
{
	switch(fps_mode) {
		case FPS_Dual: glClearColor(0.f, bgcolor, bgcolor, 1.0); break; // dual mode has blank RED channel (RED channel is first frame)
		default: glClearColor(bgcolor, bgcolor, bgcolor, 1.0); break;
	}		
}

void StimPlugin::renderFrame() { 
	// unconditionally setup the clear color here
	useBGColor();
	if (!pluginDoesOwnClearing)
		glClear( GL_COLOR_BUFFER_BIT );
	glEnable(GL_SCISSOR_TEST);
	drawFrame(); 
	glDisable(GL_SCISSOR_TEST);
	advanceFTState();
	drawFTBox(); 
}

QStack<StimPlugin::ParamHistoryEntry> StimPlugin::rebuildOriginalParamHistory() const
{
	QStack<ParamHistoryEntry> ret(paramHistory);
	for (QQueue<ParamHistoryEntry>::const_iterator it = pendingParamHistory.begin(); it != pendingParamHistory.end(); ++it) {
		ret.push(*it);
	}
	return ret;
}

QList<QByteArray> StimPlugin::getFrameDump(unsigned num, unsigned numframes, 
										   const Vec2i & cropOrigin,
										   const Vec2i & cropSize,
										   const Vec2i & downSampleFactor,
										   GLenum datatype)
{
	QList<QByteArray> ret;
	
    if (parent->runningPlugin() != this) {
        Warning() << name() << " wasn't the currently-running plugin, stopping current and restarting with `" << name() << "' this may not work 100% for some plugins!";        
        parent->runningPlugin()->stop();
        start(false);
    } else if (num < frameNum) {
        Warning() << "Got non-increasing read of frame # " << num << ", restarting plugin and fast-forwarding to frame # " << num << " (this is slower than a sequential read).  This may not work 100% for some plugins (in particular CheckerFlicker!!)";
		QMutexLocker l(&mut);
		QStack<ParamHistoryEntry> originalHistory(rebuildOriginalParamHistory());
        stop();
		setPendingParamHistoryFromString(paramHistoryToString(name(), originalHistory));
        start(false);
    } else if (!parent->isPaused()) {
        Warning() << "StimPlugin::getFrameNum() called with a non-paused parent!  This is not really supported!  FIXME!";
    }
	// make sure crop size and crop origin params are in range
	Vec2i dsf = downSampleFactor;
	if (dsf.x <= 0) dsf.x = 1;
	if (dsf.y <= 0) dsf.y = 1;
	const bool doScaling = dsf.x != 1 || dsf.y != 1;
	Vec2 scale(1.0/dsf.x, 1.0/dsf.y);
	Vec2i o(cropOrigin.x, cropOrigin.y);
	Vec2i cs(cropSize.w, cropSize.h);
	const int w = width(), h = height();
	if (o.x <= 0) o.x = 0;
	else if (o.x > w) o.x = w;
	if (o.y <= 0) o.y = 0;
	else if (o.y > h) o.y = h;
	if (cs.w <= 0) cs.w = w;
	if (cs.h <= 0) cs.h = h;
	if (cs.w + o.x > w) cs.w = w-o.x;
	if (cs.h + o.y > h) cs.h = h-o.y;
	
	unsigned long datasize = cs.w*cs.h*3, datasize_scaled = floor(cs.w*scale.x)*floor(cs.h*scale.y)*3.; // R, G, B broken out per pix.
	const unsigned long typeSize = dataTypeToSize(datatype);
	datasize *= typeSize;
	datasize_scaled *= typeSize;
	
    if (QGLContext::currentContext() != parent->context())
        parent->makeCurrent();
    double tFrame = 1./getHWRefreshRate();
	//	unsigned long cur = 0;
    do  {
        double t0 = getTime();
        cycleTimeLeft = tFrame;
		renderFrame(); // NB: renderFrame() just does drawFrame(); drawFTBox();
        if (frameNum >= num) {
			QByteArray tmp(1, 0);
			QByteArray tmpScaled(1, bgcolor * 255.);
			try {
				tmp.resize(datasize); // set to uninitialized data
				if (doScaling) 
					tmpScaled.fill(bgcolor*255.,datasize_scaled);
			} catch (const std::bad_alloc & e) {
				Error() << "Bad_alloc caught when attempting to allocate " << datasize << " of data for the frames buffer (" << e.what() << ")";
				return ret;
			}				
			
			readBackBuffer(tmp, o, cs, datatype);
			
			if (doScaling) {
				int destIdx = 0;
				const int nlines = cs.h, ncols = cs.w;
				for (int i = dsf.y/2; i < nlines; i += dsf.y) {
					for (int j = dsf.x/2; j < ncols; j += dsf.x) {
						memcpy(tmpScaled.data() + destIdx*typeSize*3, tmp.data() + (i*ncols+j)*typeSize*3, typeSize*3);
						++destIdx;
					}
				}
				ret.push_back(tmpScaled);
			} else
				ret.push_back(tmp);
        }
        ++frameNum;
		const double elapsed = getTime()-t0;
        cycleTimeLeft -= elapsed;
        afterVSync(true);
		doRealtimeParamUpdateHousekeeping();
    } while (frameNum < num+numframes && parent->runningPlugin() == this);
    glClear(GL_COLOR_BUFFER_BIT);
    return ret;
}

void StimPlugin::notifySpikeGLAboutStart()
{
    StimApp::SpikeGLNotifyParams & p(stimApp()->spikeGLNotifyParams);
    
    StimGL_SpikeGL_Integration::Notify_PluginStart(name(), getParams(),
                                                    0,
                                                    p.hostname,
                                                    p.port,
                                                    p.timeout_ms);
    needNotifyStart = false;
}

void StimPlugin::notifySpikeGLAboutStop()
{
        StimApp::SpikeGLNotifyParams & p(stimApp()->spikeGLNotifyParams);

        StimGL_SpikeGL_Integration::Notify_PluginEnd(name(), getParams(),
                                                        0,
                                                        p.hostname,
                                                        p.port,
                                                        p.timeout_ms);
        needNotifyStart = true;
}

void StimPlugin::notifySpikeGLAboutParams()
{
	StimApp::SpikeGLNotifyParams & p(stimApp()->spikeGLNotifyParams);
	
	StimGL_SpikeGL_Integration::Notify_PluginParams(name(), getParams(),
													 0,
													 p.hostname,
													 p.port,
													 p.timeout_ms);
	needNotifyStart = false;
}

unsigned StimPlugin::initDelay(void) { return 0; }

void StimPlugin::logBackbufferToDisk() const {
    QDir dir;
    dir.setPath(stimApp()->outputDirectory());
	const Vec2i o = Vec2iZero, cs(width(), height());
	QImageWriter writer;
	writer.setFormat("png");
	writer.setFileName(dir.path() + "/" + name() + "_DebugFrame_" + QString::number(frameNum) + ".png");
	if (writer.canWrite()) {
		QByteArray data; data.resize(cs.w*cs.h*3);
		if (readBackBuffer(data, o, cs, GL_UNSIGNED_BYTE)) {
			QImage img((const unsigned char *)data.constData(), cs.w, cs.h, cs.w*3, QImage::Format_RGB888);			
			writer.write(img);
		}
	} else {
		Error() << "QImageWriter cannot open for writing: " << writer.fileName();
	}
}

QString StimPlugin::paramSuffix() const { 
	if (paramSuffixStack.empty()) return "";
	return paramSuffixStack.front();
}

void StimPlugin::paramSuffixPush(const QString & suffix) {
	paramSuffixStack.push_front(suffix);
}

void StimPlugin::paramSuffixPop() {
	if (paramSuffixStack.empty()) return;
	paramSuffixStack.pop_front();
}

// specialization for strings
template <> bool StimPlugin::getParam<QString>(const QString & name, QString & out) const
{        
	QString suffix = paramSuffix();
	QMutexLocker l(&mut);
	
	/// we can assume caller knows what he/she is doing, so save param type we inferred now..
	paramTypes[name+suffix] = PT_String;
	paramTypes[name] = PT_String;
	
	StimParams::const_iterator it;
	for (it = params.begin(); it != params.end(); ++it)
		if (QString::compare(it.key(), name + suffix, Qt::CaseInsensitive) == 0)
			break;
	
	if (it != params.end()) { // found it!
		out = (*it).toString().trimmed();
		return true;
	}
	return false;
}

// specialization for QVector of doubles -- a comma-separated list
template <> bool StimPlugin::getParam<QVector<double> >(const QString & name, QVector<double> & out) const
{
	QString s;
	bool b = getParam(name, s);
	if (b) out=parseCSV(s);
	// save param type
	paramTypes[(name+paramSuffix()).toLower()] = PT_DoubleVector;
	paramTypes[name.toLower()] = PT_DoubleVector;
	return b;
}

template <> bool StimPlugin::getParam<double>(const QString & name, double & out) const
{	
	bool ret = getParam_Generic(name, out);
	paramTypes[name.toLower()] = PT_Double;
	paramTypes[(name+paramSuffix()).toLower()] = PT_Double;
	return ret;
}
template <> bool StimPlugin::getParam<float>(const QString & name, float & out) const
{
	double o = out;
	bool ret = getParam(name, o);
	out = o;
	return ret;
}
template <> bool StimPlugin::getParam<int>(const QString & name, int & out) const
{
	bool ret = getParam_Generic(name, out);
	paramTypes[name.toLower()] = PT_Int;
	paramTypes[(name+paramSuffix()).toLower()] = PT_Int;
	return ret;	
}
template <> bool StimPlugin::getParam<unsigned>(const QString & name, unsigned & out) const
{
	bool ret = getParam_Generic(name, out);
	paramTypes[name.toLower()] = PT_Int;
	paramTypes[(name+paramSuffix()).toLower()] = PT_Int;
	return ret;	
}
template <> bool StimPlugin::getParam<long>(const QString & name, long & out) const
{
	bool ret = getParam_Generic(name, out);
	paramTypes[name.toLower()] = PT_Int;
	paramTypes[(name+paramSuffix()).toLower()] = PT_Int;
	return ret;	
}

void StimPlugin::waitForInitialization() const {
	while (!initted) {
		stimApp()->processEvents(QEventLoop::WaitForMoreEvents|QEventLoop::ExcludeUserInputEvents|QEventLoop::ExcludeSocketNotifiers);
	}
}

/* virtual */
bool StimPlugin::applyNewParamsAtRuntime() { return true; }

bool StimPlugin::applyNewParamsAtRuntime_Base() 
{
	return initFromParams();
}

void StimPlugin::normalizeParamVals(const QString & n, QString & v1, QString & v2) const
{
	bool wasNum = false;
	switch(paramTypes[n.toLower()]) {
		case PT_Int: {
			// this is to regularize the doubles so they are sorta equal as strings..
			long l = v1.toLong(), l2 = v2.toLong();
			v1 = QString::number(l), v2 = QString::number(l2);
			wasNum = true;
		}
			break;
		case PT_Double: {
			// this is to regularize the doubles so they are sorta equal as strings..
			double d = v1.toDouble(), d2 = v2.toDouble();
			v1 = QString::number(d), v2 = QString::number(d2);
			wasNum = true;
		}
			break;
		case PT_DoubleVector: { 
			// this is to regularize the doubles so they are sorta equal as strings..
			QVector<double> dv1 = parseCSV(v1), dv2 = parseCSV(v2);
			v1 = joinCSV(dv1);
			v2 = joinCSV(dv2);
		}
			break;
		default:
			(void) 0;
			break;
	}
	v1 = v1.trimmed();
	v2 = v2.trimmed();
	if (wasNum) {
		while (v1.endsWith(".")) v1 = v1.left(v1.length()-1);
		while (v2.endsWith(".")) v2 = v2.left(v2.length()-1);
	}
}

/// Do a diff of params and previous_params and return a map of all the params that changed (note a newly-missing param or a param in new but not in old also is considered to have 'changed')
StimPlugin::ChangedParamMap StimPlugin::paramsThatChanged() const
{
	QMutexLocker l(&mut);
	ChangedParamMap ret;
	StimParams::const_iterator it, it2;
	// check for params in old but not in new, or params in old and in new but that aren't equal
	for (it = previous_params.begin(); it != previous_params.end(); ++it) {
		const QString k = it.key().toLower();
		QString v (it.value().toString());
		bool found = false;
		for (it2 = params.begin(); it2 != params.end(); ++it2) {
			const QString k2 = it2.key().toLower();
			if (k == k2) {
				found = true;
				QString  vnew(it2.value().toString());
				normalizeParamVals(k, v, vnew);
				if (vnew != v) ret[k] = OldNewPair(v, vnew);
			}
		}
		if (!found) {
			ret[it.key()] = OldNewPair(v, QString::null);
		}
	}
	// check for params in new but not in old, or params in old and in new but that aren't equal
	for (it = params.begin(); it != params.end(); ++it) {
		const QString k = it.key().toLower();
		QString vnew (it.value().toString());
		bool found = false;
		for (it2 = previous_params.begin(); it2 != previous_params.end(); ++it2) {
			const QString k2 = it2.key().toLower();
			if (k == k2) {
				found = true;
				QString  v(it2.value().toString());
				normalizeParamVals(k, v, vnew);
				if (vnew != v) ret[k] = OldNewPair(v, vnew);
			}
		}
		if (!found) {
			ret[it.key()] = OldNewPair(QString::null, vnew);
		}
	}
	return ret;
}

void StimPlugin::paramHistoryPush(bool lock) 
{ 
	if (lock) mut.lock();
	ParamHistoryEntry h; 
	h.frameNum = getNextFrameNum();
	h.params = params; 
	h.changedParams = paramsThatChanged();
	paramHistory.push(h);
	if (lock) mut.unlock();
}

void StimPlugin::paramHistoryPop()
{
	QMutexLocker l(&mut);
	paramHistory.pop();
}

/* virtual */
void StimPlugin::newParamsAccepted()
{
	paramHistoryPush();
}

QString StimPlugin::ParamHistoryEntry::toString() const
{
	QString ret("");
    QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text);
	ts << "frameNum " << frameNum << " {\n";
	ts << "PARAMS {\n";
	ts << params.toString();
	ts << "}\n";
	ts << "CHANGED {\n";
	for (ChangedParamMap::const_iterator it = changedParams.begin(); it != changedParams.end(); ++it) {
			ts << it.key() << " = " << it.value().first << " -> " << it.value().second << "\n";
	}
	ts << "}\n";
	ts << "}\n";
	ts.flush();
    return ret;
}


bool StimPlugin::ParamHistoryEntry::fromString(const QString &s) 
{
	QString scpy(s);
	QTextStream ts(&scpy,QIODevice::ReadOnly|QIODevice::Text);
	QString dummy;
	ts >> dummy;
	if (dummy.toLower() == "framenum") {
		int fnum = -1;
		ts >> fnum;
		if (fnum < 0) return false;
		frameNum = fnum;
	}
	ts >> dummy;
	if (dummy != "{") return false;
	ts >> dummy;
	if (dummy != "PARAMS") return false;
	ts >> dummy;
	if (dummy != "{") return false;
	qint64 anchor1 = ts.pos();
	while (!ts.atEnd()) {
		ts >> dummy;
		if (dummy == "}")  break;
	}
	if (dummy == "}") {
		qint64 anchor2 = ts.pos()-1;
		ts.seek(anchor1);
		QString s = ts.read(anchor2-anchor1);
		params.fromString(s);
		ts.seek(anchor2+1);
	} else
		return false;
	ts >> dummy;
	if (dummy != "CHANGED") return false;
	ts >> dummy;
	if (dummy != "{") return false;
	changedParams.clear();
	while (!ts.atEnd()) {
		QString s = ts.readLine();
		QStringList nvp = s.split("=");
		if (nvp.count() == 2) {
			QString k = nvp.front().trimmed(), v = nvp.back().trimmed();
			QStringList oldnew = v.split("->");
			if (oldnew.count() == 2) {
				QString o = oldnew.front().trimmed(), n = oldnew.back().trimmed();
				Debug() << "Parsed changed: " << k << " = " << o << " -> " << n;
				changedParams[k] = OldNewPair(o,n);
			}
		}
		if (s.trimmed() == "}") break;
	}
	return true;
}

QString StimPlugin::paramHistoryToString(const QString & pluginName, const QVector<ParamHistoryEntry> & h)
{
	QString ret("");
    QTextStream ts(&ret, QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text);	
	ts << "PLUGIN " << pluginName << "\n";
	for (int i = 0; i < h.size(); ++i) 
		ts << h[i].toString();
	ts.flush();
	return ret;
}

QString StimPlugin::paramHistoryToString() const
{
	QMutexLocker l(&mut);
	return paramHistoryToString(name(), paramHistory);
}

void StimPlugin::setPendingParamHistoryFromString(const QString &s)
{
    if (parent->runningPlugin() == this)  {
		Error() << "Cannot set param history on a running plugin!";
		return;
	}
	QStack<ParamHistoryEntry> h;
	QString pluginName;
	if (!parseParamHistoryString(pluginName, h, s)) {
		Error() << "Parse error: param history not applied.";
		h.clear();
	}
	if (pluginName.length() && pluginName.toLower() != name().toLower()) {
		Error() << "Param history parsed ok, but it refers to a different plugin than " << name() << "!  Not applying!";
		return;
	}
	//Debug() << "Parsed param history, re-stringified it is:\n" << paramHistoryToString(h);
	setPendingParamHistory(h);
}

void StimPlugin::setPendingParamHistory(const QVector<ParamHistoryEntry> & h) 
{
	QMutexLocker l(&mut);
	*static_cast<QVector<ParamHistoryEntry> *>(&paramHistory) = h;
    *static_cast<QList<ParamHistoryEntry> *>(&pendingParamHistory) = QList<ParamHistoryEntry>::fromVector(h);
	previous_params = previous_previous_params = StimParams();
	if (pendingParamHistory.size() && pendingParamHistory.head().frameNum == 0)
		params = pendingParamHistory.head().params;	
}

bool StimPlugin::parseParamHistoryString(QString & pluginName, QVector<ParamHistoryEntry> & h, const QString & s)
{
	h.clear();
	pluginName = "";
	if (s.isNull() || !s.length()) return true;
	QString scpy(s), dummy;
	QTextStream ts(&scpy,QIODevice::ReadOnly|QIODevice::Text);
	
	// "header": PLUGIN pluginName 
	ts >> dummy;
	if (ts.atEnd()) return true;
	if (dummy.toUpper() != "PLUGIN") {
		Error() << "PARSE ERROR: Cannot parse param history string, expected PLUGIN!";
		return false;
	}
	ts >> dummy;
	if (ts.atEnd()) return false;
	pluginName = dummy;
	
	while (!ts.atEnd()) {
		qint64 anchor1 = ts.pos();
		ts >> dummy;
		if (ts.atEnd()) break;
		if (dummy.toLower() == "framenum") {
			int fnum = -1;
			ts >> fnum;
			if (fnum < 0) {
				Error() << "PARSE ERROR: Cannot parse param history string, expected a number after frameNum!";
				return false;
			}
			if (h.isEmpty() && fnum) {
				Error() << "ERROR: In param history string, expected frameNum 0 for first entry!";				
			}
			ts >> dummy;
			if (dummy != "{") {
				Error() << "PARSE ERROR: Expected '{' after frameNum #!\n";
			}
			int bracect = 1;
			while (!ts.atEnd() && bracect > 0) {
				ts >> dummy;
				if (dummy == "{") ++bracect;
				else if (dummy == "}") --bracect;
			}
			qint64 anchor2 = ts.pos();
			ts.seek(anchor1);
			QString entryStr = ts.read(anchor2-anchor1);
			ParamHistoryEntry e;			
			if (!e.fromString(entryStr)) {
				Error() << "PARSE ERROR: Could not parse param history entry labeled 'frameNum " << fnum << "'";
				return false;
			}
			h.push_back(e);
			/*anchor1 = ts.pos();
			dummy = "";
			ts >> dummy;
			if (dummy == "}") 
				break; // end of entry
			ts.seek(anchor1);*/
		} else {
			Error() << "PARSE ERROR: Cannot parse param history string, expected frameNum, got: " << dummy;
			return false;
		}
	}
	return true;
}

// virtual
void StimPlugin::checkPendingParamHistory()
{
	QMutexLocker l (&mut);
	if (pendingParamHistory.size() && pendingParamHistory.head().frameNum == frameNum) {
		setParams(pendingParamHistory.dequeue().params);
	}
}

void StimPlugin::doRealtimeParamUpdateHousekeeping()
{
	
	// pending param history support here -- dequeues queued params at appropriate times
	checkPendingParamHistory();
	
#pragma mark Realtime param support here
	// realtime param update support HERE
	if (gotNewParams) {
		if ( !applyNewParamsAtRuntime_Base() || !applyNewParamsAtRuntime() ) {
			// restore previous params...
			mut.lock();
			params = previous_params;
			previous_params = previous_previous_params;
			applyNewParamsAtRuntime_Base() && applyNewParamsAtRuntime();
			mut.unlock();
		} else
			newParamsAccepted();
		gotNewParams = false;
	}
}

void StimPlugin::saveParamHistoryToFile() const
{
	QString fn = Util::makeUniqueFileName(stimApp()->outputDirectory() + "/" + name() + "_ParamHist", "txt");
	QFile f(fn);
	if (f.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
		QString str = paramHistoryToString();
		if (str.length()) {
			qint64 n = f.write(str.toUtf8());
			if (n <= 0) {
				Error() << "Write error writing param history file " << fn;
			} else {
				Log() << "Wrote " << n << " bytes to param history file " << fn;
			}
		}
	} else {
		Error() << "Could not open param history file for write: " << fn;
	}	
}

/// Called by StimApp loadStim() to determine how to parse a stim file. Returns true for nonzero length files with header PLUGIN
/*static*/ bool StimPlugin::fileAppearsToBeValidParamHistory(const QString & filename) 
{
	QFile f(filename);
	if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QTextStream ts(&f);
		if (!ts.atEnd() && ts.readLine().toUpper().startsWith("PLUGIN "))
			return true;
	}
	return false;
}
