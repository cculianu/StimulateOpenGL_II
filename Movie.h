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
#include <QCache>

class GLWindow;
class ReaderThread;
class QProgressDialog;
class FMVChecker;

/** \brief A plugin that plays a movie as the stim.  

    For now, only 8-bit GIF animation movies are supported.
 
    For a full description of this plugin's parameters, it is recommended you see the \subpage plugin_params "Plugin Parameter Documentation"  for more details.
*/ 
class Movie : public StimPlugin
{
	Q_OBJECT
	
	friend class GLWindow;
	friend class ReaderThread;
    	
public:
	
    void stop(bool doSave = false, bool use_gui = false, bool softStop = false);
	
protected:
    Movie(); ///< can only be constructed by our friend class

    void drawFrame(); ///< from StimPlugin
    bool init(); ///< from StimPlugin
	/* virtual */ bool applyNewParamsAtRuntime();
	/* virtual */ void afterVSync(bool isSimulated = false);

	/* virtual */ void cleanup();

private slots:
	void checkFMV(const QString & fmvfile);
	void fmvChkError(FMVChecker *f, const QString & filename, const QString & err);
	void fmvChkDone(FMVChecker *);
	void fmvChkCanceled(FMVChecker *);
	
private:
	bool initFromParams(bool skipfboinit = false);
	void stopAllThreads();
	QByteArray popOneFrame();
	void drawFrameUsingFBOTexture();

    QString file;
	int animationNumFrames;
	QSize sz;
	volatile int imgct, framect, loopsleft;
	bool loopforever;
	
	int poppedframect;
	
	QMutex readFramesMutex;
	QMap<int,QByteArray> readFrames;

	QList<QThread *> threads;
	QSemaphore sem;
	
	bool inAfterVSync, pendingStop, drewAFrame;
    int xoff, yoff;
	volatile bool movieEnded;
	
	bool initFBOs();
	void cleanupFBOs();
	bool preloadNextTexToFBO();

#define MOVIE_NUM_FBO 6
	GLuint fbos[MOVIE_NUM_FBO], texs[MOVIE_NUM_FBO];
	unsigned char fboctr;
	GLint ifmt, fmt, type, vertices[8];
	GLdouble texCoords[8];
	int nSubFrames;

	QCache<int,QByteArray> imgCache;
};


class FMVChecker : public QThread
{
	Q_OBJECT
public:
	FMVChecker(QObject *parent, Movie *m, const QString & f);
	~FMVChecker();
	
	bool checkOk;
	QString file;
	QProgressDialog *pd;
	QString errMsg;

	virtual QString what();

protected:
	void run();
signals:
	void errorMessage(FMVChecker *, const QString &, const QString &);
	void progress(int pct);
	void done(FMVChecker *);
	void canceled(FMVChecker *);
	
protected:	
	static void errCB(void *fmvinstance, const std::string &msg);
	void err(const QString &);
	static bool progCB(void *fmvinstance, int prog);
	bool prog(int);
	
};


class FMVRepairer : public FMVChecker
{
public:
	FMVRepairer(QObject *parent, Movie *m, const QString & f);
	
	QString what();

protected:
	void run();		
};

#endif
