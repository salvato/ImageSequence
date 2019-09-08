#include "setupdialog.h"
#include "ui_setupdialog.h"
#include <signal.h>
#if defined(Q_PROCESSOR_ARM)
    #include "pigpiod_if2.h"// The library for using GPIO pins on Raspberry
#endif
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QDebug>


// GPIO Numbers are Broadcom (BCM) numbers
#define PAN_PIN  14 // BCM14 is Pin  8 in the 40 pin GPIO connector.
#define TILT_PIN 26 // BCM26 IS Pin 37 in the 40 pin GPIO connector.

setupDialog::setupDialog(int hostHandle, QWidget *parent)
    : QDialog(parent)
    , pUi(new Ui::setupDialog)
    , pImageRecorder(Q_NULLPTR)
    // ================================================
    // GPIO Numbers are Broadcom (BCM) numbers
    // ================================================
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
    // ================================================
    , panPin(PAN_PIN)
    , tiltPin(TILT_PIN)
    , gpioHostHandle(hostHandle)
{
    pUi->setupUi(this);

    // Values to be checked with the used servos
    PWMfrequency    =   50; // in Hz
    pulseWidthAt_90 =  600; // in us
    pulseWidthAt90  = 2200; // in us

    pUi->dialPan->setRange(pulseWidthAt_90, pulseWidthAt90);
    pUi->dialTilt->setRange(pulseWidthAt_90, pulseWidthAt90);

    cameraPanValue  = (pulseWidthAt90-pulseWidthAt_90)/2+pulseWidthAt_90;
    cameraTiltValue = cameraPanValue;

    restoreSettings();

    // Init GPIOs
    if(!panTiltInit())
        exit(EXIT_FAILURE);
}


setupDialog::~setupDialog() {
    if(pImageRecorder) {
        pImageRecorder->terminate();
        pImageRecorder->close();
        pImageRecorder->waitForFinished(3000);
        pImageRecorder->deleteLater();
        pImageRecorder = nullptr;
    }
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
#endif
    // Save settings
    QSettings settings;
    delete pUi;
}


void
setupDialog::restoreSettings() {
    QSettings settings;
    // Restore settings
    cameraPanValue  = settings.value("panValue",  cameraPanValue).toDouble();
    cameraTiltValue = settings.value("tiltValue", cameraTiltValue).toDouble();
    pUi->dialPan->setValue(int(cameraPanValue));
    pUi->dialTilt->setValue(int(cameraTiltValue));
    // Restore Geometry and State of the window
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
}


bool
setupDialog::panTiltInit() {
#if defined(Q_PROCESSOR_ARM)
    int iResult;
    // Camera Pan-Tilt Control
    iResult = set_PWM_frequency(gpioHostHandle, panPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Pan."));
        return false;
    }
    if(!setPan(cameraPanValue))
        return false;
    if(!setTilt(cameraTiltValue))
        return false;
#endif
    return true;
}


bool
setupDialog::setPan(double cameraPanValue) {
    double pulseWidth = cameraPanValue;// In us
    int iResult = set_servo_pulsewidth(gpioHostHandle, panPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QString sError;
        if(iResult == PI_BAD_USER_GPIO)
            sError = QString("Bad User GPIO");
        else if(iResult == PI_BAD_PULSEWIDTH)
            sError = QString("Bad Pulse Width %1").arg(pulseWidth);
        else if(iResult == PI_NOT_PERMITTED)
            sError = QString("Not Permitted");
        else
            sError = QString("Unknown Error");
        QMessageBox::critical(this,
                              sError,
                              QString("Non riesco a far partire il PWM per il Pan."));
        return false;
    }
    set_PWM_frequency(gpioHostHandle, panPin, 0);
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
    return true;
}


bool
setupDialog::setTilt(double cameraTiltValue) {
    double pulseWidth = cameraTiltValue;// In us
    int iResult = set_PWM_frequency(gpioHostHandle, tiltPin, PWMfrequency);
    if(iResult < 0) {
        QMessageBox::critical(this,
                              QString("pigpiod Error"),
                              QString("Non riesco a definire la frequenza del PWM per il Tilt."));
        return false;
    }
    iResult = set_servo_pulsewidth(gpioHostHandle, tiltPin, u_int32_t(pulseWidth));
    if(iResult < 0) {
        QString sError;
        if(iResult == PI_BAD_USER_GPIO)
            sError = QString("Bad User GPIO");
        else if(iResult == PI_BAD_PULSEWIDTH)
            sError = QString("Bad Pulse Width %1").arg(pulseWidth);
        else if(iResult == PI_NOT_PERMITTED)
            sError = QString("Not Permitted");
        else
            sError = QString("Unknown Error");
        QMessageBox::critical(this,
                              sError,
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
    return true;
}


//////////////////////////////////////////////////////////////
/// Process event handlers <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//////////////////////////////////////////////////////////////
void
setupDialog::onImageRecorderStarted() {
    pid = pid_t(pImageRecorder->processId());
}


void
setupDialog::onImageRecorderError(QProcess::ProcessError error) {
    Q_UNUSED(error)
    QMessageBox::critical(this,
                          QString("raspistill"),
                          QString("Error %1")
                          .arg(error));
    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
}


void
setupDialog::onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    pImageRecorder->disconnect();
    pImageRecorder->deleteLater();
    pImageRecorder = nullptr;
    if(exitCode != 130) {// exitStatus==130 means process killed by Ctrl-C
        QMessageBox::critical(this,
                              QString("raspistill"),
                              QString("exited with status: %1, Exit code: %2")
                              .arg(exitStatus)
                              .arg(exitCode));
    }
    QList<QWidget *> widgets = findChildren<QWidget *>();
    for(int i=0; i<widgets.size(); i++) {
        widgets[i]->setEnabled(true);
    }
}


void
setupDialog::on_dialPan_valueChanged(int value) {
    cameraPanValue  = value;
    setPan(cameraPanValue);
    qDebug() << "Pan = " << cameraPanValue;
    update();
    // Save settings
    QSettings settings;
    settings.setValue("panValue",  cameraPanValue);
}


void
setupDialog::on_dialTilt_valueChanged(int value) {
    cameraTiltValue = value;
    setTilt(cameraTiltValue);
    qDebug() << "Tilt = " << cameraTiltValue;
    update();
    QSettings settings;
    settings.setValue("tiltValue", cameraTiltValue);
}
