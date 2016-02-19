#include "Util.h"
#include "GLHeaders.h"
#include "StimApp.h"
#include "GLWindow.h"

#include <qglobal.h>
#include <QGLContext>

#ifdef Q_OS_WIN
#include <winsock.h>
#include <io.h>
#include <windows.h>
#include <wingdi.h>
#include <GL/gl.h>
#endif

#ifdef Q_WS_X11
#include <GL/gl.h>
#include <GL/glx.h>
// for XOpenDisplay
#include <X11/Xlib.h>
#endif

#ifdef Q_OS_DARWIN
#include <agl.h>
#include <gl.h>
#include <unistd.h>
#endif

#ifdef Q_OS_LINUX
// for sched_setscheduler
#include <sched.h>
// for getuid, etc
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#endif

#include <string.h>
#include <iostream>
#include <QHostInfo>
#include <QProcess>

namespace {
    struct Init {
        Init() {
            getTime(); // make the gettime function remember its t0
        }
    };
    Init init;
};

namespace Util {

#ifdef Q_OS_WIN
void setRTPriority()
{
    Log() << "Setting process to realtime";
    if ( !SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) ) 
        Error() << "SetPriorityClass() call failed: " << (int)GetLastError();    
}
double getTime()
{
    static __int64 freq = 0;
    static __int64 t0 = 0;
    __int64 ct;

    if (!freq) {
        QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    }
    QueryPerformanceCounter((LARGE_INTEGER *)&ct);   // reads the current time (in system units)
    if (!t0) t0 = ct;
    return double(ct-t0) / double(freq);
}
u64 getAbsTimeNS()
{
	static __int64 freq = 0;
	__int64 ct, factor;
	
	if (!freq) {
		QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	}
	QueryPerformanceCounter((LARGE_INTEGER *)&ct);   // reads the current time (in system units) 
	factor = 1000000000LL/freq;
	if (factor <= 0) factor = 1;
	return u64(ct * factor);
}	
/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        Error() << "Error from Win32 API when setting process affinity mask: " << GetLastError();
    } else {
        Log() << "Process affinity mask set to: " << QString().sprintf("0x%x",mask);
    }
}
unsigned long long getHWPhysMem()
{
	MEMORYSTATUSEX memory_status;
	ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
	memory_status.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&memory_status)) {
		return memory_status.ullTotalPhys;
	} 
	return 512ULL*1024ULL*1024ULL; // return 512 MB?
}
#elif defined(Q_OS_LINUX)
void setRTPriority()
{
    if (geteuid() == 0) {
        Log() << "Running as root, setting priority to realtime";
        if ( mlockall(MCL_CURRENT|MCL_FUTURE) ) {
            int e = errno;
            Error() <<  "Error from mlockall(): " <<  strerror(e);
        }
        struct sched_param p;
        p.sched_priority = sched_get_priority_max(SCHED_RR);
        if ( sched_setscheduler(0, SCHED_RR, &p) ) {
            int e = errno;
            Error() << "Error from sched_setscheduler(): " <<  strerror(e);
        }
    } else {
        Warning() << "Not running as root, cannot set priority to realtime";
    }    
}

double getTime()
{
        static double t0 = -9999.;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t = double(ts.tv_sec) + double(ts.tv_nsec)/1e9;
        if (t0 < 0.) t0 = t; 
        return t-t0;
}
u64 getAbsTimeNS()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return u64(ts.tv_sec)*1000000000ULL + u64(ts.tv_nsec);
}	
/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (unsigned i = 0; i < sizeof(mask)*8; ++i) {
        if (mask & 1<<i) CPU_SET(i, &cpuset);
    }
    int err = sched_setaffinity(0, sizeof(cpuset), &cpuset);
    if (err) {
        Error() << "sched_setaffinity(" << QString().sprintf("0x%x",mask) << ") error: " << strerror(errno);
    } else {
        Log() << "Process affinity mask set to: " << QString().sprintf("0x%x",mask);
    }
}
	
unsigned long long getHWPhysMem()
{
	QProcess p;
	p.start("awk", QStringList() << "/MemTotal/ { print $2 }" << "/proc/meminfo");
	p.waitForFinished();
	QString memory = p.readAllStandardOutput();
	p.close();
	unsigned long long memory = memory.toULongLong() * 1024ULL;
	if (!memory) memory = 512*1024*1024;
	return memory;
}
	
#else /* !WIN and !LINUX */
void setRTPriority()
{
    Warning() << "Cannot set realtime priority -- unknown platform!";
}
/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
	(void)mask;
    Warning() << "`Set process affinity mask' for this platform unimplemented -- ignoring.";
}
} // end namespace util
#ifdef Q_OS_DARWIN
#include <mach/mach_time.h>
namespace Util {
double getTime()
{
	double t = static_cast<double>(mach_absolute_time());
	struct mach_timebase_info info;
	mach_timebase_info(&info);
	return t * (1e-9 * static_cast<double>(info.numer) / static_cast<double>(info.denom) );
}
u64 getAbsTimeNS() 
{
	/* get timer units */
	mach_timebase_info_data_t info;
	mach_timebase_info(&info);
	/* get timer value */
	uint64_t ts = mach_absolute_time();
	
	/* convert to nanoseconds */
	ts *= info.numer;
	ts /= info.denom;
	return ts;
}
	
unsigned long long getHWPhysMem()
{
	QProcess p;
	p.start("sysctl", QStringList() << "-n" << "hw.memsize");
	p.waitForFinished();
	QString system_info = p.readAllStandardOutput();
	p.close();
	unsigned long long mem = system_info.toULongLong();
	if (!mem) mem = 512ULL*1024ULL*1024ULL;
	return mem;
}
#else
#include <QTime>
namespace Util {

double getTime()
{
    static QTime t;
    static bool started = false;
    if (!started) { t.start(); started = true; }
    return double(t.elapsed())/1000.0;
}
u64 getAbsTimeNS()
{
	return u64(getTime()*1e9);
}

unsigned long long getHWPhysMem() { return 512ULL*1024ULL*1024ULL; }
#endif // !Q_OS_DARWIN

#endif 

static const GLubyte *strChr(const GLubyte * str, GLubyte ch)
{
    while (str && *str && *str != ch) ++str;
    return str;
}

static const char *gl_error_str(GLenum err)
{
    static char unkbuf[64];
    switch(err) {
    case GL_INVALID_OPERATION:
        return "Invalid Operation";
    case GL_INVALID_ENUM:
        return "Invalid Enum";
    case GL_NO_ERROR:
        return "No Error";
    case GL_INVALID_VALUE:
        return "Invalid Value";
    case GL_OUT_OF_MEMORY:
        return "Out of Memory";
    case GL_STACK_OVERFLOW:
        return "Stack Overflow";
    case GL_STACK_UNDERFLOW:
        return "Stack Underflow";
    default:
        qsnprintf(unkbuf, sizeof(unkbuf), "UNKNOWN: %d", (int)err);
        return unkbuf;
    }
    return 0; // not reached
}

bool hasExt(const char *ext_name)
{
    static const GLubyte * ext_str = 0;
#ifdef Q_WS_X11
    static const char *glx_exts = 0;
    if (stimApp() && stimApp()->glWin()) stimApp()->glWin()->makeCurrent();
#endif
    static const GLubyte space = static_cast<GLubyte>(' ');
    if (!ext_str) 
        ext_str = glGetString(GL_EXTENSIONS);
    if (!ext_str) {
        Warning() << "Argh! Could not get GL_EXTENSIONS! (" << gl_error_str(glGetError()) << ")";
    } else {
        const GLubyte *cur, *prev, *s1;
        const char *s2;
        // loop through all space-delimited strings..
        for (prev = ext_str, cur = strChr(prev+1, space); *prev; prev = cur+1, cur = strChr(prev, space)) {
            // compare strings
            for (s1 = prev, s2 = ext_name; *s1 && *s2 && *s1 == *s2 && s1 < cur; ++s1, ++s2)
                ;
            if (*s1 == *s2 || (!*s2 && *s1 == space)) return true; // voila! found it!
        }
    }

#ifdef Q_WS_X11
    if (!glx_exts) {
     // nope.. not a standard gl extension.. try glx_exts
     Display *dis;
     int screen;

     dis = XOpenDisplay((char *)0);
     if (dis) {
         screen = DefaultScreen(dis);
         const char * glx_exts_tmp = glXQueryExtensionsString(dis, screen);
         if (glx_exts_tmp)
             glx_exts = strdup(glx_exts_tmp);
         XCloseDisplay(dis);
     }
    }
     if (glx_exts) {
		 bool ret = false;
         const char *prev, *cur, *s1, *s2; 
         const char space = ' ';
         // loop through all space-delimited strings..
         for (prev = glx_exts, cur = strchr(prev+1, space); *prev; prev = cur+1, cur = strchr(prev, space)) {
        // compare strings
             for (s1 = prev, s2 = ext_name; *s1 && *s2 && *s1 == *s2 && s1 < cur; ++s1, ++s2)
            ;
             if (*s1 == *s2 ||  (!*s2 && *s1 == space)) { ret = true; break; } // voila! found it!
         }
		 return ret;
     }
#endif
    return false;
}

namespace {
	const char *vsms(bool b) { return b ? "enabled" : "disabled"; } // vsync mode string
}
	
#ifdef Q_WS_X11
void setVSyncMode(bool vsync)
{
    if (hasExt("GLX_SGI_swap_control")) {
        Log() << "Found `swap_control' GLX-extension, will try to set vsync mode to " << vsms(vsync) << ".";
        int (*func)(int) = (int (*)(int))glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (func) {
            func(vsync ? 1 : 0);
        } else
            Error() <<  "GLX_SGI_swap_control func not found!";
    } else
        Warning() << "Missing `swap_control' GLX-extension, vsync cannot be " << vsms(vsync) << "!";           
}
#elif defined(Q_WS_WIN) /* Windows */
typedef BOOL (APIENTRY *wglswapfn_t)(int);

void setVSyncMode(bool vsync)
{
    stimApp()->glWin()->makeCurrent();
    wglswapfn_t wglSwapIntervalEXT = (wglswapfn_t)QGLContext::currentContext()->getProcAddress( "wglSwapIntervalEXT" );
    if( wglSwapIntervalEXT ) {
        wglSwapIntervalEXT(vsync ? 1 : 0);
        Log() << "VSync mode " << vsms(vsync) << " using wglSwapIntervalEXT().";
    } else {
        Warning() << "VSync mode could not be " << vsms(vsync) << " because wglSwapIntervalEXT is missing.";
    }
}
#elif defined (Q_OS_DARWIN)

void setVSyncMode(bool vsync)
{
    stimApp()->glWin()->makeCurrent();
    GLint vs = vsync ? 1 : 0;
    AGLContext ctx = aglGetCurrentContext();
    if (aglEnable(ctx, AGL_SWAP_INTERVAL) == GL_FALSE)
        Warning() << "VSync mode could not be " << vsms(vsync) << " becuse aglEnable AGL_SWAP_INTERVAL returned false!";
    else {
        if ( aglSetInteger(ctx, AGL_SWAP_INTERVAL, &vs) == GL_FALSE )
            Warning() << "VSync mode could not be " << vsms(vsync) << " because aglSetInteger returned false!";
        else
            Log() << "VSync mode " << vsms(vsync) << " using aglSetInteger().";
    }
}

#else
#  error Unknown platform, need to implement setVSyncMode()!
#endif

bool hasAccurateHWFrameCount()
{
	return false;  /// we will assume HWFC is never accurate as it has proven to fail us time and again!
}

#ifdef Q_OS_WIN
unsigned getNProcessors()
{
    static int nProcs = 0;
    if (!nProcs) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        nProcs = si.dwNumberOfProcessors;
    }
    return nProcs;
}
#elif defined(Q_OS_LINUX)
} // end namespace util
#include <unistd.h>
namespace Util {
unsigned getNProcessors()
{
    static int nProcs = 0;
    if (!nProcs) {
        nProcs = sysconf(_SC_NPROCESSORS_ONLN);
    }
    return nProcs;
}
#elif defined(Q_OS_DARWIN)
} // end namespace util
#include <CoreServices/CoreServices.h>
namespace Util {
unsigned getNProcessors() 
{
    static int nProcs = 0;
    if (!nProcs) {
        nProcs = MPProcessorsScheduled();
    }
    return nProcs;
}
#else
unsigned getNProcessors()
{
    return 1;
}
#endif


#ifndef Q_OS_WIN /* GLX */
unsigned getHWFrameCount()
{
    static int (*func)(unsigned int *) = 0;
    static bool triedToFindFunc = false;
    if (!triedToFindFunc && !func) {
        triedToFindFunc = true;
        stimApp()->glWin()->makeCurrent();
        func = (int (*)(unsigned int *))QGLContext::currentContext()->getProcAddress( "glXGetVideoSyncSGI" );
        if (!func) {
            Error() << "No hw framecount func., will be emulated"; 
        } 
    }
    if (func) {
        unsigned int ret;
        func(&ret);
        return ret;
    }
    // otherwise emulated from the time and the HW refresh count -- note this may be off or drift because the system CPU clock is not necessarily synchronized to the video clock, right?
    return static_cast<unsigned>(getTime() * getHWRefreshRate()); 
}
#else /* WINDOWS */

unsigned getHWFrameCount()
{
	static BOOL (WINAPI *funcNV)(HDC, GLuint *) = 0;
    static BOOL (WINAPI *func)(HDC, INT64 *ust, INT64 *msc, INT64 *sbc) = 0;
    static bool triedToFindFunc = false;
    if (!triedToFindFunc && !func && stimApp() && stimApp()->glWin()) {
        triedToFindFunc = true;
        stimApp()->glWin()->makeCurrent();
		funcNV = (BOOL (WINAPI *)(HDC, GLuint *))QGLContext::currentContext()->getProcAddress("wglQueryFrameCountNV");
		func = (BOOL (WINAPI *)(HDC, INT64 *, INT64 *, INT64 *))QGLContext::currentContext()->getProcAddress( "wglGetSyncValuesOML" );
		if (funcNV) {
			Debug() << "Found NVIDIA QueryFrameCountNV function! YAY!";
		} else if (!func) {
			Error() << "No hw framecount func., will be emulated"; 
		}
    }
	if (funcNV) {
		if (stimApp() && stimApp()->glWin())
			stimApp()->glWin()->makeCurrent();			
		HDC hdc = wglGetCurrentDC();
		GLuint fc = 0;
		funcNV(hdc, &fc);
		if (fc)	return fc;
		// otherwise fall through..
	} 
	if (func) {
		if (stimApp() && stimApp()->glWin())
			stimApp()->glWin()->makeCurrent();			
        static INT64 f0 = 0;
        HDC hdc = wglGetCurrentDC();
        INT64 ust, msc, sbc;
        func(hdc, &ust, &msc, &sbc);
        if (!f0) f0 = msc;
		if (msc) return msc-f0; 
    }
	static bool didWarn = false;
	bool needWarn = funcNV || func;
	if (needWarn && !didWarn) {
		Error() << "HW framecount functions appear to return 0 values -- hwfc will be emulated!";
		didWarn = true;
	}
    // otherwise emulated from the time and the HW refresh count -- note this may be off or drift because the system CPU clock is not necessarily synchronized to the video clock, right?
    return static_cast<unsigned>(getTime() * getHWRefreshRate()); 
}
#endif

#ifdef Q_OS_WIN
unsigned getHWRefreshRate() 
{ 
    unsigned rate = 0;
    //if (!rate) {
        HDC dc;
        bool defaultdc = false;
        if (stimApp() && stimApp()->glWin()) {
            dc = GetDC(reinterpret_cast<HWND>(stimApp()->glWin()->winId()));
        } else {
            dc = CreateDCA( "DISPLAY", NULL, NULL, NULL );
            defaultdc = true; 
        }
        if (!dc) {
            Error() << "Could not create Win32 DC, getHWRefreshRate() will always return fake value of 120Hz!";
            rate = 120;
        } else {
            rate = GetDeviceCaps(dc, VREFRESH);
            //Debug() << "Windows reports HW refresh rate " << rate << "Hz";
        }
        if (defaultdc) {
            DeleteDC(dc);
        } else if (dc) {
            ReleaseDC(reinterpret_cast<HWND>(stimApp()->glWin()->winId()), dc);
        }
        //}
	//Debug() >> "getHWRefreshRate() to return: " << rate;
    return rate;
}

bool hasAccurateHWRefreshRate()
{
    return false;
}

#else
// on linux we can't query the refresh rate so we have to just take the calibrated value
unsigned getHWRefreshRate()
{
    if (stimApp())
        return stimApp()->refreshRate();
    return 120;
}

bool hasAccurateHWRefreshRate()
{
    return false;
}

#endif


QString getHostName()
{
    return QHostInfo::localHostName();
}

#ifdef Q_OS_WIN

void socketNoNagle(long sock)
{
    BOOL flag = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&flag), sizeof(flag));
    if (ret) Error() << "Error turning off nagling for socket " << sock;
}
#else
} // end namespace util
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
namespace Util {
void socketNoNagle(long sock)
{
    long flag = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&flag), sizeof(flag));
    if (ret) Error() << "Error turning off nagling for socket " << sock;
}
#endif

#ifdef Q_OS_WIN
unsigned getUpTime()
{
    return GetTickCount() / 1000;
}
#elif defined(Q_OS_LINUX)
} // end namespace Util
#include <sys/sysinfo.h>
namespace Util {
unsigned getUpTime()
{
    struct sysinfo si;
    sysinfo(&si);
    return si.uptime;
}
#else
unsigned getUpTime()
{
    return getTime();
}
#endif


#ifdef Q_OS_WIN

unsigned setCurrentThreadAffinityMask(unsigned mask)
{
	HANDLE h = GetCurrentThread();
	DWORD_PTR prev_mask = SetThreadAffinityMask(h, (DWORD_PTR) mask);
	return static_cast<unsigned>(prev_mask);
}

#else
unsigned  setCurrentThreadAffinityMask(unsigned mask)
{
	(void)mask;
	Error() << "setCurrentThreadAffinityMask() unimplemented on this platform!";
	return 0;
}
#endif

	
unsigned getPid()
{
#ifdef Q_OS_WIN
	return (unsigned)GetCurrentProcessId();
#else
	return (unsigned)getpid();
#endif
}
	
	
}
	
	
#ifdef Q_OS_WIN /* Hack for now to get windows to see the framebuffer ext stuff */
//#  ifndef WIN64
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
	
	// PBO-stuff
	GLAPI void APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
	{
		typedef void (APIENTRY *Fun_t) (GLsizei, GLuint *);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glGenBuffersARB");
		if (!fun) {
			Error() << "glGenBuffersARB not found";        
		} else
			fun(n,buffers);
	}
	GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
	{
		typedef void (APIENTRY *Fun_t) (GLsizei, const GLuint *);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glDeleteBuffersARB");
		if (!fun) {
			Error() << "glDeleteBuffersARB not found";        
		} else
			fun(n,buffers);	
	}
	GLAPI void APIENTRY glBindBuffer(GLenum target, GLuint buffer)
	{
		typedef void (APIENTRY *Fun_t) (GLenum, GLuint);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glBindBufferARB");
		if (!fun) {
			Error() << "glBindBufferARB not found";        
		} else
			fun(target,buffer);
	}
#if defined(WIN64) && QT_VERSION >= 0x050000
    GLAPI void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
#else
    GLAPI void APIENTRY glBufferData(GLenum target, GLsizei size, const GLvoid * data, GLenum usage)
#endif
	{
		typedef void (APIENTRY *Fun_t) (GLenum, GLsizei, const GLvoid *, GLenum);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glBufferDataARB");
		if (!fun) {
			Error() << "glBufferDataARB not found";        
		} else
			fun(target,size,data,usage);
	}
	GLAPI void * APIENTRY glMapBuffer(GLenum target, GLenum access)
	{
		typedef void * (APIENTRY *Fun_t) (GLenum, GLenum);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glMapBufferARB");
		if (!fun) {
			Error() << "glMapBufferARB not found";
			return 0;
		} 
		return fun(target,access);
	}
	GLAPI GLboolean APIENTRY glUnmapBuffer(GLenum target)
	{
		typedef GLboolean (APIENTRY *Fun_t) (GLenum);
		static Fun_t fun = 0;
		if (!fun) fun = (Fun_t)wglGetProcAddress("glUnmapBufferARB");
		if (!fun) {
			Error() << "glUnmapBufferARB not found";
			return 0;
		}
		return fun(target);	
	}
//#  endif
#endif
