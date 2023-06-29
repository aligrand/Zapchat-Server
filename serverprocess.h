#ifndef SERVERPROCESS_H
#define SERVERPROCESS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDataStream>
#include <QTcpSocket>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QSqlQuery>
#include <QVector>
#include <QFile>
#include <QTimer>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDebug>
#include <QDateTime>

#include "sqlrecordqstring.h"

class ServerProcess : public QObject
{
    Q_OBJECT
public:
    ServerProcess(QObject *parent = nullptr);
    ~ServerProcess();

signals:
    void command(QString cmd, QString receiverUsername);

private slots:
    void sendDataProc(QByteArray sData, QTcpSocket *socket, int &dataSize);
    void commandProc(QString cmd, QString receiverUsername);
    void newMessage();
    void messageAsDataProc(QByteArray rData);
    void messageAsCommandProc(QByteArray rData, QString senderUsername);
    void sendRun();
    void disconnectedProc();
    void dataArrivedProc(int channel, qint64 bytes);
    void newConnection();

signals:
    void sendData(QByteArray sData, QTcpSocket *socket, int &dataSize);
    void messageAsData(QByteArray rData);
    void messageAsCommand(QByteArray rData, QString senderUsername);
    void dataArrived();
    void start_sendRun();

private:
    void appendToSocketList(QTcpSocket* socket);

private:
    QTcpServer *server;
    QVector<QStringList> job; // 1: username - 2: cmd
    QVector<QStringList> job_delPending; // 1: username - 2: cmd
    QMap<QString, QTcpSocket *> socketList;
    QMap<QString, int> dataSizeList;
    QTimer readyReadTimer;
};

#endif // SERVERPROCESS_H
