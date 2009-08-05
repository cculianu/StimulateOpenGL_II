#ifndef Util_H
#define Util_H

class QString;
class StimApp;
#include <QMutex>
#include <QObject>
#include <QColor>
#include <QTextStream>
#include <QString>

#ifndef MIN
#define MIN(a,b) ( (a) <= (b) ? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a,b) ( (a) >= (b) ? (a) : (b) )
#endif

/// Various global utility functions used application-wide.
namespace Util 
{

/** Just like QObject::connect except it will popup an error box if there
    was a problem connecting, and exit the program immediately */
extern void Connect(QObject *srcobj, const QString & src_signal, 
                    QObject *destobj, const QString & dest_slot);

/// Returns a pointer to the StimApp singleton instance
extern StimApp *stimApp();

/// set the current process to realtime priority, implemented in osdep.cpp
extern void setRTPriority(); 

/// sets the process affinity mask -- a bitset of which processors to run on
extern void setProcessAffinityMask(unsigned mask);

/// return true iff opengl has the X ext gl_ext
extern bool hasExt(const char *gl_ext);

/// \brief Enable vsync
///
/// Set the opengl mode to synchronize with the vertical blank signal 
/// -- implemented in osdep.cpp
extern void setVSyncMode(); 

/// retrieve a time value from the system's high resolution timer
extern double getTime();

/// retrieve the frame count from the GLX_SGI_video_sync ext
extern unsigned getHWFrameCount();

/// \brief True if the platform has a hardware framecount function.
///
/// true on platforms like linux that has the glXGetVideoSyncSGI function
/// false on windows and platforms lacking this glX function
extern bool hasAccurateHWFrameCount();

/// retrieve the refresh rate from HW (implemented in osdep.cpp)
extern unsigned getHWRefreshRate();

/// returns the number of real CPUs (cores) on the system
extern unsigned getNProcessors(); 

/// returns the current host name
extern QString getHostName();

/// turns off Nagle algorithm for socket sock
extern void socketNoNagle(int sock);

/// \brief True if the platform we are running on has a 'get refresh rate' function
///
/// true on Windows 
/// false on others
extern bool hasAccurateHWRefreshRate();

/// \brief std::rand() based random number from [min, max].
///
/// You don't formally want to use this.  See the RNG class instead.
extern double random(double min = 0., double max = 1.);

/// returns a QString signifying the last error error
extern QString glGetErrorString(int err);

/// returns the number of seconds since this machine last rebooted
extern unsigned getUpTime();

/// `Pins' the current thread to a particular set of processors, that is, makes it only run on a particular set of processors.
/// returns 0 on error, or the previous mask on success.
extern unsigned setCurrentThreadAffinityMask(unsigned cpu_mask);

/// Super class of Debug, Warning, Error classes.  
class Log 
{
public:
    Log();
    virtual ~Log();
    
    template <class T> Log & operator<<(const T & t) {  s << t; return *this;  }
protected:
    bool doprt;
    QColor color;

private:    
    QString str;
    QTextStream s;
};

/** \brief Stream-like class to print a debug message to the app's console window
    Example: 
   \code 
        Debug() << "This is a debug message"; // would print a debug message to the console window
   \endcode
 */
class Debug : public Log
{
public:
    virtual ~Debug();
};

/** \brief Stream-like class to print an error message to the app's console window
    Example: 
   \code 
        Error() << "This is an ERROR message!!"; // would print an error message to the console window
   \endcode
 */
class Error : public Log
{
public:
    virtual ~Error();
};

/** \brief Stream-like class to print a warning message to the app's console window

    Example:
  \code
        Warning() << "This is a warning message..."; // would print a warning message to the console window
   \endcode
*/
class Warning : public Log
{
public:
    virtual ~Warning();
};

/// Stream-like class to print a message to the app's status bar
class Status
{
public:
    Status(int timeout = 0);
    virtual ~Status();
    
    template <class T> Status & operator<<(const T & t) {  s << t; return *this;  }
private:
    int to;
    QString str;
    QTextStream s;
};


struct Vec2 {
	double x,y;
	Vec2(double x = 0, double y = 0) : x(x), y(y) {}
};

struct Vec2i {
	int x, y;
	Vec2i(int x = 0, int y = 0) : x(x), y(y) {}
};

} // end namespace Util

using namespace Util;

#endif
