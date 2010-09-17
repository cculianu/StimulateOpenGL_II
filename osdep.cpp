#include "Util.h"
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
// for sched_setscheduler
#endif

#ifdef Q_WS_MACX
#include <agl.h>
#include <gl.h>
#endif

#ifdef Q_OS_LINUX
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
/// sets the process affinity mask -- a bitset of which processors to run on
void setProcessAffinityMask(unsigned mask)
{
    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        Error() << "Error from Win32 API when setting process affinity mask: " << GetLastError();
    } else {
        Log() << "Process affinity mask set to: " << QString().sprintf("0x%x",mask);
    }
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
#include <QTime>
namespace Util {
double getTime()
{
    static QTime t;
    static bool started = false;
    if (!started) { t.start(); started = true; }
    return double(t.elapsed())/1000.0;
}

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
         const char *prev, *cur, *s1, *s2; 
         const char space = ' ';
         // loop through all space-delimited strings..
         for (prev = glx_exts, cur = strchr(prev+1, space); *prev; prev = cur+1, cur = strchr(prev, space)) {
        // compare strings
             for (s1 = prev, s2 = ext_name; *s1 && *s2 && *s1 == *s2 && s1 < cur; ++s1, ++s2)
            ;
             if (*s1 == *s2 ||  (!*s2 && *s1 == space)) return true; // voila! found it!
         }
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
#elif defined (Q_WS_MACX)

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
    if (!triedToFindFunc && !func) {
        triedToFindFunc = true;
        stimApp()->glWin()->makeCurrent();
		funcNV = (BOOL (WINAPI *)(HDC, GLuint *))QGLContext::currentContext()->getProcAddress("wglQueryFrameCountNV");
		if (funcNV) {
			Debug() << "Found NVIDIA QueryFrameCountNV function! YAY!";
		} else {
			func = (BOOL (WINAPI *)(HDC, INT64 *, INT64 *, INT64 *))QGLContext::currentContext()->getProcAddress( "wglGetSyncValuesOML" );
			if (!func) {
				Error() << "No hw framecount func., will be emulated"; 
			}
		}
    }
	if (funcNV) {
		if (stimApp() && stimApp()->glWin())
			stimApp()->glWin()->makeCurrent();			
		HDC hdc = wglGetCurrentDC();
		GLuint fc = 0;
		funcNV(hdc, &fc);
		return fc;
	} else if (func) {
		if (stimApp() && stimApp()->glWin())
			stimApp()->glWin()->makeCurrent();			
        static INT64 f0 = 0;
        HDC hdc = wglGetCurrentDC();
        INT64 ust, msc, sbc;
        func(hdc, &ust, &msc, &sbc);
        if (!f0) f0 = msc;
        return msc-f0;
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
            dc = GetDC(stimApp()->glWin()->winId());
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
            ReleaseDC(stimApp()->glWin()->winId(), dc);
        }
        //}
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

void socketNoNagle(int sock)
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
void socketNoNagle(int sock)
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

}
