#include "mainwindow.h"
#include "ui_mainwindow.h"
#if defined(Q_PROCESSOR_ARM)
    #include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#endif
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>


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
    , msecInterval(1000)
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
#endif

    switchLampOff();

    pUi->pathEdit->setText(sBaseDir);
    pUi->nameEdit->setText(sOutFileName);
    pUi->startButton->setEnabled(true);
    pUi->intervalEdit->setText(QString("%1").arg(msecInterval, 8));
    pUi->tTimeEdit->setText(QString("%1").arg(msecTotTime, 8));

    // Restore Geometry and State of the window
    QSettings settings;
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
}


MainWindow::~MainWindow() {
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
#endif
    if(pImageRecorder) {
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = Q_NULLPTR;
    }
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    delete pUi;
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
}


void
MainWindow::on_startButton_clicked() {
    QString sCommand = QString("raspistill -s ");
    sCommand += QString("-t %1").arg(0);

#if defined(Q_PROCESSOR_ARM)

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
#endif

    pUi->statusBar->showMessage(sCommand);
}


void
MainWindow::on_intervalEdit_textEdited(const QString &arg1) {
    if(arg1.toInt() > 10) {
        msecInterval = arg1.toInt();
        pUi->intervalEdit->setStyleSheet(sNormalStyle);
    } else {
        pUi->intervalEdit->setStyleSheet(sErrorStyle);
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

