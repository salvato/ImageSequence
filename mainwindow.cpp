#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#include "setupdialog.h"
#include <signal.h>
#include <QMoveEvent>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>
#include <QThread>
#include <QDebug>
#include <QDir>


#define MIN_INTERVAL 1500 // in ms (depends on the image format: jpeg is HW accelerated !)
#define IMAGE_QUALITY 100 // 100 is Best quality


// ================================================
// GPIO Numbers are Broadcom (BCM) numbers
// ================================================
// +5V on pins 2 or 4 in the 40 pin GPIO connector.
// GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
// in the 40 pin GPIO connector.
// ================================================
#define LED_PIN  23 // BCM23 is Pin 16 in the 40 pin GPIO connector.


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , pUi(new Ui::MainWindow)
    , pImageRecorder(Q_NULLPTR)
    , gpioLEDpin(LED_PIN)
    , gpioHostHandle(-1)
{
    pUi->setupUi(this);
    dialogPos = pos();
    videoPos  = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();

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

    sBlackStyle   = "QLabel { \
                        color: rgb(255, 255, 255); \
                        background: rgb(0, 0, 0); \
                        selection-background-color: rgb(128, 128, 255); \
                    }";
    if(!gpioInit())
        exit(EXIT_FAILURE);

    pSetupDlg = new setupDialog(gpioHostHandle);
    switchLampOff();

    // Init User Interface with restored values
    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->stopButton->setDisabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval));
    pUi->tTimeEdit->setText(QString("%1").arg(secTotTime));
    pUi->labelVideo->setStyleSheet(sBlackStyle);

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
        pImageRecorder->terminate();
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
    // Free GPIO
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
}


void
MainWindow::moveEvent(QMoveEvent *event) {
    dialogPos = event->pos();
    videoPos = pUi->labelVideo->pos();
    videoSize = pUi->labelVideo->size();
    if(pImageRecorder) {
        QString sCommand = QString("/usr/bin/raspistill");
        QStringList sArguments = QStringList();
        sArguments.append(QString("-s"));                        // Acquire upon receiving a SIGUSR1 signal
        sArguments.append(QString("-ex auto"));                  // Exposure mode; Auto
        sArguments.append(QString("-awb auto"));                 // White Balance; Auto
        sArguments.append(QString("-drc off"));                  // Dynamic Range Compression: off
        sArguments.append(QString("-q %1").arg(IMAGE_QUALITY));  // JPEG quality: 100=max
        sArguments.append(QString("-t %1").arg(secTotTime*1000));// Acquisition Time(0 = No limit)
        sArguments.append(QString("-dt"));                       // White Balance; Auto
        sArguments.append(QString("-o %1/%2_%d.jpg")             // File name(s)
                          .arg(sBaseDir)
                          .arg(sOutFileName));
        sArguments.append(QString("-p %1,%2,%3,%4")
                          .arg(dialogPos.x()+videoPos.x())
                          .arg(dialogPos.y()+videoPos.y())
                          .arg(videoSize.width())
                          .arg(videoSize.height()));
////////////////////////////////////////////////////////////
/// Here we could use the following (Not working at present)
//    pImageRecorder->setProgram(sCommand);
//    pImageRecorder->setArguments(sArguments);
//    pImageRecorder->start();
/// Instead we have to use:
        for(int i=0; i<sArguments.size(); i++)
            sCommand += QString(" %1").arg(sArguments[i]);
////////////////////////////////////////////////////////////
        if(pImageRecorder->state() == QProcess::NotRunning)
            pImageRecorder->start(sCommand);
        else {
            pImageRecorder->disconnect();
            pImageRecorder->terminate();
            pImageRecorder->close();
            pImageRecorder->waitForFinished(3000);
            delete pImageRecorder;
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
            pImageRecorder->start(sCommand);
        }
    }
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

    // Restore Geometry and State of the window
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
}


bool
MainWindow::gpioInit() {
    int iResult;
    gpioHostHandle = pigpio_start(QString("localhost").toLocal8Bit().data(),
                                  QString("8888").toLocal8Bit().data());
    if(gpioHostHandle < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error !"),
                              QString("Non riesco ad inizializzare la GPIO."));
        return false;
    }
    // Led On/Off Control
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
    return true;
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
    pUi->lampStatus->setStyleSheet(sPhotoStyle);
    repaint();
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
    pUi->statusBar->showMessage(QString("raspistill Error %1")
                                .arg(error), 1000);
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
    intervalTimer.stop();
    pImageRecorder->disconnect();
    pImageRecorder->deleteLater();
    pImageRecorder = nullptr;
    if(exitCode != 130) {// exitStatus==130 means process killed by Ctrl-C
        pUi->statusBar->showMessage(QString("raspistill exited with status: %1, Exit code: %2")
                                    .arg(exitStatus)
                                    .arg(exitCode), 2000);
    }
    switchLampOff();
    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
    pUi->setupButton->setEnabled(true);
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
    sArguments.append(QString("-s"));                        // Acquire upon receiving a SIGUSR1 signal
    sArguments.append(QString("-ex auto"));                  // Exposure mode; Auto
    sArguments.append(QString("-awb auto"));                 // White Balance; Auto
    sArguments.append(QString("-drc off"));                  // Dynamic Range Compression: off
    sArguments.append(QString("-q %1").arg(IMAGE_QUALITY));  // JPEG quality: 100=max
    sArguments.append(QString("-t %1").arg(secTotTime*1000));// Acquisition Time(0 = No limit)
    sArguments.append(QString("-dt"));                       // White Balance; Auto
    sArguments.append(QString("-o %1/%2_%d.jpg")             // File name(s)
                      .arg(sBaseDir)
                      .arg(sOutFileName));
    sArguments.append(QString("-p %1,%2,%3,%4")
                      .arg(dialogPos.x()+videoPos.x())
                      .arg(dialogPos.y()+videoPos.y())
                      .arg(videoSize.width())
                      .arg(videoSize.height()));

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

    QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setDisabled(true);
    }
    pUi->setupButton->setDisabled(true);
    pUi->startButton->setDisabled(true);
    pUi->stopButton->setEnabled(true);
}


void
MainWindow::on_stopButton_clicked() {
    intervalTimer.stop();
    if(pImageRecorder) {
        kill(pid, SIGINT);
    }
    else {
        switchLampOff();
        QList<QLineEdit *> widgets = findChildren<QLineEdit *>();
        for(int i=0; i<widgets.size(); i++) {
            widgets[i]->setEnabled(true);
        }
        pUi->startButton->setEnabled(true);
        pUi->setupButton->setEnabled(true);
        pUi->stopButton->setDisabled(true);
    }
}


void
MainWindow::on_setupButton_clicked() {
    pSetupDlg->exec();
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
    QThread::msleep(10);
    int iErr = kill(pid, SIGUSR1);
    if(iErr == -1) {
        pUi->statusBar->showMessage(QString("Error %1 in sending SIGUSR1 signal")
                                    .arg(iErr), 2000);
    }
    QThread::msleep(300);
    switchLampOff();
}

