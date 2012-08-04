#include "Util.h"
#include "GLHeaders.h"
#include "Movie.h"
#include <QFile>
#include <QMutexLocker>
#include <QWaitCondition>

#define FRAME_QUEUE_SIZE 100

static void dummySleep(int ms) 
{
	QWaitCondition sleep;
	QMutex bs;
	bs.lock();
	sleep.wait(&bs, ms); 
	bs.unlock();
}

class ReaderThread : public QThread
{
public:
	ReaderThread(Movie *m, int threadid) : QThread(m), m(m), threadid(threadid) {}
	~ReaderThread() {}
	
	volatile bool stop;
	
protected:
	void run();
private:
	Movie *m;
	int threadid;
};

Movie::Movie()
    : StimPlugin("Movie"), loop(true), imgct(0), framect(0), xoff(0), yoff(0)
{
	int nThreads = int(getNProcessors()) /*- 2*/;
	if (nThreads < 2) nThreads = 2;
	for (int i = 0; i < nThreads; ++i) {
		QThread *t = new ReaderThread(this, i+1);
		threads.push_back(t);
	}
}

bool Movie::initFromParams(bool skipfboinit)
{	
	imgReaderMut.lock();
	readFramesMut.lock();

	inAfterVSync = false;
	pendingStop = false;
	nSubFrames = ((int)fps_mode)+1;

	QString file;
	
	if ( !getParam("file", file) ) {
		Error() << "`file' parameter missing!";
		return false;
	}
	if ( !QFile::exists(file) ) {
		Error() << "movie file `" << file << "' not found!";
		return false;
	}
	if (!getParam("loop",  loop) ) loop = true;

	imgReader.setFileName(file);
	
	if (!imgReader.canRead() ) {
		Error() << "movie file error: " << imgReader.errorString();
		return false;
	}
	
	if (!imgReader.supportsAnimation() || imgReader.imageCount() <= 1) {
		Error() << "movie file not an animation!  Use an animated GIF with at least 2 frames!" ;
		return false;        
	}

	
	poppedframect = framect = imgct = 0;
	sz = imgReader.size();
	movieEnded = false;
	
	ifmt = GL_RGB;
	fmt = GL_RGBA;
	type = GL_UNSIGNED_INT_8_8_8_8_REV;
	
	if (lmargin) xoff = lmargin;
	else xoff = (width() - sz.width()) / 2;
	if (bmargin) yoff = bmargin;
	else yoff = (height() - sz.height()) / 2;
	if (xoff < 0) xoff = 0;
	if (yoff < 0) yoff = 0;

#if QT_VERSION <  0x040500
#error Movie plugin requires Qt 4.5 or newer!  Please install the latest Qt from the Nokia Qt website!
#endif
	
	is8bit = imgReader.imageFormat() == QImage::Format_Indexed8; 
	
	for (QList<QThread *>::iterator it = threads.begin(); it != threads.end(); ++it) 
		(*it)->start();

	readFrames.clear();
	sem.release(FRAME_QUEUE_SIZE);
	
	if (!skipfboinit) {
		bool usefbo = initFBOs();
		// Re-enable rendering to the window
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);	
		if (!usefbo) {
			Error() << "Movie plugin requires FBO support from the OpenGL subsystem.  Please install good OpenGL drivers that support FBO!  Aborting plugin!";
			return false;
		}
	}

	// unlock mutexes to allow threads to proceed
	imgReaderMut.unlock();
	readFramesMut.unlock();	

	if (!skipfboinit) {
		for (int k = 0; k < nSubFrames; ++k) {
			if (!preloadNextTexToFBO()) {
				Error() << "Failed to preload the first frame to the FBO -- aborting plugin!";
				return false;
			}
		}
	}

	Log() << "Movie plugin started using " << threads.count() << " threads to blend frames " << MOVIE_NUM_FBO << " FBO backbuffers.";

	return true;	
}

bool Movie::init()
{
	if (!initFromParams()) return false;

	return true;    
}

void Movie::stop(bool doSave, bool use_gui, bool softStop)
{
	if (inAfterVSync) {
		pendingStop = true;
	} else
		StimPlugin::stop(doSave, use_gui, softStop);
}


void Movie::afterVSync(bool isSimulated)
{
	(void)isSimulated;
	inAfterVSync = true;
	for (int k = 0; k < nSubFrames; ++k) {
		if (!preloadNextTexToFBO() && !pendingStop) {
			Error() << "Failed to preload a frame to the FBO -- aborting plugin!";
			stop();
		}
	}
	StimPlugin::afterVSync(isSimulated);
	inAfterVSync = false;	
}

QByteArray Movie::popOneFrame()
{
	QByteArray frame;
	int failct = 0;
	bool endedExit = false;
	while (frame.isNull() && failct < 1000) {
		readFramesMut.lock();
		if (readFrames.size() == 0 && movieEnded) {
			Log() << "Movie file " << imgReader.fileName() << " ended.";
			endedExit = true;
			readFramesMut.unlock();
			break;
		} else if (readFrames.size() == 0 || readFrames.begin().key() != poppedframect) {
			QWaitCondition sleep;
			sleep.wait(&readFramesMut, 1);   // 1 ms
			++failct;
		} else {
			frame = readFrames.begin().value();
			readFrames.erase(readFrames.begin());
			++poppedframect;
			sem.release(1);
		}
		readFramesMut.unlock();
	}
	if (endedExit) {
		stop();
		return QByteArray();
	}
	if (failct >= 1000 && frame.isNull()) {
		Error() << "INTERNAL ERROR IN MOVIE PLUGIN: could not grab a frame from the queue.  FIXME!";
		stop();
		return QByteArray();
	}
	return frame;
}
	
/// test code for 8-bit images where *WE* do the triple-fps blending ourselves..
void Movie::drawFrame()
{   
	if (pendingStop) {
		pendingStop = false;
		stop();
		return;
	}
	
	drawFrameUsingFBOTexture();
}

void Movie::stopAllThreads()
{
	for (QList<QThread *>::iterator it = threads.begin(); it != threads.end(); ++it) {
		ReaderThread *t = dynamic_cast<ReaderThread *>(*it);
		if (t) {
			t->stop = true;
		}
	}
	dummySleep(250);
	while (sem.tryAcquire(1,1)) {} // completely 0 out the semaphore	
}

/* virtual */
bool Movie::applyNewParamsAtRuntime() 
{
	stopAllThreads();
	return initFromParams(true);
}

void Movie::cleanup()
{
	stopAllThreads();
	readFrames.clear();
	cleanupFBOs();
}

bool Movie::initFBOs()
{
	memset(fbos, 0, sizeof(fbos));
	memset(texs, 0, sizeof(texs));
	fboctr = 0;
	
	if ( !glCheckFBStatus() ) {
		Error() << "`FBO' mode is selected but the implementation doesn't support framebuffer objects.";
		return false;
	}
	
	glGetError(); // clear error flag
	Log() << "`FBO' mode enabled, generating " << MOVIE_NUM_FBO << " FBO textures  (please wait)..";
	Status() << "Generating texture cache ...";
	
	stimApp()->console()->update(); // ensure message is printed
	stimApp()->processEvents(QEventLoop::ExcludeUserInputEvents); // ensure message is printed
	const double t0 = getTime();
	
	glGenFramebuffersEXT(MOVIE_NUM_FBO, fbos);
	if ( !glCheckFBStatus() ) {
		Error() << "Error after glGenFramebuffersEXT call.";
		return false;
	}
	int err;
	if ((err=glGetError())) {
		Error() << "GL Error: " << glGetErrorString(err) << " after call to glGenFramebuffersEXT";
		return false;
	}
	glGenTextures(MOVIE_NUM_FBO, texs);
	if ((err=glGetError())) {
		Error() << "GL Error: " << glGetErrorString(err) << " after call to glGenTextures";
		return false;
	}
	// initialize the off-screen VRAM-based textures
	for (int i = 0; i < MOVIE_NUM_FBO; ++i) {
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
		if ((err=glGetError())) {
			Error() << "GL Error: " << glGetErrorString(err) << " after call to glBindTexture";
			return false;
		}
		//glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		// optimization to use less RAM
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, ifmt, sz.width(), sz.height(), 0, fmt, type, NULL);
		if ((err=glGetError())) {
			Error() << "GL Error: " << glGetErrorString(err) << " after call to glTexImage2D";
			return false;
		}
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		
	}
	// setup framebuffers-to-texture association
	for (int i = 0; i < MOVIE_NUM_FBO; ++i) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbos[i]);
		if ((err=glGetError())) {
			Error() << "GL Error: " << glGetErrorString(err) << " after call to glBindFramebufferEXT";
			return false;
		}
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
								  GL_COLOR_ATTACHMENT0_EXT,
								  GL_TEXTURE_RECTANGLE_ARB, texs[i], 0);
		
		if ((err=glGetError())) {
			Error() << "GL Error: " << glGetErrorString(err) << " after call to glFramebufferTexture2DEXT";
			return false;
		}
		if ( !glCheckFBStatus() ) {
			Error() << "`FBO' error associating fbo/tex #" << i;
			// Re-enable rendering to the window
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			return false;
		}
		
	}
	
	if ( !glCheckFBStatus() ) {
		Error() << "`FBO' error after initialization.";
		// Re-enable rendering to the window
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		return false;
	}
		
	const int w = sz.width(), h = sz.height();
	
	GLint v[] = {
		xoff, yoff,
		xoff + w, yoff,
		xoff + w, yoff + h,
		xoff, yoff + h
	};
	GLint t[] = {
		0, h,
		w, h,
		w, 0,
		0, 0,
	};
	
	memcpy(vertices, v, sizeof(vertices));
	memcpy(texCoords, t, sizeof(texCoords));
	
	Log() << "FBO init completed in " << (getTime()-t0) << " seconds.";
	return true;	
}

bool Movie::preloadNextTexToFBO()
{
	double t0 = getTime(), elapsed;
	
	const int i = (++fboctr) % MOVIE_NUM_FBO;
	
	// enable rendering to the FBO
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbos[i]);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPushAttrib(GL_VIEWPORT_BIT|GL_COLOR_BUFFER_BIT);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // just to be explicit about which attachment we are drawing to in the FBO
	glViewport(0, 0, width(), height());
	// draw to off-screen texture i
	QByteArray frame = popOneFrame();
	//t0 = getTime();
	if (!frame.isNull()) {
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, ifmt, sz.width(), sz.height(), 0, fmt, type, frame.constData());
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		// at this point we have a texture with frame i living in VRAM
		// generate a mipmap for quality? nah -- mipmaps not supprted anyway for GL_TEXTURE_RECTANGLE_ARB path
		//glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
		//glGenerateMipmapEXT(GL_TEXTURE_RECTANGLE_ARB);
	}
	glPopAttrib();
	// Re-enable rendering to the window
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);	
	
	elapsed = getTime() - t0;
	
	//qDebug("preloadNextTexToFBO %g ms", elapsed*1000.0);

	return !frame.isNull();
}

void Movie::cleanupFBOs()
{
	glDeleteFramebuffersEXT(MOVIE_NUM_FBO, fbos); // it's weird.. you have to detach the texture from the FBO first, before deleting the texture itself
	glDeleteTextures(MOVIE_NUM_FBO, texs);
	memset(fbos, 0, sizeof(fbos));
	memset(texs, 0, sizeof(texs));
	// Make sure rendering to the window is on, just in case
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void Movie::drawFrameUsingFBOTexture()
{
	double t0 = getTime(), elapsed;
	
	glDisable(GL_SCISSOR_TEST);
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	
	// render our vertex and coord buffers which don't change.. just the texture changes
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	GLint saved_cmask[4];
	glGetIntegerv(GL_COLOR_WRITEMASK, saved_cmask);
	
	for (int k = 0; k < nSubFrames; ++k) {
		const int texnum = (fboctr-(nSubFrames-k)) % MOVIE_NUM_FBO;
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[texnum]);
		if (!k) {													 
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		if (fps_mode) {
			switch (color_order[k]) {
				case 'r': glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE); break;
				case 'g': glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE); break;
				case 'b': glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE); break;
			}
		}
		
		glVertexPointer(2, GL_INT, 0, vertices);
		glTexCoordPointer(2, GL_INT, 0, texCoords);
		glDrawArrays(GL_QUADS, 0, 4);
	}
	glColorMask(saved_cmask[0], saved_cmask[1], saved_cmask[2], saved_cmask[3]);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);	
	glDisable(GL_TEXTURE_RECTANGLE_ARB);	
	
	elapsed = getTime()-t0;
	
	//qDebug("drawFrameUsingFBOTexture %g ms", elapsed*1000.0);
}

void ReaderThread::run() 
{
	bool dostop = false;
	stop = false;
	
	Debug() << "reader thread " << threadid << " started.";
	
	while (!stop && !dostop) {
		if (m->sem.tryAcquire(1,250)) {
			int framenum;
			
			m->imgReaderMut.lock();
		
		
			double t0;
			
			t0 = getTime();
			
			QImage img (m->imgReader.read());
			if (img.isNull()) {
				if (m->loop && m->fps_mode) {
					//Warning() << "FPS mode is set but the movie file has a number of frames that is not a multiple of the subframe count!";
				} else {
					if (!m->movieEnded) {
						m->movieEnded = true;
					}
					//stop();
					dostop = true;
					stop = true;
					break;
				}
			}
			framenum = m->framect++;
			++m->imgct;				
			
			if (m->loop && m->imgct >= m->imgReader.imageCount()) {
				// loop it.. re-read the image, etc...
				m->imgReader.setFileName(m->imgReader.fileName());
				m->imgct = 0;					
			}
			//qDebug("imgreader %d read time %g ms, frame %d", (int)threadid, (getTime()-t0)*1000., (int)framenum);
			m->imgReaderMut.unlock();
			
			// next, copy bits to our queue...
			if (!dostop) {
				QByteArray pixels;
				pixels.resize(m->sz.width() * m->sz.height() * 4);
				
				unsigned char *p = reinterpret_cast<unsigned char *>(pixels.data());
				
				memcpy(p, img.bits(), pixels.size());
				
				m->readFramesMut.lock();
				m->readFrames[framenum] = pixels;
				m->readFramesMut.unlock();
			}
			
			//Debug() << "thread " << threadid << ", frame " << framenum << " took " << ((getTime()-t0)*1000.) << " msec to read from file;
			//qDebug("thread %d, frame %d took %g msec to read from file", (int)threadid, (int)framenum, ((getTime()-t0)*1000.));

		}
	}
	
	if (dostop) {
		// TODO signal STOP of plugin in a thread-safe manner!
	}
	
	Debug() << "blender thread " << threadid << " stopped.";

}
