#ifndef StimGL_SpikeGL_Integration_H
#define StimGL_SpikeGL_Integration_H
#include <QString>
#include <QMap>
#include <QVariant>
#include <QTcpServer>
#include <QObject>

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
} // end namespace StimGL_SpikeGL_Integration


#endif
