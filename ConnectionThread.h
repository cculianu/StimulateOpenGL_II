#ifndef ConnectionThread_H
#define ConnectionThread_H

#include <QThread>
#include <QEvent>
class QTcpSocket;
class StimApp;

/**
   \brief A Class that encapsulates a single client connection.
   
   This class processes client protocol requests from a separate
   thread.  The run() and processLine() methods do the bulk of the work.
   
   If you want to add protocol commands, you need to modify this class, 
   specifically the processLine() method.
*/
class ConnectionThread : public QThread
{
public:
    /// Construct a connectionthread given a connected network socket.
    ConnectionThread(int socketdescr, QObject *parent = 0);
    /// Reimplemented from QObject so that we can post events to main thread from client threads
    bool eventFilter(QObject *, QEvent *); 
protected:
    /// \brief This function runs in a separate thread specific to the client and processes incoming requests from the client.
    ///
    /// Reads a line from the socket and passes it off to processLine,
    /// if processLine returns a non-null QString, sends it to the client
    /// then sends 'OK' otherwise sends 'ERROR' to the client.
    void run();
private:
    QString processLine(QTcpSocket & sock, const QString & line);
    int sockdescr;
};

#endif
