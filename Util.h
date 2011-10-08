#ifndef Util_H
#define Util_H

class QString;
class StimApp;
#include <QMutex>
#include <QObject>
#include <QColor>
#include <QTextStream>
#include <QString>
#include <QVector>
#include <QRegExp>
#include <math.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#ifndef MIN
#define MIN(a,b) ( (a) <= (b) ? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a,b) ( (a) >= (b) ? (a) : (b) )
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) (x*(M_PI/180.0))
#endif

#ifndef RAD2DEG
#define RAD2DEG(x) (x*(180.0/M_PI))
#endif

#define EPSILON 0.0000001
// since == isn't reliable for floats, use this instead
#define eqf(x,y) (fabs(x-y) < EPSILON)

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
/// Enable or disable vertical synchronization in the opengl context.  (VSynch 
///	synchronizes display updates with the vertical blank signal.)
/// -- implemented in osdep.cpp
extern void setVSyncMode(bool vsync); 

/// retrieve a time value from the system's high resolution timer, in seconds
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

/// Pass in a block of CSV text, and you will get back a vector of all the 
/// doubles that were parsed by separating out the CSV values.  
extern QVector<double> parseCSV(const QString & text, const QRegExp & sepre = QRegExp("(\\s+)|,"));
	
/// Inverse of above
extern QString joinCSV(const QVector<double> & vals, const QString & comma = ", ");
	
/// Creates a unique filename based on prefix.  Appends date to the filename and potentially a unique integer.  Used by FrameVars class and other code.
extern QString makeUniqueFileName(const QString & prefix, const QString & ext);	

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

template <typename T=double> 
struct Vec2T {
	union { 
		struct { T x, y; };		
		struct { T w, h; };		
		struct { T v1, v2; };
	};
	Vec2T(T x = 0, T y = 0) : x(x), y(y) {}	

	T & operator[](int i) { if (i == 0) return x; return y; }	

	template <typename U> Vec2T<T> operator*(const U & u) const { return Vec2T<T>(x*u,y*u); }
	template <typename U> Vec2T<T> operator/(const U & u) const { return Vec2T<T>(x/u,y/u); }
	template <typename U> Vec2T<T> operator+(const U & u) const { return Vec2T<T>(x+u,y+u); }
	template <typename U> Vec2T<T> operator-(const U & u) const { return Vec2T<T>(x-u,y-u); }
	Vec2T<T> operator-(const Vec2T<T> & v) const { return Vec2T<T>(x-v.x,y-v.y); }
	Vec2T<T> operator+(const Vec2T<T> & v) const { return Vec2T<T>(x+v.x,y+v.y); }
	T dot(const Vec2T<T> & v) const { return x*v.x + y*v.y; }
	T magnitude() const { return sqrt(x*x + y*y); }
	Vec2T<T> normalized() const { 
		const T m (magnitude());
		return Vec2T<T>(x/m, y/m);
	}
};

template <typename T=double> 
struct Vec3T {
	union { 
		struct { T x, y, z; };		
		struct { T r, g, b; };		
		struct { T v1, v2, v3; };
	};
	Vec3T(T v1 = 0, T v2 = 0, T v3 = 0) : v1(v1), v2(v2), v3(v3) {}	

	T & operator[](int i) { if (i == 0) return x; else if (i==1) return y; return z; }

	// some common vector operations
	Vec3T<T> operator-(const Vec3T<T> & v) const { return Vec3T<T>(x-v.x,y-v.y,z-v.z); }
	Vec3T<T> operator+(const Vec3T<T> & v) const { return Vec3T<T>(x+v.x,y+v.y,z+v.z); }
	template <typename U> Vec3T<T> operator*(const U & u) const { return Vec3T<T>(x*u,y*u,z*u); }
	template <typename U> Vec3T<T> operator/(const U & u) const { return Vec3T<T>(x/u,y/u,z/u); }
	template <typename U> Vec3T<T> operator+(const U & u) const { return Vec3T<T>(x+u,y+u,z+u); }
	template <typename U> Vec3T<T> operator-(const U & u) const { return Vec3T<T>(x-u,y-u,z-u); }
	
	T dot(const Vec3T<T> & v) const { return x*v.x + y*v.y + z*v.z; }
	Vec3T<T> cross(const Vec3T<T> & v) const {
		return Vec3T<T>(y*v.z - z*v.y, 
						z*v.x - x*v.z,
						x*v.y - y*v.x);
	}
	T magnitude() const { return sqrt(x*x + y*y + z*z); }
	Vec3T<T> normalized() const { 
		const T m (magnitude());
		return Vec3T<T>(x/m, y/m, z/m);
	}
	Vec3T<T> reflect(const Vec3T<T> & normal) const {
		Vec3T<T> n(normal.normalized()); // only needed if N isn't normalized already
		return (*this) - (n * ( 2.0 * dot(n) ));
	}
};
	
template <typename T=double>
struct Vec4T 
{
	union { 
		struct { T x, y, z, w; };		
		struct { T r, g, b, a; };		
		struct { T v1, v2, v3, v4; };
	};
	Vec4T(T v1 = 0, T v2 = 0, T v3 = 0, T v4 = 0) : v1(v1), v2(v2), v3(v3), v4(v4) {}
	T & operator[](int i) { if (i == 0) return x; else if (i==1) return y; else if (i == 2) return z; return w; }
	const T & operator[](int i) const { if (i == 0) return x; else if (i==1) return y; else if (i == 2) return z; return w; }
};
	
typedef Vec2T<double> Vec2d;
typedef Vec2T<float> Vec2f;
typedef Vec2T<int> Vec2i;
typedef Vec2d Vec2;	
	
extern const Vec2 Vec2Zero; // default 0 vec -- useful as a default argument to functions
extern const Vec2 Vec2Unit; // unit vector, that is, (1,1) -- usefule as a default argument to functions
extern const Vec2i Vec2iZero; // default 0 vec -- useful as a default argument to functions
extern const Vec2i Vec2iUnit; // unit vector, that is, (1,1) -- usefule as a default argument to functions

typedef Vec3T<double> Vec3;
typedef Vec4T<int> Vec4i;
typedef Vec3T<int> Vec3i;
typedef Vec4T<double> Vec4;
	
extern const Vec3 Vec3Zero; // default 0 vec -- useful as a default argument to functions
extern const Vec3 Vec3Unit; // default unit vec -- useful as a default argument to functions
extern const Vec3 Vec3Gray; // default .5 vec -- useful as a default argument to functions

/// Rotate the vector v, about the euler angles alpha, beta, gamma using x-y-z convention.  Returns the rotated vector.
extern Vec3 Vec3RotateEuler(const Vec3 & v, double a, double b, double c);
	
} // end namespace Util

using namespace Util;

#endif
