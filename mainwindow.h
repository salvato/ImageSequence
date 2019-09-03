#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QProcess>


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

public slots:
    void onImageRecorderClosed(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::MainWindow* ui;
    QProcess*       pImageRecorder;

    qint64 pid;
    int    gpioLEDpin;
    int    gpioHostHandle;

    QString          sNormalStyle;
    QString          sErrorStyle;
    QString          sDarkStyle;
    QString          sPhotoStyle;
};

#endif // MAINWINDOW_H
