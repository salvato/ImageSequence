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
#include <QDir>


#define MIN_INTERVAL 3000 // in ms (depends on the image format: jpeg is HW accelerated !)
#define IMAGE_QUALITY 100 // 100 is Max quality


// GPIO Numbers are Broadcom (BCM) numbers
#define LED_PIN  23 // BCM23 is Pin 16 in the 40 pin GPIO connector.
#define PAN_PIN  14 // BCM14 is Pin  8 in the 40 pin GPIO connector.
#define TILT_PIN 26 // BCM26 IS Pin 37 in the 40 pin GPIO connector.


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , pUi(new Ui::MainWindow)
    , pImageRecorder(Q_NULLPTR)
    // ================================================
    // GPIO Numbers are Broadcom (BCM) numbers
    // ================================================
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
    // ================================================
    , gpioLEDpin(LED_PIN)
    , panPin(PAN_PIN)
    , tiltPin(TILT_PIN)
    , gpioHostHandle(-1)
{
    pUi->setupUi(this);

    // Values to be checked with the used servos
    PWMfrequency    =   50;   // in Hz
    pulseWidthAt_90 =  600.0; // in us
    pulseWidthAt90  = 2200.0; // in us

    restoreSettings();

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

    // Init GPIOs
    if(!gpioInit())
        exit(EXIT_FAILURE);

    switchLampOff();

    // Init User Interface with restored values
    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));

    intervalTimer.stop();// Probably non needed but...does'nt hurt
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
        kill(pid, SIGTERM);
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = nullptr;
    }
    switchLampOff();
    // Save settings
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("BaseDir", sBaseDir);
    settings.setValue("FileName", sOutFileName);
    settings.setValue("Interval", msecInterval);
    settings.setValue("TotalTime", secTotTime);
    settings.setValue("panAngle",  cameraPanAngle);
    settings.setValue("tiltAngle", cameraTiltAngle);
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
#endif
}


void
MainWindow::restoreSettings() {
    QSettings settings;
    // Restore settings
    sBaseDir        = settings.value("BaseDir",
                                     QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    sOutFileName    = settings.value("FileName",
                                     QString("test")).toString();
    msecInterval    = settings.value("Interval", 10000).toInt();
    secTotTime      = settings.value("TotalTime", 0).toInt();
    cameraPanAngle  = settings.value("panAngle",  0.0).toDouble();
    cameraTiltAngle = settings.value("tiltAngle", 0.0).toDouble();

    // Restore Geometry and State of the window
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
}


bool
MainWindow::gpioInit() {
#if defined(Q_PROCESSOR_ARM)
    int iResult;
    gpioHostHandle = pigpio_start(QString("localhost").toLocal8Bit().data(),
                                  QString("8888").toLocal8Bit().data());
    if(gpioHostHandle < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error !"),
                              QString("Non riesco ad inizializzare la GPIO."));
        return false;
    }
    iResult = set_mode(gpioHostHandle, gpioLEDpin, PI_OUTPUT);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to initialize GPIO%1 as Output")
                                   .arg(gpioLEDpin));
        return false;
    }

    iResult = set_pull_up_down(gpioHostHandle, gpioLEDpin, PI_PUD_UP);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Unable to set GPIO%1 Pull-Up")
                                   .arg(gpioLEDpin));
        return false;
    }

    iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Pan."));
        return false;
    }
    double pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraPanAngle+90.0);// In us
    iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a far partire il PWM per il Pan."));
        return false;
    }
    set_PWM_frequency(gpioHostHandle, panPin, 0);

    iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        return false;
    }
    pulseWidth = pulseWidthAt_90 +(pulseWidthAt90-pulseWidthAt_90)/180.0 * (cameraTiltAngle+90.0);// In us
    iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a far partire il PWM per il Tilt."));
        return false;
    }
    iResult = set_PWM_frequency(gpioHostHandle, tiltPin, 0);
    if(iResult == PI_BAD_USER_GPIO) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Bad User GPIO"));
        return false;
    }
    if(iResult == PI_NOT_PERMITTED) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("GPIO operation not permitted"));
        return false;
    }
#endif
    return true;
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
    QDir dir(sBaseDir);
    if(!dir.exists())
        return false;
    return true;
}


//////////////////////////////////////////////////////////////
/// Process event handlers <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
void
MainWindow::onImageRecorderStarted() {
    pid = pid_t(pImageRecorder->processId());
    if(pid != 0) {
        intervalTimer.start(msecInterval);
    }
}


void
MainWindow::onImageRecorderError(QProcess::ProcessError error) {
    Q_UNUSED(error)
    pUi->statusBar->showMessage(QString("raspistill Error %1").arg(error), 1000);
    switchLampOff();
    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
}


void
MainWindow::onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
    intervalTimer.stop();
    pImageRecorder->disconnect();
    pImageRecorder->deleteLater();
    pImageRecorder = nullptr;
//    pUi->statusBar->showMessage(QString("raspistill exited with status: %1, Exit code: %2")
//                                .arg(exitStatus)
//                                .arg(exitCode));
    switchLampOff();
    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
}


//////////////////////////////////////////////////////////////
/// UI event handlers <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
void
MainWindow::on_startButton_clicked() {
    if(!checkValues()) {
        pUi->statusBar->showMessage((QString("Error: Check Values !")));
        return;
    }
    imageNum = 0;

#if defined(Q_PROCESSOR_ARM)
    pImageRecorder = new QProcess(this);
    connect(pImageRecorder,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(onImageRecorderClosed(int, QProcess::ExitStatus)));
    connect(pImageRecorder,
            SIGNAL(errorOccurred(QProcess::ProcessError)),
            this,
            SLOT(onImageRecorderError(QProcess::ProcessError)));
    connect(pImageRecorder,
            SIGNAL(started()),
            this,
            SLOT(onImageRecorderStarted()));

    QString sCommand = QString("/usr/bin/raspistill");
    QStringList sArguments = QStringList();
    sArguments.append(QString("-s"));                        // Acquire upon receiving a SIGNAL
    sArguments.append(QString("-n"));                        // No preview
    sArguments.append(QString("-ex auto"));                  // Exposure mode; Auto
    sArguments.append(QString("-awb auto"));                 // White Balance; Auto
    sArguments.append(QString("-drc off"));                  // Dynamic Range Compression: off
    sArguments.append(QString("-q %1").arg(IMAGE_QUALITY));  // JPEG quality: 100=max
    sArguments.append(QString("-t %1").arg(secTotTime*1000));// Acquisition Time(0= No limit)
    sArguments.append(QString("-o %1/%2_%04d.jpg")           // File name(s)
                      .arg(sBaseDir)
                      .arg(sOutFileName));

////////////////////////////////////////////////////////////
/// Here we could use the following (Not working at present)
//    pImageRecorder->setProgram(sCommand);
//    pImageRecorder->setArguments(sArguments);
//    pImageRecorder->start();
/// Instead we have to use:
    for(int i=0; i<sArguments.size(); i++)
        sCommand += QString(" %1").arg(sArguments[i]);
    pImageRecorder->start(sCommand);
////////////////////////////////////////////////////////////
#endif

    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setDisabled(true);
    }
    pUi->startButton->setDisabled(true);
    pUi->stopButton->setEnabled(true);
}


void
MainWindow::on_stopButton_clicked() {
    intervalTimer.stop();
    if(pImageRecorder) {
        disconnect(pImageRecorder,// To avoid a "process crashed" error
                   SIGNAL(errorOccurred(QProcess::ProcessError)),
                   nullptr,
                   nullptr);
        int iErr = kill(pid, SIGTERM);
        if(iErr == -1) {
            pUi->statusBar->showMessage(QString("Error %1 in sending SIGTERM signal").arg(iErr));
        }
    }
    else {
        switchLampOff();
        QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
        for(int i=0; i<widgets.size(); i++) {
            widgets[i]->setEnabled(true);
        }
        pUi->startButton->setEnabled(true);
        pUi->stopButton->setDisabled(true);
    }
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
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->intervalEdit->setStyleSheet(sNormalStyle);
}


void
MainWindow::on_tTimeEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() < 0) {
        pUi->tTimeEdit->setStyleSheet(sErrorStyle);
    } else {
        secTotTime = arg1.toInt();
        pUi->tTimeEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainWindow::on_tTimeEdit_editingFinished() {
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));
    pUi->tTimeEdit->setStyleSheet(sNormalStyle);
}

void
MainWindow::on_pathEdit_textChanged(const QString &arg1) {
    QDir dir(arg1);
    if(!dir.exists()) {
        pUi->pathEdit->setStyleSheet(sErrorStyle);
    }
    else {
        pUi->pathEdit->setStyleSheet(sNormalStyle);
    }
}


void
MainWindow::on_pathEdit_editingFinished() {
    sBaseDir = pUi->pathEdit->text();
}


void
MainWindow::on_nameEdit_textChanged(const QString &arg1) {
    sOutFileName = arg1;
}


//////////////////////////////////////////////////////////////
/// Acquisition timer handler <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
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


