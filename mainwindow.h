#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include <sys/types.h>



namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void switchLampOn();
    void switchLampOff();
    bool checkValues();
    bool gpioInit();

public slots:
    void onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void on_startButton_clicked();
    void on_intervalEdit_textEdited(const QString &arg1);
    void on_intervalEdit_editingFinished();
    void on_tTimeEdit_textEdited(const QString &arg1);
    void on_tTimeEdit_editingFinished();
    void onTimeToGetNewImage();

    void on_stopButton_clicked();

private:
    Ui::MainWindow* pUi;
    QProcess*       pImageRecorder;

    pid_t pid;

    uint   gpioLEDpin;
    uint   panPin;
    uint   tiltPin;
    double cameraPanAngle;
    double cameraTiltAngle;
    uint   PWMfrequency;     // in Hz
    double pulseWidthAt_90;  // in us
    double pulseWidthAt90;   // in us
    int    gpioHostHandle;

    int    msecInterval;
    int    msecTotTime;
    int    imageNum;

    QString sNormalStyle;
    QString sErrorStyle;
    QString sDarkStyle;
    QString sPhotoStyle;

    QString sBaseDir;
    QString sOutFileName;

    QTimer intervalTimer;
};

#endif // MAINWINDOW_H
