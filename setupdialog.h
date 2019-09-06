#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QProcess>
#include <sys/types.h>

namespace Ui {
class setupDialog;
}

class setupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit setupDialog(int hostHandle, QWidget *parent = nullptr);
    ~setupDialog();

protected:
    void restoreSettings();
    bool panTiltInit();

public slots:
    void onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onImageRecorderStarted();
    void onImageRecorderError(QProcess::ProcessError error);

private:
    Ui::setupDialog* pUi;
    QProcess*        pImageRecorder;
    pid_t pid;

    uint   panPin;
    uint   tiltPin;
    double cameraPanAngle;
    double cameraTiltAngle;
    uint   PWMfrequency;     // in Hz
    double pulseWidthAt_90;  // in us
    double pulseWidthAt90;   // in us
    int    gpioHostHandle;
};

#endif // SETUPDIALOG_H
