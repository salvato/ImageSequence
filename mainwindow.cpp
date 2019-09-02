#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry

#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pImageRecorder(Q_NULLPTR)
    , gpioLEDpin(23)
    // GPIO Numbers are Broadcom (BCM) numbers
    // For Raspberry Pi GPIO pin numbering see
    // https://pinout.xyz/
    //
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
    , gpioHostHandle(-1)
{
    ui->setupUi(this);

    gpioHostHandle = pigpio_start(reinterpret_cast<const char*>("localhost"),
                                  reinterpret_cast<const char*>("8888"));
    if(gpioHostHandle < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error !"),
                              QString("Non riesco ad inizializzare la GPIO."));
        exit(EXIT_FAILURE);
    }
    if(set_mode(gpioHostHandle, gpioLEDpin, PI_OUTPUT) < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to initialize GPIO%1 as Output")
                                   .arg(gpioLEDpin));
        exit(EXIT_FAILURE);
    }

    if(set_pull_up_down(gpioHostHandle, gpioLEDpin, PI_PUD_UP) < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Pull-Up")
                                   .arg(gpioLEDpin));
        exit(EXIT_FAILURE);
    }
    switchLampOff();

    QString sCommand = QString();

    pImageRecorder = new QProcess(this);
    connect(pImageRecorder,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(onImageRecorderClosed(int, QProcess::ExitStatus)));
    pImageRecorder->start(sCommand);
    if(!pImageRecorder->waitForStarted(3000)) {
        QMessageBox::critical(this,
                              QString("raspistill Error"),
                              QString("Non riesco ad eseguire 'raspistill'."));
        pImageRecorder->terminate();
        delete pImageRecorder;
        pImageRecorder = nullptr;
        exit(EXIT_FAILURE);
    }
    pid = pImageRecorder->processId();
}


MainWindow::~MainWindow() {
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
    if(pImageRecorder) {
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = Q_NULLPTR;
    }
    delete ui;
}


void
MainWindow::switchLampOn() {
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 1);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 On")
                                   .arg(gpioLEDpin));
}


void
MainWindow::switchLampOff() {
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 0);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Off")
                                   .arg(gpioLEDpin));
}

