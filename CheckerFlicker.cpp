#define GL_GLEXT_PROTOTYPES
#include <QTime>
#include <QSemaphore>
#include <QMutex>
#include <deque>
#include <QThread>
#include <QTimer.h>
#ifdef Q_OS_WIN
#define __SSE2__
#include <windows.h>
#include <wingdi.h>
#endif
#ifdef Q_WS_MACX
#  include <gl.h>
#  include <glext.h>
#else
#  include <GL/gl.h>
#  ifndef Q_OS_WIN
#  include <GL/glext.h>
#  endif
#endif
#include "CheckerFlicker.h"
#include "ZigguratGauss.h"

/* These #defines are needed because the stupid MinGW headers are 
   OpenGL 1.1 only.. but the actual Windows lib has OpenGL 2               */
#ifndef GL_BGRA
#define GL_BGRA                           0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR                            0x80E0
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#endif
#ifndef GL_UNSIGNED_BYTE_3_3_2
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#endif
#ifndef GL_UNSIGNED_BYTE_2_3_3_REV
#define GL_UNSIGNED_BYTE_2_3_3_REV        0x8362
#endif
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB          0x84F5
#endif
#ifndef GL_BUFFER_SIZE_ARB 
#define GL_BUFFER_SIZE_ARB                0x8764
#endif
#ifdef Q_OS_WIN /* Hack for now to get windows to see the framebuffer ext stuff */
#if defined(__GNUC__ ) || defined(_MSC_VER)
#ifndef GLAPI
#define GLAPI static
#endif
#define GL_MAX_COLOR_ATTACHMENTS_EXT      0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT          0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT          0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT          0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT          0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT          0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT          0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT          0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT          0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT          0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT         0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT         0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT         0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT         0x8CED
#define GL_COLOR_ATTACHMENT14_EXT         0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT         0x8CEF
#define GL_FRAMEBUFFER_EXT                0x8D40
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT    0x8CDD
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
// extern "C" {
// GLAPI void APIENTRY glDeleteFramebuffersEXT (GLsizei, const GLuint *);
// GLAPI void APIENTRY glGenFramebuffersEXT (GLsizei, GLuint *);
// GLAPI GLenum APIENTRY glCheckFramebufferStatusEXT (GLenum);
// GLAPI void APIENTRY glFramebufferTexture2DEXT (GLenum, GLenum, GLenum, GLuint, GLint);
// GLAPI void APIENTRY glGenerateMipmapEXT (GLenum);
// GLAPI void APIENTRY glBindFramebufferEXT (GLenum, GLuint);
// }
GLAPI void APIENTRY glDeleteFramebuffersEXT (GLsizei s, const GLuint *a)
{
    typedef void (APIENTRY *Fun_t)(GLsizei, const GLuint *);
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glDeleteFramebuffersEXT");
    if (!fun) {
        Error() << "glDeleteFramebuffersEXT not found";
    } else
        fun(s,a);
}
GLAPI void APIENTRY glGenFramebuffersEXT (GLsizei s, GLuint *a)
{
    typedef void (APIENTRY *Fun_t)(GLsizei, GLuint *); 
    static Fun_t fun = 0;
    if (!fun) fun =  (Fun_t)wglGetProcAddress("glGenFramebuffersEXT");
    if (!fun) {
        Error() << "glGenFramebuffersEXT not found";
    } else
        fun(s,a);
}
GLAPI GLenum APIENTRY glCheckFramebufferStatusEXT (GLenum e)
{
    typedef GLenum (APIENTRY *Fun_t)(GLenum);
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glCheckFramebufferStatusEXT");
    if (!fun) {
        Error() << "glCheckFramebufferStatusEXT not found";        
    } else
        return fun(e);
    return GL_INVALID_FRAMEBUFFER_OPERATION_EXT;
}
GLAPI void APIENTRY glFramebufferTexture2DEXT (GLenum a, GLenum b, GLenum c, GLuint d, GLint e)
{
    typedef void (APIENTRY *Fun_t) (GLenum a, GLenum b, GLenum c, GLuint d, GLint e);
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glFramebufferTexture2DEXT");
    if (!fun) {
        Error() << "glFramebufferTexture2DEXT not found";        
    } else
        fun(a,b,c,d,e);
}
GLAPI void APIENTRY glGenerateMipmapEXT (GLenum e)
{
    typedef void (APIENTRY *Fun_t)(GLenum);
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glGenerateMipmapEXT");
    if (!fun) {
        Error() << "glGenerateMipmapEXT not found";        
    } else
        fun(e);
}
GLAPI void APIENTRY glBindFramebufferEXT (GLenum a, GLuint e)
{
    typedef void (APIENTRY *Fun_t) (GLenum a, GLuint e); 
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glBindFramebufferEXT");
    if (!fun) {
        Error() << "glBindFramebufferEXT not found";        
    } else
        fun(a,e);
}

GLAPI void APIENTRY glGetBufferParameterivARB(GLenum a, GLenum b, GLint *i)
{
    typedef void (APIENTRY *Fun_t) (GLenum, GLenum, GLint *); 
    static Fun_t fun = 0;
    if (!fun) fun = (Fun_t)wglGetProcAddress("glGetBufferParameterivARB");
    if (!fun) {
        Error() << "glGetBufferParameterivARB not found";        
    } else
        fun(a,b,i);
}

#endif // __GNUC__
#endif // Q_OS_WIN


// SFMT based random number generator

namespace {

#ifdef __SSE2__
#define HAVE_SSE2
#else
#error Please compile with SSE2 enabled!
#endif

#define MEXP 19937
#include "SFMT.h"
#include "SFMT.c"

}

/** \brief An internal struct that encapsulates a frame generated by a FrameCreator thread.

 Frames are generated by the FrameCreator and queued, and then dequeued in
 side the mainthread in CheckerFlicker::afterVSync().*/
struct Frame
{
    Frame(unsigned w, unsigned h, unsigned elem_size);
    ~Frame() { delete [] mem; mem = 0; texels = 0; }
    GLubyte *mem; ///< this is the memory block that is an unaligned superset of texels and should the the one we delete []
    GLvoid *texels; ///< 16-bytes aligned texel area (useful for SFMT-sse2 rng)
    unsigned w, h; ///< in texels
    unsigned tx_size; ///< size of each texel in bytes
    unsigned long nqqw; ///< num of quad-quad words.. (128-bit words)

    // presently not used..
    int ifmt, fmt, type; ///< for gltexsubimage..
    unsigned int seed; ///< the rng seed used for this frame
	Vec2i displacement; ///< frame displacement in pixels in the x and y direction
};

Frame::Frame(unsigned w, unsigned h, unsigned es): mem(0), texels(0), w(w), h(h), tx_size(es), ifmt(0), fmt(0), type(0), seed(0)
{
    unsigned long sz;
    sz = MAX(w*h*es + 48, (N+3)*16); // gen_rand_array seems to require at least this much memory.. annoying.
    texels = mem = new GLubyte[sz];
    nqqw = w*h*es/sizeof(w128_t);
    if ( w*h*es%sizeof(w128_t) ) ++nqqw;
    if (((unsigned long)texels ) & 0xf)
        texels = (GLvoid *)((((unsigned long)mem)+sizeof(w128_t))&~0xf); // align to 16-bytes
}

/**
   \brief Class that encapsulates the frame creation thread(s) of CheckerFlicker.

   Frames in CheckerFlicker are created in a separate thread and enqueued.
   They are handed off as Frame objects to the main thread in CheckerFlicker::afterVSync().
   This thread basically contains some locking primitives plus a queue,
   and knows to call CheckerFlicker::genFrame each time it is requested
   to generate frames.  */
class FrameCreator : public QThread
{
public:
    FrameCreator(CheckerFlicker  & cf);
    ~FrameCreator();

    
    QSemaphore createMore,  ///< Main thread releases resources on this semaphore each time it thinks new frames are needed
        haveMore; ///< Frame creation thread releases 1 resource on this semaphore per frame that has been created, main thread then acquires resource to consule frames
    std::deque<Frame *> frames; ///< A queue of the frames that are waiting to be consumed (by the main thread)
    QMutex mut; ///< Mutex for frames queue

    Frame *popOne() { Frame *f; QMutexLocker l(&mut); f = frames.front(); frames.pop_front(); return f; }

    unsigned nWaiting() { QMutexLocker l(&mut); return frames.size(); }
protected:
    /// The frame creation thread function
    void run();
private:
    CheckerFlicker  & cf;
    volatile bool stop;
};


CheckerFlicker::CheckerFlicker()
    : StimPlugin("CheckerFlicker"), w(width()), h(height()), fbo(0), fbos(0), texs(0), origThreadAffinityMask(0)
{
}

CheckerFlicker::~CheckerFlicker()
{
    cleanupFCs();
}

static
bool checkFBStatus()
{
    int status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    switch(status)
    {
    case GL_FRAMEBUFFER_COMPLETE_EXT:
        return true;

    case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
        Error() << "FBO: unsupported error";
        return false;

    case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
        Error() << "FBO: invalid framebuffer operation";
        return false;

    default:
        Error() << "FBO: unknown error";
        return false;
    }
    return true;
}

bool CheckerFlicker::init()
{
    glGetError(); // clear error flag

	QString tmp;
	if ((getParam("blackwhite", tmp) && (tmp="blackwhite").length()) || (getParam("meanintensity", tmp) && (tmp="meanintensity").length())) {
		// reject deprecated params!
		Error() << "Checkerflicker param `" << tmp << "' is deprecated!  Please rename this param or get rid of it (check the docs!)";
		return false;
	}
	const int xdim = w, ydim = h;
	// find parameters
	// if not found, either set to default value or generate warning message and abort (=return false)
	if( !getParam("stixelwidth", stixelWidth) ) stixelWidth = 10;
	if( !getParam("stixelheight", stixelHeight) ) stixelHeight = 10;
	if( !getParam("blackwhite", blackwhite) ) blackwhite = true;

	if( !getParam("contrast", contrast) ){
		if( blackwhite ) contrast = 1;
		else contrast = 0.3f;
	}

	if( !getParam("lmargin", lmargin) ) lmargin = 0;
	if( !getParam("rmargin", rmargin) ) rmargin = 0;
	if( !getParam("bmargin", bmargin) ) bmargin = 0;
	if( !getParam("tmargin", tmargin) ) tmargin = 0;
        if( !getParam("fbo", fbo) ) fbo = 0;
        if( !fbo && !getParam("prerender", fbo) ) fbo = 0;        
        fbos = 0;
        if( !getParam("cores", nCoresMax) ) nCoresMax = 2; ///< NB: more than 2 cores not really supported yet since random number generators aren't reentrant
        unsigned colortable=1<<17;
        if (!getParam("colortable", colortable)) {
            // force default of 2^17 colortable size or about 130KB
            colortable = 1U<<17;
            Warning() << "`colortable' not specified in configuration file, defaulting to colortable=" << colortable;
        }
        if (colortable <= 24 && colortable > 11) colortable = (1<<colortable);
        if (colortable <= 2048 || (colortable&(colortable-1))) {
            Error() << "`colortable' parameter of " << colortable << " is invalid.  Try a power of two >2048 (or a number <=24 to auto-compute a power of 2).";
            return false;
        }
        gaussColorMask = colortable-1; // all 1's before the colorTable..
        
	// make sure that positive values are used
	if( stixelWidth < 0 ) stixelWidth *= -1;
	if( stixelHeight < 0 ) stixelHeight *= -1;
	if( stixelWidth < 1 ) stixelWidth = 1;
	if( stixelHeight < 1 ) stixelHeight = 1;
	if( !getParam( "seed", originalSeed) ) originalSeed = 10000;

	ran1Gen.reseed(originalSeed); // NB: it doesn't matter anymore if seed is negative or positive -- all negative seeds end up being positie anyway and the generator no longer needs a negative seed to indicate "reseed".  That was ugly.  See RanGen.h for how to use the class.. 
    gasGen.reseed(originalSeed); // Need to reseed this too.. 
    // our SFMT random number generator gets seeded too
    init_gen_rand(originalSeed);
    gen_rand_all();

	if( !getParam( "Nblinks", Nblinks) ) Nblinks = 1;
        blinkCt = 0;
        nConsecSkips = 0;
	if( Nblinks < 1 ) Nblinks = 1;

        lastAvgTexSubImgProcTime = 0.0045;

	if (!getParam("rand_displacement_x", rand_displacement_x)) rand_displacement_x = 0;
	if (!getParam("rand_displacement_y", rand_displacement_y)) rand_displacement_y = 0;

	if (rand_displacement_x && (!lmargin && !rmargin)) {
		Warning() << "rand_displacement_x set to: " << rand_displacement_x << " -- Recommend setting lmargin and rmargin to " << -int(rand_displacement_x);
	}
	if (rand_displacement_y && (!tmargin && !bmargin)) {
		Warning() << "rand_displacement_y set to: " << rand_displacement_y << " -- Recommend setting tmargin and bmargin to " << -int(rand_displacement_y);
	}
		
	// determine the right number of tiles in x and y direction to fill screen        
        do {
            xpixels = xdim-(lmargin+rmargin);
            ypixels = ydim-(bmargin+tmargin);
            if (xpixels < 0 || ypixels < 0) {
                Error() << "Parameters are such that there is no stixel area!";
                return false;
            }
            Nx = xpixels/stixelWidth;
            Ny = ypixels/stixelHeight;
            if( xpixels%stixelWidth ) {
                rmargin += xpixels-(Nx*stixelWidth);
            }
            if( ypixels%stixelHeight ) {
                tmargin += ypixels-(Ny*stixelHeight);            
            }
        } while ( xpixels%stixelWidth || ypixels%stixelHeight );

        if (!fbo) {
            Warning() << "Prerender/fbo not specified -- defaulting to prerender setting of 30 -- Please look into using either `fbo' or `prerender' as a configuration parameter.";
            fbo = 30;            
        }
        
        if (fbo < 3) {
            Warning() << "Specified <3 prerender frames!  Probably need more than this to be robust.. forcing 3.";
            fbo = 3;
        }

        ifmt = GL_RGB;
        fmt = GL_BGRA;
        type = GL_UNSIGNED_INT_8_8_8_8_REV;
        initted = false;
		frameGenAvg_usec;

        if (bgcolor < 0. || bgcolor > 1. || contrast < 0. || contrast > 1.) {
            Error() << "Either one of: `bgcolor' or `contrast' is out of range.  They must be in the range [0,1]";
            return false;
        }

		unsigned nProcs;
		if ((nProcs=getNProcessors()) > 2) {
			const unsigned mask = 0x1<<(nProcs-3);
			origThreadAffinityMask = setCurrentThreadAffinityMask(mask); // make it run on second cpu
			Log() << "Set affinity mask of main thread to: " << mask;
		}

        QTime tim; tim.start();
        Log() << "Pregenerating gaussian color table of size " << colortable << ".. (please be patient)";
        Status() << "Generating gaussian color table ...";
//        stimApp()->console()->update(); // ensure message is printed
        //stimApp()->processEvents(QEventLoop::ExcludeUserInputEvents); // ensure message is printed
        genGaussColors();
        Log() << "Generated " << gaussColors.size() << " colors in " << tim.elapsed()/1000.0 << " secs";
        
        // fastest, not as compatible on some boards
        if (!initFBO()) {
            Error() << "FBO initialization failed -- possibly due to lack of support on this hardware or a misconfiguration.";
            Error() << "*FBO MODE IS REQUIRED FOR THIS PLUGIN TO WORK*";
            return false;
        }        
        frameNum = 0; // reset frame num!

#ifdef Q_OS_WIN
		Sleep(500); // sleep for 500ms to ensure init is ok
#endif

	return true;
}

bool CheckerFlicker::initFBO()
{
            if ( !checkFBStatus() ) {
                Error() << "`FBO' mode is selected but the implementation doesn't support framebuffer objects.";
                return false;
            }

            glGetError(); // clear error flag
            Log() << "`FBO' mode enabled, generating " << fbo << " FBO textures  (please wait)..";
            Status() << "Generating texture cache ...";

            stimApp()->console()->update(); // ensure message is printed
            stimApp()->processEvents(QEventLoop::ExcludeUserInputEvents); // ensure message is printed
            QTime tim;
            tim.start();

            cleanupFCs();
            FrameCreator *fc = new FrameCreator(*this);
            fcs.push_back(fc);
            fc->start();

            fbos = new GLuint[fbo];
            memset(fbos, 0, sizeof(GLuint) * fbo);
            texs = new GLuint[fbo];
            memset(texs, 0, sizeof(GLuint) * fbo);
			disps = new Vec2i[fbo];
			memset(disps, 0, sizeof(Vec2i) * fbo);

            glGenFramebuffersEXT(fbo, fbos);
            if ( !checkFBStatus() ) {
                Error() << "Error after glGenFramebuffersEXT call.";
                return false;
            }
            int err;
            if ((err=glGetError())) {
                Error() << "GL Error: " << glGetErrorString(err) << " after call to glGenFramebuffersEXT";
                return false;
            }
            glGenTextures(fbo, texs);
            if ((err=glGetError())) {
                Error() << "GL Error: " << glGetErrorString(err) << " after call to glGenTextures";
                return false;
            }
            // initialize the off-screen VRAM-based textures
            for (unsigned i = 0; i < fbo; ++i) {
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
                if ((err=glGetError())) {
                    Error() << "GL Error: " << glGetErrorString(err) << " after call to glBindTexture";
                    return false;
                }
                //glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
                // optimization to use less RAM
                glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, ifmt, Nx, Ny, 0, fmt, type, NULL);
                if ((err=glGetError())) {
                    Error() << "GL Error: " << glGetErrorString(err) << " after call to glTexImage2D";
                    return false;
                }
                glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                
            }
            // setup framebuffers-to-texture association
            for (unsigned i = 0; i < fbo; ++i) {
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
                if ( !checkFBStatus() ) {
                    Error() << "`FBO' error associating fbo/tex #" << i;
                    // Re-enable rendering to the window
                    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
                    return false;
                }

            }
            
            if ( !checkFBStatus() ) {
                Error() << "`FBO' error after initialization.";
                // Re-enable rendering to the window
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
                return false;
            }

            fc->createMore.release(fbo);
            for (unsigned i = 0; i < fbo; ++i) {
                // enable rendering to the FBO
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbos[i]);
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
                glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // just to be explicit about which attachment we are drawing to in the FBO
                glPushAttrib(GL_VIEWPORT_BIT);
                glViewport(0, 0, w, h);
                // draw to off-screen texture i
                fc->haveMore.acquire();
                Frame * f = fc->popOne();
                glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, Nx, Ny, fmt, type, f->texels);
				disps[i] = f->displacement;
                delete f;
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
                glPopAttrib();
                // at this point we have a texture with frame i living in VRAM
                // generate a mipmap for quality? nah -- mipmaps not supprted anyway for GL_TEXTURE_RECTANGLE_ARB path
                //glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[i]);
                //glGenerateMipmapEXT(GL_TEXTURE_RECTANGLE_ARB);
            }
            if (getNProcessors() < 2) cleanupFCs(); // no frame creation threads -- do it all in main thread
            // Re-enable rendering to the window
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            // re-set the drawbuffer
            glDrawBuffer(GL_BACK);
            
            Log() << "FBO texture generation completed in " << tim.elapsed()/1000. << " seconds.";
            setNums();
            return true;
}

void CheckerFlicker::cleanupFCs()
{
    for (std::vector<FrameCreator *>::iterator fc = fcs.begin(); fc != fcs.end(); ++fc) 
        delete *fc;
    fcs.clear();
}

void CheckerFlicker::cleanupFBO()
{
    if (fbos || texs) {
        if (fbos)
            glDeleteFramebuffersEXT(fbo, fbos); // it's weird.. you have to detach the texture from the FBO first, before deleting the texture itself
        if (texs)
            glDeleteTextures(fbo, texs);
        delete [] fbos;
        delete [] texs;
		delete [] disps;
        fbos = 0;
        texs = 0;
		disps = 0;
        Debug() << "Freed " << fbo << " framebuffer objects.";
        // Make sure rendering to the window is on, just in case
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    } 
    fbo = 0;
    cleanupFCs();
}
void CheckerFlicker::cleanup() 
{
    cleanupFBO();
    setNums();    
	if (origThreadAffinityMask) {
		setCurrentThreadAffinityMask(origThreadAffinityMask); // make it runnable as it was before plugin was run
	}
}

void CheckerFlicker::genGaussColors()
{
    gaussColors.resize(gaussColorMask+1); // reserve data for color table -- gaussColorMask should be <=16MB..
    for (unsigned i = 0; i < gaussColors.size(); ++i) {
        double d = ((gasGen()*(bgcolor*contrast)) + bgcolor);
        if (d < 0. || d > 1.) { --i; continue; } // keep retrying until we get a value in range
        gaussColors[i] = static_cast<GLubyte>( d * 256.);
        //qDebug("%hhu", gaussColors[i]);
    }
}


Frame *CheckerFlicker::genFrame(std::vector<unsigned> & entvec)
{
    static QMutex rngmut;
    double t0 = getTime();

    Frame *f = new Frame(Nx, Ny, 4);
    __m128i *quads = (__m128i *)f->texels; (void)quads;
    unsigned *dwords = (unsigned *)f->texels;
    bool dolocking = fcs.size() > 1;

	// Random Frame displacement -- NEW!  Added by Calin 8/04/2009
	if (rand_displacement_x || rand_displacement_y) {
		if (dolocking) rngmut.lock();
		if (rand_displacement_x) f->displacement.x = ((ran1Gen.next()*2.0)-1.0)*int(rand_displacement_x);
		if (rand_displacement_y) f->displacement.y = ((ran1Gen.next()*2.0)-1.0)*int(rand_displacement_y);
		if (dolocking) rngmut.unlock();
	}

    if (blackwhite) {
        if (dolocking) rngmut.lock();
        gen_rand_array((w128_t *)f->texels, MAX(f->nqqw, N));
        if (dolocking) rngmut.unlock();
        if (fps_mode == FPS_Dual) { // for this mode we need to eliminate the green channels (and alpha can be set to whatever), so we use an SSE2 instruction
			// need to 0 out every other byte
            const __m128i mask = _mm_set_epi32(0, 0, 0, 0);
			for (unsigned long i = 0; i < f->nqqw; ++i) 
                quads[i] = _mm_unpacklo_epi8(quads[i], mask); // this makes quads[i] be b0,0,b1,0,b2,0..b7,0  
        } 
/*        if (quad_fps) {
            const __m128i mask = _mm_set_epi32(0, 0, 0, 0);
            for (unsigned long i = 0; i < f->nqqw; ++i)
                quads[i] = _mm_cmpgt_epi8(mask, quads[i]);
        } else {
            const __m128i mask = _mm_set_epi32(0, 0, 0, 0);
            for (unsigned long i = 0; i < f->nqqw; ++i)
                quads[i] = _mm_cmpgt_epi32(mask, quads[i]);
        }
		*/
    } else {
#if 1
		const unsigned entr_arr_sz = f->nqqw*4*(((int)fps_mode)+1);
        entvec.resize(MAX(entr_arr_sz+8, (N+3)*4));
        const unsigned ndwords = f->nqqw*4;
        unsigned * entr = reinterpret_cast<unsigned *>((reinterpret_cast<unsigned long>(&entvec[0])+0x10UL)&~0xfUL); // align to 16-byte boundary
        if (dolocking) rngmut.lock();
        gen_rand_array((w128_t *)entr, MAX(entr_arr_sz/4, N));
        if (dolocking) rngmut.unlock();
        
        // f->seed = whatever here..
        if (fps_mode == FPS_Triple) {
            for (unsigned i = 0; i < ndwords; ++i) {
                dwords[i] = getColor(entr[0])|getColor(entr[1])<<8|getColor(entr[2])<<16;
                entr+=3;
            }
		} else if (fps_mode == FPS_Dual) {
            for (unsigned i = 0; i < ndwords; ++i) {
                dwords[i] = getColor(entr[0])|/*0|*/getColor(entr[1])<<16;
                entr+=2;
            }
        } else {  // single fps
            for (unsigned i = 0; i < ndwords; ++i) {
                unsigned char cc = getColor(*entr++);
                dwords[i] = cc|cc<<8|cc<<16|cc<<24;
            }
        }
#endif
#if 0 /* Uses Ziggurat Gaussians.. still not as fast as above method */
#       define CLR static_cast<unsigned char>((ZigguratGauss::generate(gen_rand32, contrast) + bgcolor)*(unsigned char)0xff)
        unsigned ndwords = f->nqqw*4;
        const double factor = contrast*bgcolor;
        if (dolocking) rngmut.lock();
        // f->seed = whatever here..
        if (quad_fps) {
            for (unsigned i = 0; i < ndwords; ++i) {
                dwords[i] = CLR|CLR<<8|CLR<<16;
            }

        } else {
            for (unsigned i = 0; i < ndwords; ++i) {
                unsigned char cc = CLR;
                dwords[i] = cc|cc<<8|cc<<16|cc<<24;
            }
        }
        if (dolocking) rngmut.unlock();
#       undef CLR
#endif
    }
    lastFramegen = static_cast<int>((getTime()-t0)*1000.);
    const unsigned nFrames = MIN(frameNum, 1000);
    frameGenAvg_usec = static_cast<unsigned int>(((double(frameGenAvg_usec) * (nFrames-1)) + lastFramegen*1e3) / double(nFrames));
    return f;
}

void CheckerFlicker::drawFrame()
{
        if (!initted) {
            glClear( GL_COLOR_BUFFER_BIT );
            return;
        } 

        // using framebuffer objects.. the fastest but not as portable 
        // method
        if (!blinkCt) takeNum();
        if (++blinkCt >= Nblinks) blinkCt = 0, putNum();
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[num]);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (blackwhite) {
            glEnable(GL_BLEND);
			if (fps_mode == FPS_Dual) // black out green channel
				glClearColor(bgcolor, 0., bgcolor, 1.0-contrast);
			else
				glClearColor(bgcolor, bgcolor, bgcolor, 1.0-contrast);
            glBlendFunc(GL_ONE, GL_ZERO);
            glClear(GL_COLOR_BUFFER_BIT);
            //glBlendColor(0., 0., 0., contrast);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);
        } else {
			// clearcolor set at top of function
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glTranslatef(disps[num].x, disps[num].y, 0); // displace frame
        glBegin(GL_QUADS);
          if (blackwhite) glColor4f(0., 0., 0., contrast);
          glTexCoord2i(0, 0);   glVertex2i(lmargin, bmargin);
          glTexCoord2i(Nx, 0);   glVertex2i(w-rmargin, bmargin);
          glTexCoord2i(Nx, Ny);   glVertex2i(w-rmargin, h-tmargin);
          glTexCoord2i(0, Ny);   glVertex2i(lmargin, h-tmargin);
        glEnd();
        glTranslatef(-disps[num].x, -disps[num].y, 0); // translate back

        glDisable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
}


void CheckerFlicker::setNums()
{
    newnums.clear(); oldnums.clear();
    for (unsigned i = 0; i < fbo; ++i) oldnums.push_back(i);
}

void CheckerFlicker::afterVSync(bool isSimulated)
{
    if (!initted) return;
    QTime tim;
    tim.start();
    unsigned n = 0, avail = 0, havmor = 0, nwait = 0;
    if (!fcs.size()) { 
        // single processor mode..
        static std::vector<unsigned> hack_entr_vec;
        Frame *f = genFrame(hack_entr_vec);
        ++n;
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[idx]);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, Nx, Ny, fmt, type, f->texels);
		disps[idx] = f->displacement; // save displacement as well
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
        delete f;        
        return;
    } // else.. multiprocessor mode -- we have at least 1 FrameCreator thread, so harvest frames here

   
    const unsigned nQMax = MAX(fbo/fcs.size(),1);
    lastAvgTexSubImgProcTime = 0.0;
    for (std::vector<FrameCreator *>::iterator fcit = fcs.begin(); fcit != fcs.end(); ++fcit) {
        FrameCreator *fc = *fcit;
        avail += fc->createMore.available();
        havmor += fc->haveMore.available();
        nwait += fc->nWaiting();

        if ( fc->nWaiting()+fc->createMore.available() < nQMax)
            fc->createMore.release();

        QTime tim2;         
        if ( ( fc->haveMore.available() // if there are frames to consume, that is, we won't block
               || isSimulated)  // or we are simulated, in which case we are ok with blocking
             && blinkCt==0 // only get frames if blinkCt==0 so that we don't lose our frame order
             && !newnums.size()) { // only grab frames if our 'queue' is empty
            tim2.restart();
            unsigned idx = newFrameNum();
            
            //qDebug("idx %u hwfc %u", idx, (unsigned)getHWFrameCount());
            fc->haveMore.acquire(); // NB: doesn't block here, except for isSimulated mode where it does
            Frame *f = fc->popOne();
            if ( fc->nWaiting()+fc->createMore.available() < nQMax )
                fc->createMore.release();
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texs[idx]);
            glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, Nx, Ny, fmt, type, f->texels);
			disps[idx] = f->displacement;
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
            delete f;
            ++n;
            
            const float secs = tim2.elapsed()/1e3;
            lastAvgTexSubImgProcTime += secs;
            cycleTimeLeft -= secs;
        }
        if (isSimulated && n) break; // only loop once for isSimulated mode
    }

    if (!n && !nwait)
        nConsecSkips++;
    else nConsecSkips -= n;
    if (nConsecSkips < 0) nConsecSkips = 0;

    if (n) {
        lastAvgTexSubImgProcTime /= n; // convert to avg        
        //qDebug("fcount: %d xferred %d textures in %d msecs avgTexSubImgTime %g msecs cycletime left %g msecs haveMore %d createMore %d nWaiting %u nConsecSkips %d lastFGenTime: %d msecs", getHWFrameCount(), n, tim.elapsed(), lastAvgTexSubImgProcTime*1e3, cycleTimeLeft*1e3, havmor, avail, nwait, nConsecSkips, lastFramegen); 
    }

    if (nConsecSkips >= (int)fbo) {
        Warning() << "Possible underrun of frames.  Frame creation thread can't keep up for frame " << getHWFrameCount();
        QString s;
        s.sprintf("cycletimeleft: %g ms thrdct: %d haveMore: %d createMore: %d nWaiting: %u nConsecSkips: %d lastFramegen: %d ms required: %d ms",  cycleTimeLeft*1e3, fcs.size(), havmor, avail, nwait, nConsecSkips, lastFramegen, int((1e3/getHWRefreshRate())*fcs.size()));
        Debug() << s;
        // now, create new threads to keep up, if available
        int ncoresavail = getNProcessors() - fcs.size() - 1; // allow 1 proc to be used for main thread always
        if (ncoresavail < 0) ncoresavail = 0;
        if (ncoresavail && fcs.size() < nCoresMax) {
            Warning() << "You have " << ncoresavail << " cores available, creating  a new FrameCreator thread to compensate.";
            Error() << "CHECKERFLICKER CURRENTLY PRODUCES NON-DETERMINISTIC FRAME ORDERING FOR >1 FRAME CREATOR THREADS!  SET CORES=2 TO AVOID THIS!! FIXME!!";
            fcs.push_back(new FrameCreator(*this));
            fcs.back()->start();
        }
        --nConsecSkips;
    }
    // every 45 frames or so, update custom string
    if (!(frameNum%45)) {
        customStatusBarString.sprintf("Frame gen. %d ms  Avg. %d usec", lastFramegen, frameGenAvg_usec);
    }
}

FrameCreator::FrameCreator(CheckerFlicker & cf)
    : QThread(&cf), cf(cf), stop(false)
{
}

FrameCreator::~FrameCreator()
{
    stop = true;
    createMore.release(1);
    wait();
    mut.lock();
    for (std::deque<Frame *>::iterator it = frames.begin(); it != frames.end(); ++it)
        delete *it;
    frames.clear();
    mut.unlock();
}

void FrameCreator::run()
{
    std::vector<unsigned> entropyMem; 
	entropyMem.reserve(cf.Nx*cf.Ny*(((int)cf.fps_mode)+1)+16);

	unsigned nProcs;
	if ((nProcs=getNProcessors()) > 2) {
		const unsigned mask = (0x1<<(nProcs-3))<<(cf.fcs.size()%nProcs);
		setCurrentThreadAffinityMask(mask); // pin it to one core
		Log() << "Set thread affinity mask to: " << mask;
	}

    while (!stop) {
        createMore.acquire();
        if (stop) return;
        Frame *f = cf.genFrame(entropyMem);
        mut.lock();
        frames.push_back(f);
        mut.unlock();
        haveMore.release();
    }
}

void CheckerFlicker::save()
{
    outStream << "Parameters:" << "\n"
              << "stixelwidth = " << stixelWidth << "\n"
              << "stixelheight = " << stixelHeight << "\n"
              << "blackwhite = " << (blackwhite?"true":"false") << "\n"
              << "bgcolor = " << bgcolor << "\n"
              << "contrast = " << contrast << "\n"
              << "seed = " << originalSeed << "\n"
              << "Nblinks = " << Nblinks << "\n"
              << "lmargin = " << lmargin << "\n"
              << "rmargin = " << rmargin << "\n"
              << "bmargin = " << bmargin << "\n"
              << "tmargin = " << tmargin << "\n"
			  << "fps_mode = " << (fps_mode == FPS_Single ? "single" : (fps_mode == FPS_Dual ? "dual" : "triple")) << "\n"
              << "fbo = " << fbo << "\n"
              << "colortable = " << (gaussColorMask+1) << "\n"
              << "cores = " << nCoresMax << "\n"
              << "Stats:\n"
              << "cores_used = " << (fcs.size()+1) << "\n"
              << "last_frame_gen_time_ms = " << lastFramegen << "\n"
              << "lastAvgTexSubImgProcTime_secs = " << lastAvgTexSubImgProcTime << "\n";

}

unsigned CheckerFlicker::initDelay(void)
{
	return 500;
}