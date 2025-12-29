/* 
 * Wearable Sensing LSL GUI
 *
 * Please create a GitHub Issue or contact support@wearablesensing.com if you
 * encounter any issues or would like to request new features.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QtGui>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QProcess>


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
    
    /* For checking impedance */
    void onZCheckBoxToggled(bool checked);
    void handleZCheckBoxToggled();

    /* For resetting impedance */
    void onResetZButtonClicked();

private:
    Ui::MainWindow *ui;
    QProcess *streamer;
    int timerId;
    int counter;
    QProgressBar *progressBar;

    /* For checking impedance */
    QCheckBox *ZCheckBox;
    bool zCheckState;

    /* For resetting impedance */
    QPushButton *resetZButton; /* Pointer to your resetZButton */

};

#endif /* MAINWINDOW_H */