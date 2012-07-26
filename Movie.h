#ifndef Movie_H
#define Movie_H

#include "StimPlugin.h"
#include <QImageReader>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QSemaphore>
#include <QVector>
#include <QList>
#include "Util.h"

class GLWindow;
class BlenderThread;

/** \brief A plugin that plays a movie as the stim.  

    For now, only 8-bit GIF animation movies are supported.
 
    For a full description of this plugin's parameters, it is recommended you see the \subpage plugin_params "Plugin Parameter Documentation"  for more details.
*/ 
class Movie : public StimPlugin
{
	friend class GLWindow;
	friend class BlenderThread;

    bool loop;
    	
public:
	
    void stop(bool doSave = false, bool use_gui = false, bool softStop = false);

protected:
    Movie(); ///< can only be constructed by our friend class

    void drawFrame(); ///< from StimPlugin
    bool init(); ///< from StimPlugin
	/* virtual */ bool applyNewParamsAtRuntime();
	/* virtual */ void afterVSync(bool isSimulated = false);

	/* virtual */ void cleanup();
	
private:
	bool initFromParams(bool skipfboinit = false);
	void stopAllThreads();
	QByteArray popOneFrame();
	void drawFrameUsingDrawPixels();
	void drawFrameUsingFBOTexture();
    
	QMutex imgReaderMut;
    QImageReader imgReader;
	QSize sz;
	volatile int imgct, framect;
	
	int poppedframect;
	
	QMutex blendedFramesMut;
	QMap<int,QByteArray> blendedFrames;

	QList<QThread *> threads;
	QSemaphore sem;
	
	bool is8bit, inAfterVSync, pendingStop;
    int xoff, yoff;
	volatile bool movieEnded;
	
	
	bool initFBOs();
	void cleanupFBOs();
	bool preloadNextFrameToFBO();
	
	static const int nfbo = 2;
	GLuint fbos[nfbo], texs[nfbo];
	int fboctr;
	GLint ifmt, fmt, type, vertices[8], texCoords[8];
	bool usefbo;
};


#endif