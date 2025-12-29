/* 
 * Wearable Sensing LSL GUI
 *
 * Please create a GitHub Issue or contact support@wearablesensing.com if you
 * encounter any issues or would like to request new features.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtGui>
#include <QProgressBar>
#include <QPushButton>
#include <QCheckBox>
#include <QByteArray>
#include <iostream>

#ifndef WIN32
#include <unistd.h>
#endif

/*
 * This program is a GUI for the dsi2lsl console application;
 * it works by running dsi2lsl as a subprocess, and passes in
 * the configuration as command-line arguments.
 */
#ifdef WIN32
const QString program = "dsi2lsl.exe";
#else
const QString program = "./dsi2lsl";
#endif

const QString port = "--port=";
const QString lslStream = "--lsl-stream-name=";
const QString montage = "--montage=";
const QString reference = "--reference=";
const QString defaultValule = "(use default)";


/**
 * Constructor for MainWindow
 * It initializes the UI, sets up the environment, and prepares the streamer process.
 * @param QWidget - The parent widget for the main window.
 * @return void
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    counter(0),
    zCheckState(false)
{
    ui->setupUi(this);

    /* Set LD_LIBRARY_PATH */
    #ifndef WIN32
    char cdir[256];
    std:setenv("LD_LIBRARY_PATH",getcwd(cdir, 256), 1);
    #endif

    ui->nameLineEdit->setText("WS-default");
    ui->montageLineEdit->setText("(use default)");
    ui->referenceLineEdit->setText("(use default)");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Start");
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("Stop");
    ui->statusBar->setVisible(false);
    this->ui->statusBar->showMessage("Streaming...");
    this->progressBar = new QProgressBar(this);
    ui->statusBar->addPermanentWidget(this->progressBar, 1);
    this->progressBar->setTextVisible(false);
    this->streamer = new QProcess(this);
    this->streamer->setProcessChannelMode(QProcess::MergedChannels);
    /* Connecting Impedance button */
    connect(ui->ZCheckBox, &QCheckBox::toggled, this, &MainWindow::onZCheckBoxToggled);
    connect(ui->ResetZButton, &QPushButton::clicked, this, &MainWindow::onResetZButtonClicked);
}


/**
 * Destructor for MainWindow
 * It stops the streamer process if it is running and deletes the UI.
 * @return void
 */
MainWindow::~MainWindow()
{
    this->on_buttonBox_rejected();
    delete ui;
}

/**
 * This function is called when the user clicks the "Z" button.
 * It toggles the state of the Z button and sends a command to the streamer process
 * to check the impedance status.
 * If the streamer is not running, it appends a message to the console.
 * @param checked - The state of the Z checkbox (true if checked, false if unchecked).
 * @return void
 */
void MainWindow::onZCheckBoxToggled(bool checked){
    this->zCheckState = checked;
}
void MainWindow::handleZCheckBoxToggled(){
    QByteArray command;
        if(this->zCheckState){
            command = "checkZOn\n";
            this->ui->console->append("\n---------- Impedance On -----------\n");
        }else{
            command = "checkZOff\n";
            this->ui->console->append("\n---------- Impedance Off ----------\n");
        }
        // Write the command to the process's standard input
        this->streamer->write(command);
}

/**
 * This function is called when the user clicks the "Reset Z" button.
 * It sends a command to the streamer process to reset the impedance status.
 * @return void
 */
void MainWindow::onResetZButtonClicked(){
    if (this->streamer && this->streamer->state() == QProcess::Running) {
        /* The command to send, including the newline character to simulate 'Enter'. */
        QByteArray command;

        command = "resetZ\n";

        /* Write the command to the process's standard input. */
        this->streamer->write(command);
        this->ui->console->append("\n---------- Reset ----------\n");

    } else {
        this->ui->console->append("Streamer is not running. Cannot send command.");
    }
}

/**
 * This function is called when the user clicks the "Start" button.
 * It checks if the streamer is already running, and if so, it stops it.
 * Then it sets up the input arguments for the streamer and starts it.
 * It also connects the output of the streamer to a slot that writes to the console.
 * 
 * @return void
 */
void MainWindow::on_buttonBox_accepted()
{
    if (this->streamer && this->streamer->state() == QProcess::Running) {
        this->on_buttonBox_rejected();
    }
    /* Set input arguments to the streamer */
    QStringList arguments = this->parseArguments();
    this->streamer->start(program, arguments);
    connect(this->streamer, SIGNAL(readyReadStandardOutput()), this, SLOT(writeToConsole()));
    handleZCheckBoxToggled(); /* Handle the Z checkbox state */
    this->counter = 0;
    this->timerId = this->startTimer(1000);
    this->ui->ZCheckBox->setEnabled(false); /* Enable the ZCheckBox */
}

/** 
 * This function is called when the timer event occurs.
 * It updates the progress bar and status bar message.
 * If the streamer process is not running, it stops the timer and resets the UI.
 * @param event - The timer event that triggered this function.
 * @return void
 */
void MainWindow::timerEvent(QTimerEvent *event)
{
    if(this->streamer->state()==QProcess::NotRunning)
    {
        this->on_buttonBox_rejected();
        return;
    }
    if(!this->ui->statusBar->isVisible())
        this->ui->statusBar->setVisible(true);
    this->counter += 33;
    if(this->counter > 100)
        this->counter = 0;
    this->progressBar->setValue(this->counter);
    this->ui->statusBar->showMessage("Streaming...");
}

/** 
 * This function reads the output from the streamer process and appends it to the console.
 * It is called whenever there is new data available from the streamer.
 * @return void
 */
void MainWindow::writeToConsole()
{
    while(this->streamer->canReadLine()){
        QString line = this->streamer->readLine(); /* Read the line into a string */
        if (!line.contains("netinterfaces.cpp") && !line.contains("udp_server.cpp") && !line.contains("common.cpp") && !line.contains("api_config.cpp")) { /* Check if the line contains "netinterfaces.cpp" */
            this->ui->console->append(line); /* If it doesn't, add it to the console */
        }
    }
}


/** 
 * This function is called when the user clicks the "Stop" button.
 * It stops the streamer process and resets the UI elements.
 * @return void
 */
void MainWindow::on_buttonBox_rejected()
{
    if(this->streamer != NULL){
        this->streamer->close();
        this->ui->console->append("Streamer will exit now. Good bye!");
        this->killTimer(this->timerId);
        this->counter = 0;
        this->ui->statusBar->setVisible(false);
        this->ui->ZCheckBox->setEnabled(true); /* Enable the ZCheckBox */
    }

}

/**
 * Parses the arguments from the GUI input fields and returns a QStringList
 * containing the command-line arguments for the dsi2lsl process.
 * @return QStringList - The list of arguments to be passed to the dsi2lsl process.
 */
QStringList MainWindow::parseArguments()
{
    QStringList arguments;

    arguments << (port+this->ui->portLineEdit->text().simplified()).toStdString().c_str()
              << (lslStream+this->ui->nameLineEdit->text().simplified()).toStdString().c_str();

    if(ui->montageLineEdit->text().simplified().compare(defaultValule))
        arguments << (montage+this->ui->montageLineEdit->text().simplified()).toStdString().c_str();

    if(ui->referenceLineEdit->text().simplified().compare(defaultValule))
        arguments << (reference+this->ui->referenceLineEdit->text().simplified()).toStdString().c_str();

    return arguments;
}