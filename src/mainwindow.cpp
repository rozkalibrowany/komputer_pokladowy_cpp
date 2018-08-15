#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "rpmwidget.h"
#include "testform.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("ADek UI v0.1");
    this->centerOnScreen();

    // create connection
    connection = new Connections();
    // create Rpm Widget
    RpmWidget *rpm = new RpmWidget(ui->rpm_widget);

}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::centerOnScreen(void)
{

   this->move((resolution.width()/2) - (this->frameSize().width()/2),
           ((resolution.height()/2) - (this->frameSize().height()/2)));
}



