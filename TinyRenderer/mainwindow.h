#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QDebug>
#include <QTextBrowser>
#include <string>
#include <QImage>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QMouseEvent>
#include <QTimer>
//读取本地文件
#include <QFileDialog>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include "our_gl.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    Ui::MainWindow *ui;
    TGAImage diffuseTexture();
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
signals:
    void updateSignals();


private slots:
    void on_actionOpen_triggered();
    void on_lightX_valueChanged(int value);
    void on_lightY_valueChanged(int value);
    void on_lightZ_valueChanged(int value);
    void on_comboBox_currentIndexChanged(int index);
    void update();
private:
    void InitMainwindow();      //窗口初始化
    void MainRender();          //主渲染函数
    void TestRender();

};
#endif // MAINWINDOW_H
