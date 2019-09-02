#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pigpiod_if2.h"// The libraries for using GPIO pins on Raspberry

#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pImageRecorder(Q_NULLPTR)
{
    ui->setupUi(this);

    gpioHostHandle = pigpio_start(reinterpret_cast<const char*>("localhost"),
                                  reinterpret_cast<const char*>("8888"));
    if(gpioHostHandle < 0) {
        QMessageBox::critical(this,
                              QString("gpiod Error !"),
                              QString("Non riesco ad inizializzare la GPIO."));
        exit(EXIT_FAILURE);
    }

    QString sCommand = QString();

    pImageRecorder = new QProcess(this);
    connect(pImageRecorder,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(onImageRecorderClosed(int, QProcess::ExitStatus)));
    pImageRecorder->start(sCommand);
    if(!pImageRecorder->waitForStarted(3000)) {
        QMessageBox::critical(this,
                              QString("raspistill Error !"),
                              QString("Non riesco ad eseguire 'raspistill'."));
        pImageRecorder->terminate();
        delete pImageRecorder;
        pImageRecorder = nullptr;
        exit(EXIT_FAILURE);
    }
    pid = pImageRecorder->processId();
}


MainWindow::~MainWindow() {
    delete ui;
}
