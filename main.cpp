#include <QCoreApplication>

#include "serverprocess.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QSqlDatabase db;
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("hostDB.sqlite3");
    db.open();

    QSqlQuery sqlQuery;
    sqlQuery.prepare("PRAGMA foreign_keys = 1;");
    sqlQuery.exec();

    ServerProcess *sp = new ServerProcess;

    return a.exec();
}
