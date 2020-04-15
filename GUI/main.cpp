// Wearable Sensing LSL GUI
// Copyright (C) 2014-2020 Syntrogi Inc dba Intheon.

#include "mainwindow.h"
#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
