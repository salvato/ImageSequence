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

private slots:
    void on_startButton_clicked();
    void on_intervalEdit_textEdited(const QString &arg1);
    void on_intervalEdit_editingFinished();

    void on_tTimeEdit_textEdited(const QString &arg1);
    void on_tTimeEdit_editingFinished();

private:
    Ui::MainWindow* pUi;
    QProcess*       pImageRecorder;

    qint64 pid;

    int    gpioLEDpin;
    int    gpioHostHandle;

    int    msecInterval;
    int    msecTotTime;

    QString sNormalStyle;
    QString sErrorStyle;
    QString sDarkStyle;
    QString sPhotoStyle;

    QString sBaseDir;
    QString sOutFileName;

};

#endif // MAINWINDOW_H
