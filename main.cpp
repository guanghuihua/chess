#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("GuanghuiEducationLab");
    QCoreApplication::setApplicationName("XiangqiTraining");
    MainWindow w;
    w.show();
    return a.exec();
}
