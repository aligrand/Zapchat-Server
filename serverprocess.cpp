#include "serverprocess.h"

ServerProcess::ServerProcess(QObject *parent) : QObject(parent)
{
    server = new QTcpServer();

    qDebug() << "server set-up";

    server->listen(QHostAddress::Any, 8080);

    connect(this, &ServerProcess::command, this, &ServerProcess::commandProc);
    connect(&readyReadTimer, &QTimer::timeout, this, &ServerProcess::newMessage);
    connect(this, &ServerProcess::sendData, this, &ServerProcess::sendDataProc);
    connect(this, &ServerProcess::messageAsData, this, &ServerProcess::messageAsDataProc);
    connect(this, &ServerProcess::messageAsCommand, this, &ServerProcess::messageAsCommandProc);
    connect(server, &QTcpServer::newConnection, this, &ServerProcess::newConnection);
    connect(&cmdTimer, &QTimer::timeout, this, &ServerProcess::readCMD);
    connect(&sendTimer, &QTimer::timeout, this, &ServerProcess::sendRun);

    cmdFile = new QFile("cmd.txt");
    cmdFile->open(QIODevice::ReadWrite | QIODevice::Text);

    QFile _jFile("job-queue.txt");
    _jFile.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!_jFile.atEnd())
    {
        QStringList _content;
        _content[0] = _jFile.readLine().split(' ').first();
        _content[1] = _jFile.readLine().split(' ').last();
        job.push_front(_content);
    }
    _jFile.close();

    QFile _fjFile("force-job-queue.txt");
    _fjFile.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!_fjFile.atEnd())
    {
        QStringList _content;
        _content[0] = _fjFile.readLine().split(' ').first();
        _content[1] = _fjFile.readLine().split(' ').last();
        fJob.push_front(_content);
    }
    _fjFile.close();

    QString cmd;
    for (int i = 0; i < job.size() ; --i)
    {
        cmd = job[i][1];

        if (cmd == "UPDATE-DB" || cmd == "SET-PASS" || cmd == "UN-EXIST" ||
                cmd == "ROOM-EXIST" || cmd == "LOGIN" || cmd == "MESSAGE-INDEX" ||
                cmd == "REMOVE-USER" || cmd == "REMOVE-ROOM" || cmd == "ARRIVE")
        {
            fJob.push_back(job[i]);
            job.removeAt(i);
        }
    }

    sendTimer.start(50);
    readyReadTimer.start(50);
    cmdTimer.start(2000);
}

ServerProcess::~ServerProcess()
{
    cmdFile->close();
    delete cmdFile;

    foreach (const QString &socketList_key, socketList.keys())
    {
        socketList[socketList_key]->close();
        socketList[socketList_key]->deleteLater();

        socketList.remove(socketList_key);
    }

    QFile _jFile("job-queue.txt");
    _jFile.open(QIODevice::WriteOnly | QIODevice::Text);
    _jFile.resize(0);
    while (j_delPending.size() > 0)
    {
        _jFile.write((j_delPending.last()[0] + " " + j_delPending.last()[1] + "\n").toStdString().c_str());
        j_delPending.pop_back();
    }
    while (job.size() > 0)
    {
        _jFile.write((job.last()[0] + " " + job.last()[1] + "\n").toStdString().c_str());
        job.pop_back();
    }
    foreach (const QStringList &sList, offlineJob)
    {
        _jFile.write((sList[0] + " " + sList[1] + "\n").toStdString().c_str());
    }
    _jFile.close();

    QFile _fjFile("job-queue.txt");
    _fjFile.open(QIODevice::WriteOnly | QIODevice::Text);
    _fjFile.resize(0);
    while (fJob.size() > 0)
    {
        _fjFile.write((fJob.last()[0] + " " + fJob.last()[1] + "\n").toStdString().c_str());
        fJob.pop_back();
    }
    _fjFile.close();
}

void ServerProcess::sendDataProc(QByteArray sData, QTcpSocket *socket)
{
    QDataStream socketStream(socket);

    socketStream << sData;
}

void ServerProcess::commandProc(QString cmd, QString receiverUsername)
{
    QString cmdName = cmd.split(" ").first();
    QStringList _content = {receiverUsername, cmd};

    if (cmdName == "UN-EXIST-RESULT" || cmdName == "ROOM-EXIST-RESULT" ||
            cmdName == "LOGIN-RESULT" || cmdName == "MESSAGE-INDEX-RESULT" ||
            cmdName == "REMOVE-USER" || cmdName == "REMOVE-ROOM" || cmdName == "ARRIVE")
    {
        fJob.push_front(_content);
    }
    else
    {
        job.push_front(_content);
    }
}

void ServerProcess::newMessage()
{
    foreach (const QString &socketList_key, socketList.keys())
    {
        QByteArray data;

        if (socketList[socketList_key]->bytesAvailable() == 0)
        {
            continue;
        }

        QDataStream socketStream(socketList[socketList_key]);
        socketStream.setVersion(QDataStream::Qt_5_13);

        socketStream.startTransaction();
        socketStream >> data;

        if (!socketStream.commitTransaction())
        {
            continue;
        }

        if (data.at(0) == 0)
        {
            emit messageAsCommand(data, socketList_key);
        }
        else
        {
            emit messageAsData(data, socketList_key);
        }
    }
}

void ServerProcess::messageAsDataProc(QByteArray rData, QString senderUsername)
{
    QString fileName, fileFormat, dir;

    fileName = rData.mid(1, 60);
    fileFormat = rData.mid(61, 10);

    qDebug() << "RECIVE" << fileName << "." << fileFormat;

    if (fileName[fileName.size() - 1] == "I")
    {
        dir = "Images/";
    }
    else if (fileName[fileName.size() - 1] == "V")
    {
        dir = "Videos/";
    }
    else if (fileName[fileName.size() - 1] == "A")
    {
        dir = "Audios/";
    }
    else if (fileName[fileName.size() - 1] == "F")
    {
        dir = "Files/";
    }
    else
    {
        dir = "Profiles/";
    }

    emit command("ARRIVE", senderUsername);

    QFile::remove(dir + fileName + "." + fileFormat);
    QFile file(dir + fileName + "." + fileFormat);
    file.open(QIODevice::WriteOnly);
    file.write(rData.mid(71));
    file.close();
}

void ServerProcess::messageAsCommandProc(QByteArray rData, QString senderUsername)
{
    QString cmd = rData.mid(71);
    QString cmdName = cmd.split(" ").first();
    QString cmdArgs = QString::fromStdString(cmd.toStdString().substr(static_cast<size_t>(cmd.indexOf(" ")) + 1));

    qDebug() << "RECIVE" << senderUsername << " -- " << cmd;

    QRegularExpression dbRegex("(?<=ƒ)[^ƒ]*(?=ƒ)");
    QRegularExpressionMatch dbMatch;
    QStringList dbList;
    QSqlQuery sqlQuery;
    SqlRecordQString record;

    if (cmdName == "ADD-ROOM")
    {
        dbMatch = dbRegex.match(cmdArgs);
        dbList = dbMatch.captured(0).split("‡");

        sqlQuery.prepare("INSERT INTO rooms (id, name, photoADDRESS, info, type, pin) "
                         "VALUES (?, ?, ?, ?, ?, ?)");
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.addBindValue(dbList[3]);
        sqlQuery.addBindValue(dbList[4]);
        sqlQuery.addBindValue(dbList[5]);
        sqlQuery.exec();

        sqlQuery.prepare("INSERT INTO participants (userID, roomID, role) "
                         "VALUES (?, ?, ?)");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.addBindValue("M");
        sqlQuery.exec();
    }
    else if (cmdName == "ADD-USER")
    {
        dbMatch = dbRegex.match(cmdArgs);
        dbList = dbMatch.captured(0).split("‡");

        sqlQuery.prepare("INSERT INTO users (username, password, emailAddress, phoneNumber, name, photoADDRESS,"
                         " info, isOnline, messageIndex) VALUES (?, 'Zc123$', ?, ?, ?, ?, ?, ?, 0)");
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.addBindValue(dbList[3]);
        sqlQuery.addBindValue(dbList[4]);
        sqlQuery.addBindValue(dbList[5]);
        sqlQuery.addBindValue(dbList[6]);
        sqlQuery.exec();
    }
    else if (cmdName == "ADD-MESSAGE")
    {
        dbMatch = dbRegex.match(cmdArgs);
        dbList = dbMatch.captured(0).split("‡");

        sqlQuery.prepare("INSERT INTO messages (id, roomID, userID, key, DT, replyID, text, "
                         "imageADDRESS, videoADDRESS, audioADDRESS, fileADDRESS) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.addBindValue(dbList[3]);
        sqlQuery.addBindValue(dbList[4]);
        sqlQuery.addBindValue(dbList[5]);
        sqlQuery.addBindValue(dbList[6]);
        sqlQuery.addBindValue(dbList[7]);
        sqlQuery.addBindValue(dbList[8]);
        sqlQuery.addBindValue(dbList[9]);
        sqlQuery.addBindValue(dbList[10]);
        sqlQuery.exec();

        sqlQuery.prepare("UPDATE users SET messageIndex=messageIndex+1 WHERE username=?");
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=? AND NOT userID=?");
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            emit command(cmd, sqlQuery.value("userID").toString());
        } while (sqlQuery.next());
    }
    else if (cmdName == "REMOVE-MESSAGE")
    {
        sqlQuery.prepare("SELECT roomID FROM messages WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        QString _roomID = sqlQuery.value("roomID").toString();

        sqlQuery.prepare("DELETE FROM messages WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=? AND NOT userID=?");
        sqlQuery.addBindValue(_roomID);
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            emit command(cmd, sqlQuery.value("userID").toString());
        } while (sqlQuery.next());
    }
    else if (cmdName == "REMOVE-USER")
    {
        QStringList _roomID;

        sqlQuery.prepare("SELECT roomID FROM participants WHERE userID=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (sqlQuery.first())
        {
            do
            {
                _roomID << sqlQuery.value("roomID").toString();
            } while (sqlQuery.next());
        }

        foreach (QString i, _roomID)
        {
            sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=?");
            sqlQuery.addBindValue(i);
            sqlQuery.exec();
            if (!sqlQuery.first())
            {
                continue;
            }

            do
            {
                emit command(cmd, sqlQuery.value("userID").toString());
            } while (sqlQuery.next());
        }

        sqlQuery.prepare("SELECT roomID FROM participants WHERE userID=? AND role='M'");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (sqlQuery.first())
        {
            do
            {
                _roomID << sqlQuery.value("roomID").toString();
            } while (sqlQuery.next());
        }

        foreach (QString i, _roomID)
        {
            sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=?");
            sqlQuery.addBindValue(i);
            sqlQuery.exec();
            if (!sqlQuery.first())
            {
                continue;
            }

            do
            {
                emit command("REMOVE-ROOM " + i, sqlQuery.value("userID").toString());
            } while (sqlQuery.next());

            sqlQuery.prepare("DELETE FROM rooms WHERE id=?");
            sqlQuery.addBindValue(i);
            sqlQuery.exec();
        }

        sqlQuery.prepare("DELETE FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
    }
    else if (cmdName == "REMOVE-PARTICIPANT")
    {
        sqlQuery.prepare("SELECT role FROM participants WHERE roomID=? AND userID=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.addBindValue(cmdArgs.split(" ").last());
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        if (sqlQuery.value("role").toString() == "M")
        {
            sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=? ANd NOT userID=?");
            sqlQuery.addBindValue(cmdArgs.split(" ").first());
            sqlQuery.addBindValue(cmdArgs.split(" ").last());
            sqlQuery.exec();
            if (sqlQuery.first())
            {
                do
                {
                    emit command("REMOVE-ROOM " + cmdArgs.split(" ").first(), sqlQuery.value("userID").toString());
                } while (sqlQuery.next());
            }

            sqlQuery.prepare("DELETE FROM rooms WHERE id=?");
            sqlQuery.addBindValue(cmdArgs.split(" ").first());
            sqlQuery.exec();
        }
        else
        {
            sqlQuery.prepare("DELETE FROM participants WHERE roomID=? AND userID=?");
            sqlQuery.addBindValue(cmdArgs.split(" ").first());
            sqlQuery.addBindValue(cmdArgs.split(" ").last());
            sqlQuery.exec();

            sqlQuery.prepare("SELECT COUNT(userID) FROM participants WHERE roomID=?");
            sqlQuery.addBindValue(cmdArgs.split(" ").first());
            sqlQuery.exec();
            sqlQuery.first();
            int sqlsize = sqlQuery.value("COUNT(userID)").toInt();

            if (sqlsize == 0)
            {
                sqlQuery.prepare("DELETE FROM rooms WHERE id=?");
                sqlQuery.addBindValue(cmdArgs.split(" ").first());
                sqlQuery.exec();

                return;
            }

            sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=?");
            sqlQuery.addBindValue(cmdArgs.split(" ").first());
            sqlQuery.exec();
            sqlQuery.first();

            do
            {
                emit command(cmd, sqlQuery.value("userID").toString());
            } while (sqlQuery.next());
        }
    }
    else if (cmdName == "REMOVE-ROOM")
    {
        sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=? AND NOT userID=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            emit command(cmd, sqlQuery.value("userID").toString());
        } while (sqlQuery.next());

        sqlQuery.prepare("DELETE FROM rooms WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
    }
    else if (cmdName == "EDIT-ROOM")
    {
        dbMatch = dbRegex.match(cmdArgs);
        dbList = dbMatch.captured(0).split("‡");

        sqlQuery.prepare("UPDATE rooms SET name=?, photoADDRESS=?, info=?, pin=? WHERE id=?");
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.addBindValue(dbList[3]);
        sqlQuery.addBindValue(dbList[5]);
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=?");
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            emit command(cmd, sqlQuery.value("userID").toString());
        } while (sqlQuery.next());
    }
    else if (cmdName == "EDIT-USER")
    {
        QStringList _roomID;

        dbMatch = dbRegex.match(cmdArgs);
        dbList = dbMatch.captured(0).split("‡");

        sqlQuery.prepare("UPDATE users SET emailAddress=?, phoneNumber=?, name=?, "
                         "photoADDRESS=?, info=?, isOnline=? WHERE username=?");
        sqlQuery.addBindValue(dbList[1]);
        sqlQuery.addBindValue(dbList[2]);
        sqlQuery.addBindValue(dbList[3]);
        sqlQuery.addBindValue(dbList[4]);
        sqlQuery.addBindValue(dbList[5]);
        sqlQuery.addBindValue(dbList[6]);
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT roomID FROM participants WHERE userID=?");
        sqlQuery.addBindValue(dbList[0]);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            _roomID << sqlQuery.value("roomID").toString();
        } while (sqlQuery.next());

        foreach (QString i, _roomID)
        {
            sqlQuery.prepare("SELECT userID FROM participants WHERE roomID=? AND NOT userID=?");
            sqlQuery.addBindValue(i);
            sqlQuery.addBindValue(dbList[0]);
            sqlQuery.exec();
            if (!sqlQuery.first())
            {
                continue;
            }

            do
            {
                emit command(cmd, sqlQuery.value("userID").toString());
            } while (sqlQuery.next());
        }
    }
    else if(cmdName == "DOWNLOAD")
    {
        if (cmdArgs.split(".").first().back() == "I")
        {
            emit command("_UPLOAD_ Images/" + cmdArgs, senderUsername);
        }
        else if (cmdArgs.split(".").first().back() == "V")
        {
            emit command("_UPLOAD_ Videos/" + cmdArgs, senderUsername);
        }
        else if (cmdArgs.split(".").first().back() == "A")
        {
            emit command("_UPLOAD_ Audios/" + cmdArgs, senderUsername);
        }
        else if (cmdArgs.split(".").first().back() == "F")
        {
            emit command("_UPLOAD_ Files/" + cmdArgs, senderUsername);
        }
        else
        {
            emit command("_UPLOAD_ Profiles/" + cmdArgs, senderUsername);
        }
    }
    else if(cmdName == "UPDATE-DB")
    {

    }
    else if(cmdName == "SET-PASS")
    {
        sqlQuery.prepare("UPDATE users SET password=? WHERE username=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").last());
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.exec();
    }
    else if (cmdName == "ROOM-USER")
    {
        sqlQuery.prepare("SELECT * FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        record << sqlQuery.value("username").toString() << sqlQuery.value("emailAddress").toString()
               << sqlQuery.value("phoneNumber").toString() << sqlQuery.value("name").toString()
               << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
               << sqlQuery.value("isOnline").toString();
        record.end();

        emit command("_UPLOAD_ Profiles/" + sqlQuery.value("photoADDRESS").toString(), senderUsername);
        emit command("ADD-USER " + QString(record), senderUsername);

        record.clean();

        sqlQuery.prepare("SELECT * FROM users WHERE username=?");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        record << sqlQuery.value("username").toString() << sqlQuery.value("emailAddress").toString()
               << sqlQuery.value("phoneNumber").toString() << sqlQuery.value("name").toString()
               << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
               << sqlQuery.value("isOnline").toString();
        record.end();

        emit command("_UPLOAD_ Profiles/" + sqlQuery.value("photoADDRESS").toString(), cmdArgs);
        emit command("ADD-USER " + QString(record), cmdArgs);

        record.clean();

        QDateTime dt = QDateTime::currentDateTimeUtc();
        QString roomID = dt.toString("dd.MM.yyyy-hh.mm.ss");

        sqlQuery.prepare("INSERT INTO rooms (id, type) VALUES (?, 0)");
        sqlQuery.addBindValue(roomID);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT * FROM rooms WHERE id=?");
        sqlQuery.addBindValue(roomID);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        record << sqlQuery.value("id").toString() << "" << "" << ""
               << sqlQuery.value("type").toString() << "";
        record.end();

        emit command("ADD-ROOM " + QString(record), senderUsername);
        emit command("ADD-ROOM " + QString(record), cmdArgs);

        sqlQuery.prepare("INSERT INTO participants (userID, roomID, role) VALUES (?, ?, 'G')");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.addBindValue(roomID);
        sqlQuery.exec();

        sqlQuery.prepare("INSERT INTO participants (userID, roomID, role) VALUES (?, ?, 'G')");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.addBindValue(roomID);
        sqlQuery.exec();

        sqlQuery.prepare("SELECT * FROM participants WHERE roomID=?");
        sqlQuery.addBindValue(roomID);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            record.clean();

            record << sqlQuery.value("userID").toString() << sqlQuery.value("roomID").toString()
                   << sqlQuery.value("role").toString();
            record.end();

            emit command("ADD-PARTICIPANT " + QString(record), senderUsername);
            emit command("ADD-PARTICIPANT " + QString(record), cmdArgs);
        } while (sqlQuery.next());
    }
    else if (cmdName == "ENTER-ROOM")
    {
        SqlRecordQString senderUserRecord;

        sqlQuery.prepare("SELECT * FROM users WHERE username=?");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        senderUserRecord << sqlQuery.value("username").toString() << sqlQuery.value("emailAddress").toString()
                         << sqlQuery.value("phoneNumber").toString() << sqlQuery.value("name").toString()
                         << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
                         << sqlQuery.value("isOnline").toString();
        senderUserRecord.end();

        QString photo = sqlQuery.value("photoADDRESS").toString();

        sqlQuery.prepare("SELECT * FROM rooms WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        record << sqlQuery.value("id").toString() << sqlQuery.value("name").toString()
               << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
               << sqlQuery.value("type").toString() << sqlQuery.value("pin").toString();
        record.end();

        emit command("_UPLOAD_ Profiles/" + sqlQuery.value("photoADDRESS").toString(), senderUsername);
        emit command("ADD-ROOM " + QString(record), senderUsername);

        record.clean();

        sqlQuery.prepare("SELECT * FROM users INNER JOIN participants "
                         "ON participants.userID=users.username "
                         "WHERE participants.roomID=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        do
        {
            record << sqlQuery.value("username").toString() << sqlQuery.value("emailAddress").toString()
                   << sqlQuery.value("phoneNumber").toString() << sqlQuery.value("name").toString()
                   << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
                   << sqlQuery.value("isOnline").toString();
            record.end();

            emit command("_UPLOAD_ Profiles/" + sqlQuery.value("photoADDRESS").toString(), senderUsername);
            emit command("ADD-USER " + QString(record), senderUsername);

            record.clean();

            record << sqlQuery.value("userID").toString() << sqlQuery.value("roomID").toString()
                   << sqlQuery.value("role").toString();
            record.end();

            emit command("ADD-PARTICIPANT " + QString(record), senderUsername);

            record.clean();

            record << senderUsername << cmdArgs << "G";
            record.end();

            emit command("_UPLOAD_ Profiles/" + photo, sqlQuery.value("username").toString());
            emit command("ADD-USER " + QString(senderUserRecord), sqlQuery.value("username").toString());
            emit command("ADD-PARTICIPANT " + QString(record), sqlQuery.value("username").toString());

            record.clean();
        } while(sqlQuery.next());

        sqlQuery.prepare("INSERT INTO participants (userID, roomID, role) VALUES (?, ?, 'G')");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();

        record << senderUsername << cmdArgs << "G";
        record.end();

        emit command("ADD-PARTICIPANT " + QString(record), senderUsername);
    }
    else if (cmdName == "UN-EXIST")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QString data;

        if (sqlsize == 0)
        {
            data = "UN-EXIST-RESULT " + cmdArgs + " 0";
        }
        else // > 0 and -1
        {
            data = "UN-EXIST-RESULT " + cmdArgs + " 1";
        }

        emit command(data, senderUsername);
    }
    else if (cmdName == "ROOM-EXIST")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM rooms WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QString data;

        if (sqlsize == 0)
        {
            data = "ROOM-EXIST-RESULT " + cmdArgs + " 0";
        }
        else // > 0 and -1
        {
            data = "ROOM-EXIST-RESULT " + cmdArgs + " 1";
        }

        emit command(data, senderUsername);
    }
    else if(cmdName == "LOGIN")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QString data;

        sqlQuery.prepare("SELECT password FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.exec();
        sqlQuery.first();

        if (sqlsize == 0)
        {
            data = "LOGIN-RESULT 0";
        }
        else if (sqlQuery.value("password").toString() == cmdArgs.split(" ").last())
        {
            QTcpSocket *s = socketList[senderUsername];

            socketList.remove(senderUsername);

            socketList.insert(cmdArgs.split(" ").first(), s);

            senderUsername = cmdArgs.split(" ").first();

            foreach (const QStringList &sList, offlineJob)
            {
                if (sList[0] == senderUsername)
                {
                    job.push_front(sList);
                    offlineJob.removeOne(sList);
                }
            }

            data = "LOGIN-RESULT 1";
        }
        else
        {
            data = "LOGIN-RESULT 2";
        }

        emit command(data, senderUsername);
    }
    else if (cmdName == "MESSAGE-INDEX")
    {
        QString data;

        sqlQuery.prepare("SELECT messageIndex FROM users WHERE username=?");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        data = "MESSAGE-INDEX-RESULT %1"+ sqlQuery.value("messageIndex").toString();

        emit command(data, senderUsername);
    }
    else if (cmdName == "ARRIVE")
    {
        for (int i = j_delPending.size() - 1; i >= 0; --i)
        {
            if (j_delPending[i][0] == senderUsername)
            {
                j_delPending.removeAt(i);

                break;
            }
        }
    }

    if (cmdName != "ARRIVE")
    {
        emit command("ARRIVE", senderUsername);
    }
}

void ServerProcess::sendRun()
{
    QString final_cmd;
    QString final_user;
    QByteArray header;
    QByteArray data;

    if (fJob.size() > 0)
    {
        final_user = fJob.last()[0];
        final_cmd = fJob.last()[1];

        fJob.pop_back();
    }
    else if (job.size() > 0)
    {
        final_user = job.last()[0];
        final_cmd = job.last()[1];

        job.pop_back();
    }
    else
    {
        return;
    }

    QStringList _content = {final_user, final_cmd};

    if (!socketList.contains(final_user))
    {
        offlineJob.append(_content);

        return;
    }

    qDebug() << "SEND" << final_user << " -- " << final_cmd;

    if (final_cmd.split(" ").first() == "_UPLOAD_")
    {
        if (final_cmd.split(" ").last().isEmpty())
        {
            return;
        }

        QString filePath = QString::fromStdString(final_cmd.toStdString().substr(static_cast<size_t>(final_cmd.indexOf(" ")) + 1));
        QFile _file(filePath);
        if (!_file.open(QIODevice::ReadOnly))
        {
            qDebug() << "File not found -> " << filePath;

            return;
        }

        j_delPending.push_front(_content);

        data = _file.readAll();
        _file.close();

        header.append(char(1));
        header.append(filePath.split("/").last().split(".").first().toUtf8());
        header.resize(61);
        header.append(filePath.split("/").last().split(".").last().toUtf8());
        header.resize(71);
    }
    else
    {
        j_delPending.push_front(_content);

        data = final_cmd.toUtf8();

        header.append(char(0));
        header.append("");
        header.resize(61);
        header.append("");
        header.resize(71);
    }

    header.append(data);

    emit sendData(header, socketList[final_user]);
}

void ServerProcess::disconnectedProc()
{
    QTcpSocket *socket = reinterpret_cast<QTcpSocket *>(sender());

    foreach (const QString &socketList_key, socketList.keys())
    {
        if (socketList[socketList_key] == socket)
        {
            qDebug() << "del connection === " << socketList_key;

            socketList.remove(socketList_key);
            break;
        }
    }

    socket->deleteLater();
}

void ServerProcess::newConnection()
{
    while (server->hasPendingConnections())
        appendToSocketList(server->nextPendingConnection());
}

void ServerProcess::readCMD()
{
    QString cmd = cmdFile->readLine();

    if (cmd.size() == 0)
    {
        return;
    }

    if (cmd.back() == '\n')
    {
        cmdFile->resize(0);
        cmd = cmd.trimmed();

        if (cmd == "exit")
        {
            app->exit(0);
        }
        else if (cmd == "offjob-size")
        {
            qDebug() << "---------------offjob_size---------------" << offlineJob.size();
        }
    }
}

void ServerProcess::appendToSocketList(QTcpSocket* socket)
{
    socketList.insert(QString::number(socket->socketDescriptor()), socket);
    connect(socket, &QTcpSocket::disconnected, this, &ServerProcess::disconnectedProc);

    qDebug() << "new connection === " << socket->socketDescriptor();
}
