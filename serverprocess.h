#ifndef SERVERPROCESS_H
#define SERVERPROCESS_H

#include <QCoreApplication>
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

extern QCoreApplication *app;

class ServerProcess : public QObject
{
    Q_OBJECT
public:
    ServerProcess(QObject *parent = nullptr);
    ~ServerProcess();

signals:
    void command(QString cmd, QString receiverUsername);

private slots:
    void sendDataProc(QByteArray sData, QTcpSocket *socket);
    void commandProc(QString cmd, QString receiverUsername);
    void newMessage();
    void messageAsDataProc(QByteArray rData, QString senderUsername);
    void messageAsCommandProc(QByteArray rData, QString senderUsername);
    void sendRun();
    void disconnectedProc();
    void newConnection();
    void readCMD();

signals:
    void sendData(QByteArray sData, QTcpSocket *socket);
    void messageAsData(QByteArray rData, QString senderUsername);
    void messageAsCommand(QByteArray rData, QString senderUsername);

private:
    void appendToSocketList(QTcpSocket* socket);

private:
    QTcpServer *server;
    QVector<QStringList> job; // 1: username - 2: cmd
    QVector<QStringList> fJob; // 1: username - 2: cmd
    QVector<QStringList> j_delPending; // 1: username - 2: cmd
    QList<QStringList> offlineJob; // 1: username - 2: cmd
    QMap<QString, QTcpSocket *> socketList;
    QTimer readyReadTimer;
    QTimer sendTimer;
    QTimer cmdTimer;
    QFile *cmdFile;
};

#endif // SERVERPROCESS_H
