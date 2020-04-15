// Wearable Sensing LSL GUI
// Copyright (C) 2014-2020 Syntrogi Inc dba Intheon.

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QtGui>


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT


public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void writeToConsole();
    QStringList parseArguments();
    void timerEvent(QTimerEvent *event);

private:
    Ui::MainWindow *ui;
    QProcess *streamer;
    int timerId;
    int counter;
    QProgressBar *progressBar;
};

#endif // MAINWINDOW_H
