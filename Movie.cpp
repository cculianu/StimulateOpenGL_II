#include "Util.h"
#include "GLHeaders.h"
#include "Movie.h"
#include <QFile>
#include <QMutexLocker>
#include <QWaitCondition>
#include "GifReader.h"

#define FRAME_QUEUE_SIZE 100
#define IMAGE_CACHE_SIZE 100*1024*1024 /* 100MB image cache! */

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
	ReaderThread(Movie *m, int threadid) : QThread(m), reader(0), m(m), threadid(threadid) {}
	~ReaderThread() { delete reader; reader = 0; }
	
	volatile bool stop;
	
	GifReader *reader;
	QFile iodevice;
	
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
	readFramesMutex.lock();

	inAfterVSync = false;
	pendingStop = false;
	drewAFrame = false;
	nSubFrames = ((int)fps_mode)+1;
	imgCache.clear();
	imgCache.setMaxCost(IMAGE_CACHE_SIZE);
	
	if ( !getParam("file", file) ) {
		Error() << "`file' parameter missing!";
		return false;
	}
	if ( !QFile::exists(file) ) {
		Error() << "movie file `" << file << "' not found!";
		return false;
	}
	if (!getParam("loop",  loop) ) loop = true;

	QFile f (file);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen()) {
		Error() << "movie file error, cannot open: " << file;
		return false;
		
	}
	GifReader gr;
	gr.setDevice(&f);
	
	if (!gr.canRead() ) {
		Error() << "movie file error: cannot read input file";
		return false;
	}

	if (gr.imageCount() < 2) {
		Error() << "movie file not an animation!  Use an animated GIF with at least 2 frames!" ;
		return false;        		
	}
	
	if (!gr.isAnimatedGifNonOptimized()) { // scan file..
		Error() << "Input movie is an optimized GIF. (This plugin only supports non-optimized GIFs for fast reading.)  Use the @GifWriter Matlab class to generate non-optimized GIFs for input to this plugin!";
		return false;
	}
	
	unsigned long long memsize = getHWPhysMem();
	if (static_cast<unsigned long long>(imgCache.maxCost()) > memsize/2ULL) {
		Debug() << "Image cache size is too big for physical memory; shrinking to 1/2 of RAM!";
		imgCache.setMaxCost(static_cast<int>(memsize / 2ULL));
	}
	Debug() << "MemSize: " << memsize << ", image cache size: " << imgCache.maxCost();
	
	poppedframect = framect = imgct = 0;
	sz = gr.size();
	animationNumFrames = gr.imageCount();
	movieEnded = false;
	
	ifmt = GL_RGB;
	fmt = GL_LUMINANCE;
	type = GL_UNSIGNED_BYTE;
	
	if (lmargin) xoff = lmargin;
	else xoff = (width() - sz.width()) / 2;
	if (bmargin) yoff = bmargin;
	else yoff = (height() - sz.height()) / 2;
	if (xoff < 0) xoff = 0;
	if (yoff < 0) yoff = 0;

	for (QList<QThread *>::iterator it = threads.begin(); it != threads.end(); ++it) {
		ReaderThread *rt = dynamic_cast<ReaderThread *>(*it);
		if (rt) {
			delete rt->reader;
			rt->reader = new GifReader;
			rt->iodevice.close();
			rt->iodevice.setFileName(file);
			rt->iodevice.open(QIODevice::ReadOnly);
			rt->reader->setDevice(&rt->iodevice);
			rt->reader->copyImageLengthsAndOffsets(gr); // copy cached values from existing reader..
		} else {
			Error() << "INTERNAL PLUGIN ERROR -- reader thread is not of type ReaderThread!";
			return false;
		}
		(*it)->start();
	}

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

	// unlock mutex to allow threads to proceed
	readFramesMutex.unlock();	

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
	if (!drewAFrame) return;
	inAfterVSync = true;
	bool preloadOk = true;
	for (int k = 0; preloadOk && k < nSubFrames; ++k) {
		preloadOk = preloadNextTexToFBO();
		if ( !preloadOk && !pendingStop) {
			Error() << "Failed to preload a frame to the FBO -- aborting plugin!";
			stop();
		}
	}
	StimPlugin::afterVSync(isSimulated);
	inAfterVSync = false;	
	drewAFrame = false;
}

QByteArray Movie::popOneFrame()
{
	QByteArray frame;
	int failct = 0;
	bool endedExit = false;
	while (frame.isNull() && failct < 1000) {
		readFramesMutex.lock();
		if (readFrames.size() == 0 && movieEnded) {
			Log() << "Movie file " << file << " ended.";
			endedExit = true;
			readFramesMutex.unlock();
			break;
		} else if (readFrames.size() == 0 || readFrames.begin().key() != poppedframect) {
			QWaitCondition sleep;
			sleep.wait(&readFramesMutex, 1);   // 1 ms
			++failct;
		} else {
			frame = readFrames.begin().value();
			readFrames.erase(readFrames.begin());
			++poppedframect;
			sem.release(1);
		}
		readFramesMutex.unlock();
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
	drewAFrame = true;
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
	imgCache.clear();
	cleanupFBOs();
}

bool Movie::initFBOs()
{
	memset(fbos, 0, sizeof(fbos));
	memset(texs, 0, sizeof(texs));
	fboctr = nSubFrames;
	
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
	
	const int i = unsigned(fboctr++) % MOVIE_NUM_FBO;
	
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
		const int texnum = (unsigned(fboctr)-(nSubFrames-k)) % MOVIE_NUM_FBO;
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
	stop = false;
	
	Debug() << "reader thread " << threadid << " started.";
	
	QImage img(m->sz.width(), m->sz.height(), QImage::Format_RGB32);
	
	while (!stop) {
		if (m->sem.tryAcquire(1,250)) {
			int framenum;
					
		
			double t0;
			
			t0 = getTime();
			
			m->readFramesMutex.lock();
			if (m->imgct >= m->animationNumFrames) {
				if (m->loop)
					m->imgct = 0;
				else
					m->movieEnded = true;
			}
			int imgct = m->imgct++;		
			framenum = m->framect++;
			bool movieEnded = m->movieEnded;
			QByteArray cachedImg;
			if (m->imgCache.contains(imgct))
				cachedImg = *(m->imgCache.object(imgct));
			m->readFramesMutex.unlock();
			
			if (movieEnded) {
				stop = true;
				break;
			}
			
			if (!cachedImg.isNull()) {
				
				m->readFramesMutex.lock();
				m->readFrames[framenum] = cachedImg;
				m->readFramesMutex.unlock();
				
			} else {
				// img not in cache, so read it from the disk file and enqueue it, and also cache it
				bool readok = false;
				
				// jump the reader to current image
				readok=reader->randomAccessRead(&img, imgct+1);
				
				// next, copy bits to our queue...
				if (readok) {
					QByteArray *pixels = new QByteArray; // it's ok, the imgCache below will own and auto-delete this object when it goes out-of-cache..
					pixels->resize(img.byteCount());
					
					unsigned char *p = reinterpret_cast<unsigned char *>(pixels->data());
					
					memcpy(p, img.bits(), pixels->size());
					
					m->readFramesMutex.lock();
					m->readFrames[framenum] = *pixels; // shallow copy..
					m->imgCache.insert(imgct, pixels, pixels->size()); // img cache owns object, will delete when emptying..
					m->readFramesMutex.unlock();
				}
				
			}
			
			//qDebug("thread %d, frame %d imgnr %d took %g msec to read from %s", (int)threadid, (int)framenum, imgct, ((getTime()-t0)*1000.),				   cachedImg.isNull() ? "file" : "cache");

		}
	}
	delete reader;
	reader = 0;
	Debug() << "reader thread " << threadid << " stopped.";

}
