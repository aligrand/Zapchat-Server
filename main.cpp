#include <QCoreApplication>
#include <QObject>
#include <signal.h>

#include "serverprocess.h"

QCoreApplication *app;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    app = &a;

    QSqlDatabase db;
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("hostDB.sqlite3");
    db.open();

    QSqlQuery sqlQuery;
    sqlQuery.prepare("PRAGMA foreign_keys = 1;");
    sqlQuery.exec();

    ServerProcess *sp = new ServerProcess(&a);

    return a.exec();
}
