#include "MainWindow.h"
#include "MavlinkStatusClient.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QMap>

namespace {
constexpr int CAN_HEALTH_REQUEST = 2;
constexpr int STATE_REQUEST = 3;
constexpr int SERVICE_INFO_REQUEST = 4;
constexpr int FAULT_LOG_REQUEST = 5;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("RPi Status GCS");
    m_client = new MavlinkStatusClient(this);

    m_ledCommands = {
        {"timer_activation", {0x02, "RPI activation command", false}},
        {"check1", {0x03, "CHECK1 command", false}},
        {"check2", {0x04, "CHECK2 command", false}},
        {"check3", {0x05, "CHECK3 command", false}},
        {"health_check", {0x06, "System health check", false}},
        {"led_active", {0x07, "Led_active command", false}},
        {"led_insert", {0x0A, "LED insert command", false}},
        {"ils_out", {0xF5, "Move Led holder out", false}},
        {"ils_in", {0x5F, "Move Led holder in", false}},
        {"ils_feedback", {0x09, "Request current led holder position", false}},
        {"watchdog_preset", {0x0B, "Configure watchdog preset time in minutes", true}},
        {"timer_abort", {0x08, "Abort timmer command", false}}
    };

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *form = new QFormLayout;

    m_host = new QLineEdit("127.0.0.1", central);
    m_port = new QSpinBox(central); m_port->setRange(1, 65535); m_port->setValue(5760);
    m_transport = new QComboBox(central); m_transport->addItems({"TCP", "Serial"});
    m_serialDevice = new QLineEdit("COM3", central);
    m_serialBaud = new QSpinBox(central); m_serialBaud->setRange(1200, 3000000); m_serialBaud->setValue(57600);
    m_vehicleSystemId = new QSpinBox(central); m_vehicleSystemId->setRange(1, 255); m_vehicleSystemId->setValue(1);
    m_connectionState = new QLabel("Disconnected", central);
    m_canState = new QLabel("Unknown", central);
    m_canInterfaceState = new QLabel("Unknown", central);
    m_watchdogState = new QLabel("Unknown", central);
    m_ilsState = new QLabel("Unknown", central);
    m_faultCount = new QLabel("0", central);
    m_log = new QPlainTextEdit(central); m_log->setReadOnly(true);

    form->addRow("Connection type", m_transport);
    form->addRow("FC TCP host", m_host);
    form->addRow("FC TCP port", m_port);
    form->addRow("Serial port", m_serialDevice);
    form->addRow("Serial baud", m_serialBaud);
    form->addRow("Vehicle system ID", m_vehicleSystemId);
    form->addRow("Connection", m_connectionState);
    form->addRow("CAN available", m_canState);
    form->addRow("CAN interface up", m_canInterfaceState);
    form->addRow("Watchdog", m_watchdogState);
    form->addRow("ILS", m_ilsState);
    form->addRow("Fault count", m_faultCount);
    layout->addLayout(form);

    auto *buttons = new QVBoxLayout;
    auto *primary = new QHBoxLayout;
    auto *diagnostic = new QHBoxLayout;
    auto *connectButton = new QPushButton("Connect", central);
    auto *statusButton = new QPushButton("Refresh Diagnostics", central);
    auto *canHealthButton = new QPushButton("CAN Health", central);
    auto *statesButton = new QPushButton("Watchdog / ILS", central);
    auto *serviceButton = new QPushButton("Service Info", central);
    auto *faultButton = new QPushButton("Fault Log", central);
    primary->addWidget(connectButton); primary->addWidget(statusButton); primary->addWidget(canHealthButton);
    diagnostic->addWidget(statesButton); diagnostic->addWidget(serviceButton); diagnostic->addWidget(faultButton);
    buttons->addLayout(primary); buttons->addLayout(diagnostic);
    layout->addLayout(buttons);

    auto *ledGroup = new QGroupBox("LED / actuator commands", central);
    auto *ledLayout = new QGridLayout(ledGroup);
    m_watchdogMinutes = new QSpinBox(central);
    m_watchdogMinutes->setRange(0, 255); m_watchdogMinutes->setValue(5);
    int index = 0;
    for (auto it = m_ledCommands.cbegin(); it != m_ledCommands.cend(); ++it, ++index) {
        const QString name = it.key();
        QString label = name;
        label.replace('_', ' ');
        label = label.toLower();
        if (!label.isEmpty()) label[0] = label[0].toUpper();
        auto *button = new QPushButton(label, ledGroup);
        button->setToolTip(it.value().description);
        connect(button, &QPushButton::clicked, this, [this, name]() { sendLedCommand(name); });
        m_ledButtons.append(button);
        ledLayout->addWidget(button, index / 3, index % 3);
    }
    const int minutesRow = (m_ledCommands.size() / 3) + 1;
    ledLayout->addWidget(new QLabel("Watchdog preset (minutes):", ledGroup), minutesRow, 0);
    ledLayout->addWidget(m_watchdogMinutes, minutesRow, 1);
    layout->addWidget(ledGroup);
    layout->addWidget(m_log);
    setCentralWidget(central);

    for (auto *button : m_ledButtons) button->setEnabled(false);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectVehicle);
    connect(statusButton, &QPushButton::clicked, this, &MainWindow::requestStatus);
    connect(canHealthButton, &QPushButton::clicked, this, [this]() { requestDiagnostic(CAN_HEALTH_REQUEST); });
    connect(statesButton, &QPushButton::clicked, this, [this]() { requestDiagnostic(STATE_REQUEST); });
    connect(serviceButton, &QPushButton::clicked, this, [this]() { requestDiagnostic(SERVICE_INFO_REQUEST); });
    connect(faultButton, &QPushButton::clicked, this, [this]() { requestDiagnostic(FAULT_LOG_REQUEST); });
    connect(m_client, &MavlinkStatusClient::connectionStateChanged, m_connectionState, &QLabel::setText);
    connect(m_client, &MavlinkStatusClient::connectionStateChanged, this, &MainWindow::updateLedButtons);
    connect(m_client, &MavlinkStatusClient::statusMessage, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_client, &MavlinkStatusClient::canAvailabilityChanged, this, &MainWindow::setCanState);
    connect(m_client, &MavlinkStatusClient::diagnosticValue, this, &MainWindow::setDiagnosticValue);
}

void MainWindow::connectVehicle()
{
    m_client->connectToVehicle(m_transport->currentText(), m_host->text(), m_port->value(),
                               m_serialDevice->text(), m_serialBaud->value());
}
void MainWindow::requestStatus() { m_client->requestRpiStatus(m_vehicleSystemId->value()); }
void MainWindow::requestDiagnostic(int request) { m_client->requestDiagnostic(request, m_vehicleSystemId->value()); }

void MainWindow::sendLedCommand(const QString &name)
{
    const auto spec = m_ledCommands.value(name);
    int watchdogMinutes = 0;
    QString detail = spec.description;
    if (spec.hasWatchdogArgument) {
        watchdogMinutes = m_watchdogMinutes->value();
        detail = QString("%1 (%2 min)").arg(spec.description).arg(watchdogMinutes);
    }
    const auto answer = QMessageBox::question(
        this, "Confirm actuator command",
        QString("Send '%1' to vehicle system %2?\n\n%3").arg(name).arg(m_vehicleSystemId->value()).arg(detail),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    m_client->requestLedCommand(spec.commandId, m_vehicleSystemId->value(), watchdogMinutes);
}

void MainWindow::updateLedButtons(const QString &state)
{
    const bool connected = (state == "Connected");
    for (auto *button : m_ledButtons) button->setEnabled(connected);
}
void MainWindow::setCanState(bool available) { m_canState->setText(available ? "Yes" : "No"); }

void MainWindow::setDiagnosticValue(const QString &name, int value)
{
    if (name == "CAN_IF_UP") m_canInterfaceState->setText(value ? "Yes" : "No");
    else if (name == "WD_STATE") {
        const QMap<int, QString> states{{0,"Unknown"},{1,"Active"},{2,"Deactive"},{3,"Timeout"}};
        m_watchdogState->setText(states.value(value, "Unknown"));
    } else if (name == "ILS_STATE") {
        const QMap<int, QString> states{{0,"Unknown"},{1,"Idle"},{2,"Checked"},{3,"Random"}};
        m_ilsState->setText(states.value(value, "Unknown"));
    } else if (name == "FAULT_CT") m_faultCount->setText(QString::number(value));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_client->disconnectFromVehicle();
    event->accept();
}
