#ifndef StimGL_SpikeGL_Integration_H
#define StimGL_SpikeGL_Integration_H
#include <QString>
#include <QMap>
#include <QVariant>
#include <QTcpServer>
#include <QObject>
class QSharedMemory;

namespace StimGL_SpikeGL_Integration 
{
#define SPIKE_GL_NOTIFY_DEFAULT_PORT 52521
#define SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS 1000


    /** Called by StimGL to notify SpikeGL that a plugin started.
        Blocks until the notification is completed, or until timeout expires.
        Use the errStr_out ptr to QString to determine the error, if any.
        @return false if timeout or error, true if succeeded.*/
    bool Notify_PluginStart(const QString & pluginName, 
                            const QMap<QString, QVariant>  &pluginParams, 
                            QString *errStr_out = 0, 
                            const QString & host = "127.0.0.1",
                            unsigned short port = SPIKE_GL_NOTIFY_DEFAULT_PORT, 
                            int timeout_msecs = SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS);

    /** Called by StimGL to notify SpikeGL that a plugin ended.
        Blocks until the notification is completed, or until timeout expires.
        Use the errStr_out ptr to QString to determine the error, if any.
        @return false if timeout or error, true if succeeded.*/
    bool Notify_PluginEnd(const QString & pluginName, 
                          const QMap<QString, QVariant>  &pluginParams, 
                          QString *errStr_out = 0, 
                          const QString & host = "127.0.0.1",
                          unsigned short port = SPIKE_GL_NOTIFY_DEFAULT_PORT, 
                          int timeout_msecs = SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS);
    
	/** Called by StimGL in some circumstances even if notification is turned off.  
	    This is so that PD-based acquisitions in SpikeGL can also be informed of plugin params. */	    
	bool Notify_PluginParams(const QString & pluginName, 
							 const QMap<QString, QVariant>  &pluginParams, 
							 QString *errStr_out = 0, 
							 const QString & host = "127.0.0.1",
							 unsigned short port = SPIKE_GL_NOTIFY_DEFAULT_PORT, 
							 int timeout_msecs = SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS);
	

    /** Object to  used inside SpikeGL to receive plugin start events
        from StimGL via the network. */
    class NotifyServer : public QObject
    {
        Q_OBJECT
    public:
        NotifyServer(QObject *parent);
        ~NotifyServer();

        /// returns immediately, but it starts the server and will emit gotPluginStartedNotification() when it receives notificaton from stimgl that the plugin started...
        bool beginListening(const QString & iface = "127.0.0.1", unsigned short port = SPIKE_GL_NOTIFY_DEFAULT_PORT, int timeout_msecs = SPIKE_GL_NOTIFY_DEFAULT_TIMEOUT_MSECS);

    signals:
        /// connect to this signal to be notified that the plugin started
        void gotPluginStartNotification(const QString & plugin,
                                        const QMap<QString, QVariant>  & params);
        /// connect to this signal to be notified that the plugin params were received.  Note not emitted if the gotPluginStartNotification was already just emitted for these params to prevent signal spamming
        void gotPluginParamsNotification(const QString & plugin,
                                         const QMap<QString, QVariant>  & params);
        /// connect to this signal to be notified that the plugin ended
        void gotPluginEndNotification(const QString & plugin,
                                      const QMap<QString, QVariant>  & params);

    private slots:
        void gotNewConnection();
        void emitGotPluginNotification(bool isStart, bool isEnd, const QString &, const QMap<QString, QVariant>  &);
        void processConnection(QTcpSocket & sock);

    private:
        QTcpServer srv;
        int timeout_msecs;
    };
	
#define FRAME_SHARE_SHM_MAGIC 0xf33d53c6
#ifdef Q_OS_DARWIN
#define FRAME_SHARE_SHM_SIZE (4*1024*1024)	///< size limits in OSX Darwin kernel for max shm size..
#else
#define FRAME_SHARE_SHM_SIZE (8*1024*1024)	///< on other OS's, use 8MB for the shm, which should be enough..
#endif
#define FRAME_SHARE_SHM_DATA_SIZE (FRAME_SHARE_SHM_SIZE-reinterpret_cast<unsigned long>(&reinterpret_cast<StimGL_SpikeGL_Integration::FrameShareShm *>(0)->data))
	extern "C" struct FrameShareShm {
		unsigned magic; ///< set by whichever program creates the Shm first for sanity checking..
		int enabled; ///< set to true by SpikeGL when it "wants" frames from StimGL.. if true StimGL will write frames into data and set w,h,fmt,sz_bytes appropriately...
		unsigned frame_num; ///< the frame count.. written-to by StimGL
		int fmt, w, h, sz_bytes; ///< the format and other info on the frame data, written by StimGL
		int do_box_select; ///< SpikeGL writes to this to tell StimGL to do the box selection stuff
		unsigned stimgl_pid; ///< StimGL writes to this to tell SpikeGL its PID.  If 0, StimGL definitely isn't running
		unsigned spikegl_pid; ///< SpikeGL writes to this to tell StimGL its PID.  If 0, SpikeGL definitely isn't running
		float box_x, box_y, box_w, box_h; ///< StimGL writes to this to tell SpikeGL the exact area of the overlay box relative to the overall StimGL window, coordinate range of values is always 0->1
		int dump_full_window; ///< SpikeGL writes to this to tell StimGL to not use the overlay box area and instead dump entire StimGL window, if true
		quint64 frame_abs_time_ns; ///< StimGL writes to this to give SpikeGL an indication of the age/time of the frame...
		int frame_rate_limit; ///< SpikeGL writes to this to tell StimGL at what approx. rate to write frames into the buffer.  Defaults to 10. 0 means don't use a frame rate limit.
		int reserved[14]; ///< reserved for future implementations and to align the data a bit..
		char data[1]; ///< the frame data, written-to by StimGL.. real size is obviously bigger than 1!
	};
	
	class FrameShare {
	public:
		FrameShare();
		~FrameShare();
		
		volatile FrameShareShm *shm;
		bool createdByThisInstance;
		
		void detach(); ///< detaches, side-effects is it sets shm to NULL
		
		int size() const; ///< returns the size of the shm, in bytes 
		bool lock();
		bool unlock();
		
		bool warnUserAlreadyRunning() const; ///< returns true if the user accepted the situation and hit Yes, or false if they hit No
	private:
		QSharedMemory *qsm;
	};
	
} // end namespace StimGL_SpikeGL_Integration

#endif
