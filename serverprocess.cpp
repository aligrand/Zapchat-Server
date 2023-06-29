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
    connect(this, &ServerProcess::dataArrived, this, &ServerProcess::sendRun);
    connect(this, &ServerProcess::start_sendRun, this, &ServerProcess::sendRun);
    connect(server, &QTcpServer::newConnection, this, &ServerProcess::newConnection);

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

    readyReadTimer.start(10);
}

ServerProcess::~ServerProcess()
{
    foreach (const QString &socketList_key, socketList.keys())
    {
        socketList[socketList_key]->close();
        socketList[socketList_key]->deleteLater();

        socketList.remove(socketList_key);
        dataSizeList.remove(socketList_key);
    }

    QFile _jFile("job-queue.txt");
    _jFile.open(QIODevice::WriteOnly | QIODevice::Text);
    _jFile.resize(0);
    while (job_delPending.size() > 0)
    {
        _jFile.write((job_delPending.last()[0] + " " + job_delPending.last()[1] + "\n").toStdString().c_str());
        job_delPending.pop_back();
    }
    while (job.size() > 0)
    {
        _jFile.write((job.last()[0] + " " + job.last()[1] + "\n").toStdString().c_str());
        job.pop_back();
    }
    _jFile.close();
}

void ServerProcess::sendDataProc(QByteArray sData, QTcpSocket *socket, int &dataSize)
{
    QDataStream socketStream(socket);

    socketStream << sData;
    dataSize = static_cast<int>(socket->bytesToWrite());
}

void ServerProcess::commandProc(QString cmd, QString receiverUsername)
{
    QStringList _content = {receiverUsername, cmd};

    if (job.size() == 0)
    {
        job.push_front(_content);

        emit start_sendRun();
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
            return;
        }

        QDataStream socketStream(socketList[socketList_key]);
        socketStream.setVersion(QDataStream::Qt_5_13);

        socketStream.startTransaction();
        socketStream >> data;

        if (!socketStream.commitTransaction())
        {
            return;
        }

        if (data.at(0) == 0)
        {
            emit messageAsCommand(data, socketList_key);
        }
        else
        {
            emit messageAsData(data);
        }
    }
}

void ServerProcess::messageAsDataProc(QByteArray rData)
{
    QString fileName, fileFormat, dir;

    fileName = rData.mid(1, 60);
    fileFormat = rData.mid(61, 10);

    qDebug() << fileName << "." << fileFormat;

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

    qDebug() << senderUsername << " -- " << cmd;

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

        sqlQuery.prepare("UPDATE messages SET messageIndex=messageIndex+1 WHERE username=?");
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
        sqlQuery.first();
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
        sqlQuery.first();

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
                return;
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

        emit command("ADD-USER " + QString(record), cmdArgs);

        record.clean();

        QDateTime dt = QDateTime::currentDateTimeUtc();
        QString roomID = dt.toString("dd.MM.yyyy-hh:mm:ss");

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

        record << sqlQuery.value("id").toString() << sqlQuery.value("name").toString()
               << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
               << sqlQuery.value("type").toString() << sqlQuery.value("pin").toString();
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

        record.clean();

        do
        {
            record << sqlQuery.value("userID").toString() << sqlQuery.value("roomID").toString()
                   << sqlQuery.value("role").toString();
            record.end();

            emit command("ADD-PARTICIPANTS " + QString(record), senderUsername);
            emit command("ADD-PARTICIPANTS " + QString(record), cmdArgs);
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

        sqlQuery.prepare("SELECT * FROM ((rooms "
                         "INNER JOIN participants ON participants.roomID=rooms.id) "
                         "INNER JOIN users ON users.username=participants.userID)"
                         "WHERE rooms.id=?");
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

        emit command("ADD-ROOM " + QString(record), senderUsername);

        record.clean();

        do
        {
            record << sqlQuery.value("username").toString() << sqlQuery.value("emailAddress").toString()
                   << sqlQuery.value("phoneNumber").toString() << sqlQuery.value("name").toString()
                   << sqlQuery.value("photoADDRESS").toString() << sqlQuery.value("info").toString()
                   << sqlQuery.value("isOnline").toString();
            record.end();

            emit command("ADD-USER " + QString(record), senderUsername);

            record.clean();

            record << sqlQuery.value("userID").toString() << sqlQuery.value("roomID").toString()
                   << sqlQuery.value("role").toString();
            record.end();

            emit command("ADD-PARTICIPANT " + QString(record), senderUsername);

            record.clean();

            record << senderUsername << cmdArgs << "G";
            record.end();

            emit command("ADD-USER " + QString(senderUserRecord), sqlQuery.value("username").toString());
            emit command("ADD-PARTICIPANTS " + QString(record), sqlQuery.value("username").toString());

            record.clean();
        } while(sqlQuery.next());

        sqlQuery.prepare("INSERT INTO participants (userID, roomID, role) VALUES (?, ?, 'G')");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();

        record << senderUsername << cmdArgs << "G";
        record.end();

        emit command("ADD-PARTICIPANTS " + QString(record), senderUsername);
    }
    else if (cmdName == "UN-EXIST")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QByteArray data;
        QByteArray header;

        if (sqlsize == 0)
        {
            data = QString("UN-EXIST-RESULT %1 0").arg(cmdArgs).toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }
        else // > 0 and -1
        {
            data = QString("UN-EXIST-RESULT %1 1").arg(cmdArgs).toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }

        emit sendData(header, socketList[senderUsername], dataSizeList[senderUsername]);
    }
    else if (cmdName == "ROOM-EXIST")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM rooms WHERE id=?");
        sqlQuery.addBindValue(cmdArgs);
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QByteArray data;
        QByteArray header;

        if (sqlsize == 0)
        {
            data = QString("ROOM-EXIST-RESULT %1 0").arg(cmdArgs).toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }
        else // > 0 and -1
        {
            data = QString("ROOM-EXIST-RESULT %1 1").arg(cmdArgs).toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }

        emit sendData(header, socketList[senderUsername], dataSizeList[senderUsername]);
    }
    else if(cmdName == "LOGIN")
    {
        sqlQuery.prepare("SELECT COUNT(*) FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.exec();
        sqlQuery.first();
        int sqlsize = sqlQuery.value("COUNT(*)").toInt();

        QByteArray data;
        QByteArray header;

        sqlQuery.prepare("SELECT password FROM users WHERE username=?");
        sqlQuery.addBindValue(cmdArgs.split(" ").first());
        sqlQuery.exec();
        sqlQuery.first();

        if (sqlsize == 0)
        {
            data = QString("LOGIN-RESULT 0").toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }
        else if (sqlQuery.value("password").toString() == cmdArgs.split(" ").last())
        {
            QTcpSocket *s = socketList[senderUsername];
            int ds = dataSizeList[senderUsername];

            socketList.remove(senderUsername);
            dataSizeList.remove(senderUsername);

            socketList.insert(cmdArgs.split(" ").first(), s);
            dataSizeList.insert(cmdArgs.split(" ").first(), ds);

            senderUsername = cmdArgs.split(" ").first();

            data = QString("LOGIN-RESULT 1").toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }
        else
        {
            data = QString("LOGIN-RESULT 2").toUtf8();

            header.append(char(0));
            header.append("");
            header.resize(61);
            header.append("");
            header.resize(71);
            header.append(data);
        }

        emit sendData(header, socketList[senderUsername], dataSizeList[senderUsername]);
    }
    else if (cmdName == "MESSAGE-INDEX")
    {
        QByteArray data;
        QByteArray header;

        sqlQuery.prepare("SELECT messageIndex FROM users WHERE username=?");
        sqlQuery.addBindValue(senderUsername);
        sqlQuery.exec();
        if (!sqlQuery.first())
        {
            return;
        }

        data = QString("MESSAGE-INDEX-RESULT %1").arg(sqlQuery.value("messageIndex").toString()).toUtf8();

        header.append(char(0));
        header.append("");
        header.resize(61);
        header.append("");
        header.resize(71);
        header.append(data);

        emit sendData(header, socketList[senderUsername], dataSizeList[senderUsername]);
    }
}

void ServerProcess::sendRun()
{
    QString final_cmd;
    QString final_user;
    QByteArray header;
    QByteArray data;

    if (job.size() == 0)
    {
        return;
    }

    final_user = job.last()[0];
    final_cmd = job.last()[1];

    QStringList _content = {final_user, final_cmd};

    if (!socketList.contains(final_user))
    {
        job.pop_back();
        job.push_front(_content);

        emit start_sendRun();
        return;
    }

    job.pop_back();
    job_delPending.push_front(_content);

    if (final_cmd.split(" ").first() == "_UPLOAD_")
    {
        QString filePath = QString::fromStdString(final_cmd.toStdString().substr(static_cast<size_t>(final_cmd.indexOf(" ")) + 1));
        QFile _file(filePath);
        if (!_file.open(QIODevice::ReadOnly))
        {
            qDebug() << "File not found -> " << filePath;

            return;
        }
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
        data = final_cmd.toUtf8();

        header.append(char(0));
        header.append("");
        header.resize(61);
        header.append("");
        header.resize(71);
    }

    header.append(data);

    emit sendData(header, socketList[final_user], dataSizeList[final_user]);
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
            dataSizeList.remove(socketList_key);
            break;
        }
    }

    socket->deleteLater();
}

void ServerProcess::dataArrivedProc(int channel, qint64 bytes)
{
    QTcpSocket *socket = reinterpret_cast<QTcpSocket *>(sender());

    dataSizeList[socketList.key(socket)] -= bytes;

    if (dataSizeList[socketList.key(socket)] == 0)
    {
        dataSizeList[socketList.key(socket)] = INT_MAX;

        for (int i = 0; i < job_delPending.length(); ++i)
        {
            if (job_delPending[i][0] == socketList.key(socket))
            {
                job_delPending.remove(i);
            }
        }

        emit dataArrived();
    }
}

void ServerProcess::newConnection()
{
    while (server->hasPendingConnections())
        appendToSocketList(server->nextPendingConnection());
}

void ServerProcess::appendToSocketList(QTcpSocket* socket)
{
    socketList.insert(QString::number(socket->socketDescriptor()), socket);
    dataSizeList.insert(QString::number(socket->socketDescriptor()), INT_MAX);
    connect(socket, &QTcpSocket::disconnected, this, &ServerProcess::disconnectedProc);
    connect(socket, &QTcpSocket::channelBytesWritten, this, &ServerProcess::dataArrivedProc);

    qDebug() << "new connection === " << socket->socketDescriptor();
}
