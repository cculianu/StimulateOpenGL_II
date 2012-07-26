#include "Movie.h"
#include "Util.h"
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

class BlenderThread : public QThread
{
public:
	BlenderThread(Movie *m, int threadid) : QThread(m), m(m), threadid(threadid) {}
	~BlenderThread() {}
	
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
		QThread *t = new BlenderThread(this, i+1);
		threads.push_back(t);
	}
}


bool Movie::initFromParams()
{	
	QMutexLocker l1(&imgReaderMut), l2(&blendedFramesMut);
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
	
	if (lmargin) xoff = lmargin;
	else xoff = (width() - sz.width()) / 2;
	if (bmargin) yoff = bmargin;
	else yoff = (height() - sz.height()) / 2;
	if (xoff < 0) xoff = 0;
	if (yoff < 0) yoff = 0;
	is8bit = imgReader.imageFormat() == QImage::Format_Indexed8; 

	Log() << "Movie plugin using " << threads.count() << " threads to blend frames.";

	for (QList<QThread *>::iterator it = threads.begin(); it != threads.end(); ++it) 
		(*it)->start();

	blendedFrames.clear();
	sem.release(FRAME_QUEUE_SIZE);

	return true;	
}

bool Movie::init()
{
	if (!initFromParams()) return false;

	return true;    
}

void Movie::afterVSync(bool isSimulated)
{
	(void)isSimulated;

	StimPlugin::afterVSync(isSimulated);
}

QByteArray Movie::popOneFrame()
{
	QByteArray frame;
	int failct = 0;
	bool endedExit = false;
	while (frame.isNull() && failct < 1000) {
		blendedFramesMut.lock();
		if (blendedFrames.size() == 0 && movieEnded) {
			Log() << "Movie file " << imgReader.fileName() << " ended.";
			endedExit = true;
			break;
		} else if (blendedFrames.size() == 0 || blendedFrames.begin().key() != poppedframect) {
			QWaitCondition sleep;
			sleep.wait(&blendedFramesMut, 1);   // 1 ms
			++failct;
		} else {
			frame = blendedFrames.begin().value();
			blendedFrames.erase(blendedFrames.begin());
			++poppedframect;
			sem.release(1);
		}
		blendedFramesMut.unlock();
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
	double t0 = getTime(), elapsed = 0.;
	
	// Done in calling code.. glClear( GL_COLOR_BUFFER_BIT );
	glPushMatrix();
    	
    glRasterPos2i(xoff,yoff);
    
	QByteArray frame ( popOneFrame() );
	
	if (!frame.isNull()) 	
		glDrawPixels(sz.width(), sz.height(), GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, frame.constData());
	
	glPopMatrix();
	
	elapsed = 1000.0*(getTime() - t0);
	//qDebug("frame %d elapsed: %f msec", poppedframect, elapsed);
	//if (elapsed > 3.0) {
	//	Warning() << "frame " << frameNum << " draw time: " << elapsed << " msec exceeds 3ms!";
	//}
}

void Movie::stopAllThreads()
{
	for (QList<QThread *>::iterator it = threads.begin(); it != threads.end(); ++it) {
		BlenderThread *t = dynamic_cast<BlenderThread *>(*it);
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
	return initFromParams();
}

void Movie::cleanup()
{
	stopAllThreads();
	blendedFrames.clear();
}

void BlenderThread::run() 
{
	bool dostop = false;
	stop = false;
	
	Debug() << "blender thread " << threadid << " started.";
	
	while (!stop && !dostop) {
		if (m->sem.tryAcquire(1,250)) {
			// supports dual/triple fps mode!
			const int nSubFrames = ((int)m->fps_mode)+1;
			int framenum;
			QList<QImage> imgs;
			
			m->imgReaderMut.lock();
		
			// first, grab all the subframes from the reader as an atomic operation...
			for (int k = 0; k < nSubFrames; ++k) {
				
				QImage img (m->imgReader.read());
				if (img.isNull()) {
					if (m->loop && m->fps_mode) {
						Warning() << "FPS mode is set but the movie file has a number of frames that is not a multiple of the subframe count!";
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
				if (!k) framenum = m->framect++;
				++m->imgct;				
				imgs.push_back(img);
				
				if (m->loop && m->imgct >= m->imgReader.imageCount()) {
					// loop it.. re-read the image, etc...
					m->imgReader.setFileName(m->imgReader.fileName());
					m->imgct = 0;					
				}
			}
			
			m->imgReaderMut.unlock();
			
			// next, blend them...
			if (!dostop) {
				int k = 0;
				QByteArray pixels;
				pixels.resize(m->sz.width() * m->sz.height() * 4);

				double t0;
				
				t0 = getTime();
				
				for (QList<QImage>::iterator it = imgs.begin(); it != imgs.end(); ++it, ++k) {
					QImage img(*it);
					
					int pxidx(0);
					if (m->fps_mode) {
						switch (m->color_order[k]) {
							case 'r': pxidx = 0; break;
							case 'g': pxidx = 1; break;
							case 'b': pxidx = 2; break;
						}
					}
					const int w(img.width()), h(img.height());
					
					unsigned char *p = reinterpret_cast<unsigned char *>(pixels.data());
					
					for (int y = h-1; y >= 0; --y) {
						for (int x = 0; x < w; ++x, p += 4) {
							QRgb rgb = img.pixel(x,y);
							
							if (m->fps_mode) {
								if (!k) *((unsigned int *)p) = 0; // clear entire pixel if first frame in subframe set
								p[pxidx] = qGray(rgb);
							} else {
								p[0] = qRed(rgb);
								p[1] = qGreen(rgb);
								p[2] = qBlue(rgb);
							}					
							p[3] = 255;
						}
					}
				}
				//Debug() << "thread " << threadid << ", frame " << framenum << " took " << ((getTime()-t0)*1000.) << " msec to blend";
				m->blendedFramesMut.lock();
				m->blendedFrames[framenum] = pixels;
				m->blendedFramesMut.unlock();
			}
		}
	}
	
	if (dostop) {
		// TODO signal STOP of plugin in a thread-safe manner!
	}
	
	Debug() << "blender thread " << threadid << " stopped.";

}
