#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QProcess>
#include <QTimer>


namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void switchLampOn();
    void switchLampOff();
    bool checkValues();

public slots:
    void onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void on_startButton_clicked();
    void on_intervalEdit_textEdited(const QString &arg1);
    void on_intervalEdit_editingFinished();
    void on_tTimeEdit_textEdited(const QString &arg1);
    void on_tTimeEdit_editingFinished();
    void onTimeToGetNewImage();

private:
    Ui::MainWindow* pUi;
    QProcess*       pImageRecorder;

    int pid;

    int    gpioLEDpin;
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
