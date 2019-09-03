#include "mainwindow.h"
#include "ui_mainwindow.h"
#if defined(Q_PROCESSOR_ARM)
    #include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#endif
#include <signal.h>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>
#include <QThread>
#include <QDebug>

#define MIN_INTERVAL 3000 // in ms


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , pUi(new Ui::MainWindow)
    , pImageRecorder(Q_NULLPTR)
    // ================================================
    // GPIO Numbers are Broadcom (BCM) numbers
    // For Raspberry Pi GPIO pin numbering see
    // https://pinout.xyz/
    // ================================================
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
    // ================================================
    , gpioLEDpin(23)
    , gpioHostHandle(-1)
    , msecInterval(10000)
    , msecTotTime(0)
{
    pUi->setupUi(this);

    sBaseDir     = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    sOutFileName = QString("test");

    // Setup the QLineEdit styles
    sNormalStyle = pUi->lampStatus->styleSheet();
    sErrorStyle  = "QLineEdit { \
                        color: rgb(255, 255, 255); \
                        background: rgb(255, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
    sDarkStyle   = "QLineEdit { \
                        color: rgb(255, 255, 255); \
                        background: rgb(0, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
    sPhotoStyle  = "QLineEdit { \
                        color: rgb(0, 0, 0); \
                        background: rgb(255, 255, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";

#if defined(Q_PROCESSOR_ARM)
    gpioHostHandle = pigpio_start(QString("localhost").toLocal8Bit().data(),
                                  QString("8888").toLocal8Bit().data());
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
#endif

    switchLampOff();

    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval, 8));
    pUi->tTimeEdit->setText(QString("%1").arg(msecTotTime, 8));

    // Restore Geometry and State of the window
    QSettings settings;
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    connect(&intervalTimer,
            SIGNAL(timeout()),
            this,
            SLOT(onTimeToGetNewImage()));
}


void
MainWindow::closeEvent(QCloseEvent *event) {
    Q_UNUSED(event)
    intervalTimer.stop();
    if(pImageRecorder) {
        kill(pid, SIGKILL);
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = nullptr;
    }
    switchLampOff();
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);// This does'nt works
#endif
}


void
MainWindow::switchLampOn() {
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 1);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 On")
                                   .arg(gpioLEDpin));
#endif
    pUi->lampStatus->setStyleSheet(sPhotoStyle);
    repaint();
}


void
MainWindow::switchLampOff() {
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 0);
    else
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Off")
                                   .arg(gpioLEDpin));
#endif
    pUi->lampStatus->setStyleSheet(sDarkStyle);
    repaint();
}


bool
MainWindow::checkValues() {
    return true;
}


void
MainWindow::on_startButton_clicked() {
    if(!checkValues()) {
        pUi->statusBar->showMessage((QString("Error: Check Values !")));
        return;
    }
    imageNum = 0;
    QString sCommand = QString("raspistill -s ");
    sCommand += QString("-n ");// No preview
    sCommand += QString("-ex auto ");// Exposure mode; Auto
    sCommand += QString("-awb auto ");// White Balance; Auto
//    sCommand += QString("-awb fluorescent ");// White Balance; Fluorescent tube
    sCommand += QString("-drc off ");// Dynamic Range Compression: off
    sCommand += QString("-q 100 ");// JPEG quality: 100=max
    sCommand += QString("-t %1 ").arg(0);
    sCommand += QString("-o %1/%2_%04d.jpg").arg(sBaseDir).arg(sOutFileName);

#if defined(Q_PROCESSOR_ARM)
    pImageRecorder = new QProcess(this);
    connect(pImageRecorder,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(onImageRecorderClosed(int, QProcess::ExitStatus)));
    pImageRecorder->start(sCommand);
    if(!pImageRecorder->waitForStarted(3000)) {
        pUi->statusBar->showMessage(QString("Non riesco ad eseguire 'raspistill'."));
        pImageRecorder->terminate();
        delete pImageRecorder;
        pImageRecorder = nullptr;
        return;
    }
    pid = pid_t(pImageRecorder->processId());
#endif

    pUi->statusBar->showMessage(sCommand);
    pUi->startButton->setDisabled(true);
    pUi->stopButton->setEnabled(true);
    onTimeToGetNewImage();
    intervalTimer.start(msecInterval);
}


void
MainWindow::on_stopButton_clicked() {
    intervalTimer.stop();
    if(pImageRecorder) {
        int iErr = kill(pid, SIGKILL);
        if(iErr == -1) {
            pUi->statusBar->showMessage(QString("Error %1 in sending SIGKILL signal").arg(iErr));
        }
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = nullptr;
    }
    switchLampOff();
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
}


void
MainWindow::on_intervalEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() < MIN_INTERVAL) {
        pUi->intervalEdit->setStyleSheet(sErrorStyle);
    } else {
        msecInterval = arg1.toInt();
        pUi->intervalEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainWindow::on_intervalEdit_editingFinished() {
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval, 8));
    pUi->intervalEdit->setStyleSheet(sNormalStyle);
}


void
MainWindow::on_tTimeEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() < 0) {
        msecTotTime = arg1.toInt();
        pUi->tTimeEdit->setStyleSheet(sNormalStyle);
    } else {
        pUi->tTimeEdit->setStyleSheet(sErrorStyle);
    }
}


void
MainWindow::on_tTimeEdit_editingFinished() {
    pUi->tTimeEdit->setText(QString("%1").arg(msecTotTime, 8));
    pUi->tTimeEdit->setStyleSheet(sNormalStyle);
}


void
MainWindow::onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
}


void
MainWindow::onTimeToGetNewImage() {
    switchLampOn();
    QThread::msleep(300);
    int iErr = kill(pid, SIGUSR1);
    if(iErr == -1) {
        pUi->statusBar->showMessage(QString("Error %1 in sending SIGUSR1 signal").arg(iErr));
    }
    QThread::msleep(300);
    switchLampOff();
}
