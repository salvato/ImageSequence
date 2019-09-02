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
    void onImageRecorderClosed(int, QProcess::ExitStatus);

private:
    Ui::MainWindow* ui;
    QProcess*       pImageRecorder;

    qint64 pid;
    int    gpioLEDpin;
    int    gpioHostHandle;
};

#endif // MAINWINDOW_H
