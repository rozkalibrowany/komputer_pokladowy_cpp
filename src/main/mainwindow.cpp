#include <QObject>
#include <string>
#include <QSequentialIterable>
#include <QDateTime>
#include <QStyle>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../common/parameters.h"
#include "../common/logger.h"
#include "../settings/parser.h"


#define CLASS_INFO          "main window"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    LOG (LOG_MAINWINDOW, "%s - in constructor", CLASS_INFO);

    //LOG (LOG_MAINWINDOW, "%s - app version - %s", CLASS_INFO, GIT_VERSION);

    ui->setupUi(this);
    if (GIT_VERSION)
        this->setWindowTitle(QString("VOCC ver. %1").arg(GIT_VERSION));
    this->centerOnScreen();
    lastButtonObject = NULL;
    lapTimerStarted = false;

    /* create rpm widget */
    rpm = new RpmWidget(ui->rpm_widget);

    /* create converter alerts widget */
    alerts = new Alerts(ui->controllerWidget, ui->batteryWidget);

    /* create connection */
    connection = new Connections(rpm, alerts);

    /* create settings widget */
    settings = new Settings(ui->settingsWidget, connection);

    /* create statistics widget */
    stats = new Statistics(ui->statsWidget, connection);

    /* set connection status */
    connection->setConnectionStatus(false);

    /* set connection button state */
    setStateConnectionButton(connection->getConnectionStatus());

    /* set buttons map */
    map["vfMain"] = 0;
    map["vfAlerts"] = 1;
    map["vfStats"] = 2;
    map["vfSettings"] = 3;

    /* initialize functions buttons */
    initializeFunctionButtons();

    /* initialize signals and slots */
    initializeSignalsAndSlots();

    /* set system datetime */
    setSystemDateSlot();
    initializeTimerForDateTime();

    /* initialize lap timer */
    initializeLapTimer();

    /* initializing flashing button timer */
    initializeFlashTimer();

    /* set start page */
    menuButtonChanged(*ui->vfMain);

    /* set default alert status */
    updateAlertsStatus(-1);

    /* enable data refreshing */
    enableDataRefreshing();

    /* read config file */
    conf_exit();
    int key = conf_init ("settings.conf");

    if (key) {
        LOG (LOG_MAINWINDOW, "%s - CONFIG file found", CLASS_INFO);
        settings->consolePrintExternalMessage("config file found", 0);
        QTimer *nt = new QTimer();
        nt->setInterval(1000);
        nt->setSingleShot(true);
        connect (nt, &QTimer::timeout, [=] { settings->readConfigFile(); });
        nt->start();
    } else {
        LOG (LOG_MAINWINDOW, "%s - config file not found", CLASS_INFO);
        settings->consolePrintExternalMessage("config file not found", 1);
    }

}


MainWindow::~MainWindow()
{
    LOG (LOG_MAINWINDOW, "%s - destructor", CLASS_INFO);

    delete ui;
    delete rpm;
    delete settings;
    delete alerts;
    delete connection;
}


void MainWindow::centerOnScreen(void)
{
   LOG (LOG_MAINWINDOW, "%s - centering on screen", CLASS_INFO);

   this->move((resolution.width()/2) - (this->frameSize().width()/2),
           ((resolution.height()/2) - (this->frameSize().height()/2)));
}


void MainWindow::initializeFunctionButtons(void)
{
    LOG (LOG_MAINWINDOW, "%s - initializing function buttons", CLASS_INFO);

    connect (ui->driveButton, &QPushButton::clicked,
                [this] { menuButtonChanged(*ui->vfMain); });
    connect (ui->driveButton, &QPushButton::clicked,
                this, &MainWindow::enableDataRefreshing);
    connect (ui->alertsButton, &QPushButton::clicked,
                [this] { menuButtonChanged(*ui->vfAlerts); });
    connect (ui->statsButton, &QPushButton::clicked,
                stats, &Statistics::enableChartData);
    connect (this, &MainWindow::statsMenuDisabled,
                stats, &Statistics::disableChartData);
    connect (ui->statsButton, &QPushButton::clicked,
                [this] { menuButtonChanged(*ui->vfStats); });
    connect (ui->settingsButton, &QPushButton::clicked,
                [this] { menuButtonChanged(*ui->vfSettings); });
}


void MainWindow::initializeSignalsAndSlots(void)
{
    LOG (LOG_MAINWINDOW, "%s - initializing signals and slots", CLASS_INFO);

    connect (ui->canButton, &QPushButton::clicked,
                connection, &Connections::initializeConnection);

    connect (settings, &Settings::updateBackgroundContrast,
                [=] (int value) { updateBackgroundContrast(value); });

    connect (settings, &Settings::updateFontSize,
                [=] (QString size) { updateFontSize(size); });

    connect (settings, &Settings::quitApplication,
                [this] () { this->close(); });

    connect (settings, &Settings::shutdownSystem,
                this, &MainWindow::shutdownSystem);

    connect (settings, &Settings::rebootSystem,
                this, &MainWindow::rebootSystem);
}


void MainWindow::initializeLapTimer(void)
{
    LOG (LOG_MAINWINDOW, "%s - initializing lap timer", CLASS_INFO);

    ui->timerButton->setText("Start Timer");

    lapTimer = new QTimer(this);
    sleepTimer = new QTimer(this);

    sleepTimer->setInterval(5000);
    sleepTimer->setSingleShot(true);


    m = ms = s = 0;

    connect (ui->timerButton, &QPushButton::clicked, this, &MainWindow::startLapTimerSlot);
    connect (lapTimer, &QTimer::timeout, this, &MainWindow::updateLapTimerSlot);
    connect (sleepTimer, &QTimer::timeout, this, &MainWindow::resetLapTimerSlot);
}


void MainWindow::initializeFlashTimer(void)
{
    flashTimer = new QTimer();
    flashTimer->setInterval(800);
    setFlash = false;
}


void MainWindow::initializeTimerForDateTime(void)
{
    LOG (LOG_MAINWINDOW, "%s - current date and time %s, %s", CLASS_INFO, \
         date.toStdString().c_str(), time.toStdString().c_str());

    QTimer *timer = new QTimer();
    connect (timer, &QTimer::timeout, this, &MainWindow::setSystemDateSlot);
    timer->start(1000);
}


void MainWindow::menuButtonChanged(QFrame &frame)
{
    LOG (LOG_MAINWINDOW, "%s - initializing function button: %s", CLASS_INFO, \
         frame.objectName().toStdString().c_str());

    if (lastButtonObject == NULL) {
        lastButtonObject = &frame;
        setNewPage(map[frame.objectName()]);
        buttonStyleUpdate(frame, true);
        LOG (LOG_MAINWINDOW, "%s - changed page to %d", CLASS_INFO, map[frame.objectName()]);
    } else if (lastButtonObject != &frame) {
        switch (map[lastButtonObject->objectName()])
        {
            case 0:
                disableDataRefreshing();
                break;
            case 2:
                emit statsMenuDisabled();
                break;
        }

        if (!QString::compare(flashingObject, frame.objectName())) {
            setButtonFlashing(frame, false);
        }

        setNewPage(map[frame.objectName()]);
        buttonStyleUpdate(frame, true);
        buttonStyleUpdate(*lastButtonObject, false);
        lastButtonObject = &frame;
    }
}


void MainWindow::enableDataRefreshing(void)
{
    LOG (LOG_MAINWINDOW, "%s - enabled main window data refreshing", CLASS_INFO);

    connect (connection, &Connections::updateRpmSpeed, rpm,
                [=] (quint16 speed) { rpm->updateWidget(speed); });

    connect (connection, &Connections::setConnectionStateButton, this,
                [=] (bool isConnected) { setStateConnectionButton(isConnected); });

    connect (connection, &Connections::updateBatteryCurrent, this,
                [=] (quint16 current) { updateBatteryCurrent(current); });

    connect (connection, &Connections::updateBatteryVoltage, this,
                [=] (quint16 voltage) { updateBatteryVoltage(voltage); });

    connect (connection, &Connections::updatePower, this,
                [=] (float power) { updatePower(power); });

    connect (connection, &Connections::updateThrottle, this,
                [=] (quint16 throttle) { updateThrottle(throttle); });

    connect (connection, &Connections::updateControllerTemp, this,
                [=] (quint16 temp) { updateControllerTemp(temp); });

    connect (connection, &Connections::updateMotorTemp, this,
                [=] (quint16 temp) { updateMotorTemp(temp); });

    connect (alerts, &Alerts::setAlertsButtonState,
                [=] (int errors) { updateAlertsStatus(errors); });

    connect (connection, &Connections::setAlertsButtonState, this,
                [=] (int state) { updateAlertsStatus(state); });
}


void MainWindow::disableDataRefreshing(void)
{
    LOG (LOG_MAINWINDOW, "%s - disabled main window data refreshing", CLASS_INFO);

    disconnect(connection, 0, this, 0);
    disconnect(connection, &Connections::updateRpmSpeed, rpm, 0);
    disconnect(alerts, 0, this, 0);

}


void MainWindow::buttonStyleUpdate(QFrame &frame, bool isChanged)
{

    LOG (LOG_MAINWINDOW, "%s - %s - %s", CLASS_INFO, Q_FUNC_INFO, \
         frame.objectName().toStdString().c_str());

    QPushButton *iconObject;
    QLabel *textObject;

    iconObject = frame.findChild<QPushButton *>();
    textObject = frame.findChild<QLabel *>();

    styleUpdate(&frame, "chosen", isChanged);
    styleUpdate(iconObject, "chosen", isChanged);
    styleUpdate(textObject, "chosen", isChanged);

}


void MainWindow::setStateConnectionButton(bool isConnected)
{
    LOG (LOG_MAINWINDOW, "%s - connection established - %d", CLASS_INFO, isConnected);

    if (isConnected) {
        styleUpdate(ui->canButton, "connected", true);
        ui->canButton->setText("Connected");
    } else {
        styleUpdate(ui->canButton, "connected", false);
        ui->canButton->setText(">Connect<");
    }
}


template <typename T> void MainWindow::styleUpdate(T *widget, const char* property, bool isChanged)
{
    widget->setProperty(property, isChanged);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}


template <typename T> void MainWindow::lcdStyleUpdate(T &widget, quint16 value, quint16 limit, quint16 max, bool isChanged)
{
    if (value > max)
        styleUpdate(widget, "warning2", isChanged);
    else if (value > limit) {
        styleUpdate(widget, "warning", isChanged);
        styleUpdate(widget, "warning2", !isChanged);
    } else {
        styleUpdate(widget, "warning", !isChanged);
        styleUpdate(widget, "warning2", !isChanged);
    }
}


void MainWindow::updateBatteryCurrent(quint16 current)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - battery current - %d [A]", CLASS_INFO, current);

    ui->batteryCurrentLcd->display(current);
    lcdStyleUpdate(ui->batteryCurrent, current, 80, 140, true);
    lcdStyleUpdate(ui->batteryCurrentLcd, current, 80, 140, true);
}


void MainWindow::updateBatteryVoltage(quint16 voltage)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - battery voltage - %d [V]", CLASS_INFO, voltage);

    ui->batteryVoltageLcd->display(voltage);
    lcdStyleUpdate(ui->batteryVoltage, voltage, 84, 74, false); /* TODO not working properly */
    lcdStyleUpdate(ui->batteryVoltageLcd, voltage, 84, 74, false);
}


void MainWindow::updatePower(float power)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - battery power - %d [W]", CLASS_INFO, power);

    ui->avrPowerLcd->display(power);
    lcdStyleUpdate(ui->avrPower, power, 8, 20, true);
    lcdStyleUpdate(ui->avrPowerLcd, power, 8, 20, true);
}


void MainWindow::updateThrottle(quint16 throttle)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - throttle - %d [%]", CLASS_INFO, throttle);

    ui->throttleLcd->display(throttle);
}


void MainWindow::updateControllerTemp(quint16 temp)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - controller temperature - %d [C]", CLASS_INFO, temp);

    ui->contrTempLcd->display(temp);
    lcdStyleUpdate(ui->contrTemp, temp, 45, 65, true);
    lcdStyleUpdate(ui->contrTempLcd, temp, 45, 65, true);
}


void MainWindow::updateMotorTemp(quint16 temp)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - motor temperature - %d [C]", CLASS_INFO, temp);

    ui->motorTempLcd->display(temp);
    lcdStyleUpdate(ui->motorTemp, temp, 50, 65, true);
    lcdStyleUpdate(ui->motorTempLcd, temp, 50, 65, true);
}


void MainWindow::updateBackgroundContrast(int value)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - background contrast changed to (%d,%d,%d)", CLASS_INFO,
            value, value, value);

    QString style = "background-color: rgb(%1, %2, %3);";
    ui->centralwidget->setStyleSheet(style.arg(value).arg(value).arg(value));
    ui->gridFrame->setStyleSheet(style.arg(value).arg(value).arg(value));
    ui->stackedWidget->setStyleSheet(style.arg(value).arg(value).arg(value));
    ui->menuButtonsGroup->setStyleSheet(style.arg(value).arg(value).arg(value));
}


void MainWindow::updateFontSize(QString size)
{
    LOG (LOG_MAINWINDOW_DATA, "%s - font size changed to - %s", CLASS_INFO,
            size.toStdString().c_str());

    QString style = "font: italic bold %1pt";
    ui->centralwidget->setStyleSheet(style.arg(size));
    ui->gridFrame->setStyleSheet(style.arg(size));
    ui->stackedWidget->setStyleSheet(style.arg(size));
    ui->menuButtonsGroup->setStyleSheet(style.arg(size));
}

/* 0x0 - no alerts
 * 0x1 - 0x18 - number of active alerts
 * 0x1A - no data
 * 0x1B - CAN init error
 * 0x1C - wrong CAN frame
 */
void MainWindow::updateAlertsStatus(int err)
{
    LOG (LOG_MAINWINDOW, "%s - ENABLED ALERTS STATUS", CLASS_INFO);
    QString text;

    if (err == 0x0) {
        styleUpdate(ui->alertStatus, "isAlert", false);
        styleUpdate(ui->alertStatus, "noData", false);
        styleUpdate(ui->vfAlerts, "alert", false);
        setButtonFlashing(*ui->vfAlerts, false);

        text = "No alerts";
    }

    if (err > 0x0 && err < 0x1A) {
        LOG (LOG_MAINWINDOW_DATA, "%s - found %d alerts!", CLASS_INFO, err);
        styleUpdate(ui->alertStatus, "isAlert", true);
        styleUpdate(ui->alertStatus, "noData", false);
        styleUpdate(ui->vfAlerts, "alert", true);
        setButtonFlashing(*ui->vfAlerts, true);

        text = QString::number(err) + " " + "alerts!";
    } else {
        switch (err) {
            case 0x1A:
                LOG (LOG_MAINWINDOW_DATA, "%s - no data!", CLASS_INFO);
                text = "No data";
                styleUpdate(ui->alertStatus, "noData", true);
                setButtonFlashing(*ui->vfSettings, true);
                break;
            case 0x1B:
                LOG (LOG_MAINWINDOW_DATA, "%s - CAN init error", CLASS_INFO);
                text = "CAN init err";
                styleUpdate(ui->alertStatus, "error", true);
                styleUpdate(ui->vfSettings, "alert", true);
                setButtonFlashing(*ui->vfSettings, true);
                break;
            case 0x1C:
                LOG (LOG_MAINWINDOW_DATA, "%s - wrong CAN frame", CLASS_INFO);
                text = "Wrong frame";
                styleUpdate(ui->alertStatus, "noData", true);
                styleUpdate(ui->vfSettings, "alert", true);
                setButtonFlashing(*ui->vfSettings, true);
        }
    }

    ui->alertStatus->setText(text);
}

/* This method works only for 1 button simultaneously */
void MainWindow::setButtonFlashing(QFrame &frame, bool start)
{
    if (flashingObject != NULL && start)
        return;

    if (start) {
        flashTimer->start();
        flashingObject = frame.objectName();
        connect (flashTimer, &QTimer::timeout, [this, &frame]
        {
             setFlash = !setFlash;
             this->styleUpdate(&frame, "alert", setFlash);
        });
    }
    else {
        flashTimer->stop();
        flashTimer->disconnect();
        flashingObject = QString::null;
    }
}


void MainWindow::setNewPage(int index)
{
    LOG (LOG_MAINWINDOW, "%s - changing page to %d", CLASS_INFO, index);

    ui->stackedWidget->setCurrentIndex(index);
}


void MainWindow::setSystemDateSlot(void)
{
    date = QDate::currentDate().toString("dd-MM-yyyy");
    if (date != NULL)
        ui->dateLabel->setText(date);

    time = QTime::currentTime().toString("hh:mm");
    if (time != NULL)
        ui->timeLabel->setText(time);

}


void MainWindow::setLapTimerTime(void)
{
    lapTime = QString("%1:%2:%3").arg(QString::number(m), QString::number(s), QString::number(ms));
    ui->lcdNumber->setDigitCount(lapTime.length());
    ui->lcdNumber->display(lapTime);
}


void MainWindow::startLapTimerSlot(void)
{
    LOG (LOG_MAINWINDOW, "%s - lap timer started", CLASS_INFO);

    if (!lapTimerStarted) {
        lapTimerStarted = true;
        styleUpdate(ui->timerButton, "clicked", true);
        ui->timerButton->setText("Stop Timer");
        lapTimer->start(10);
    } else {
        styleUpdate(ui->timerButton, "clicked", false);
        ui->timerButton->setText("Start Timer");
        sleepTimer->start();
        lapTimer->stop();
        lapTimerStarted = false;
    }
}


void MainWindow::updateLapTimerSlot(void)
{
    if (ms < 100)
        ms += 1;
    else {
        if (s < 59) {
            ms = 0;
            s += 1;
        } else if (s == 59 && m < 59) {
            m += 1;
            ms = 0;
            s = 0;
        } else
            lapTimer->stop();
    }

    setLapTimerTime();
}


void MainWindow::resetLapTimerSlot(void)
{
    LOG (LOG_MAINWINDOW, "%s - resetting lap timer", CLASS_INFO);

    s = m = ms = 0;

    emit addLapTime(lapTime);
    setLapTimerTime();
}


void MainWindow::shutdownSystem(void)
{
    QProcess *proc = new QProcess();

    proc->start(SHUTDOWN_CMD);
    proc->waitForFinished();
}

void MainWindow::rebootSystem(void)
{
    QProcess *proc = new QProcess();

    proc->start(REBOOT_CMD);
    proc->waitForFinished();
}
