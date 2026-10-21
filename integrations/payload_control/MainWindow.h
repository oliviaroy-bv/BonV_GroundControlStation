#pragma once

#include <QMainWindow>
#include <QVector>

class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QPlainTextEdit;
class MavlinkStatusClient;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void connectVehicle();
    void requestStatus();
    void requestDiagnostic(int request);
    void sendLedCommand(const QString &name);
    void updateLedButtons(const QString &state);
    void setCanState(bool available);
    void setDiagnosticValue(const QString &name, int value);

private:
    struct LedCommandSpec { int commandId; QString description; bool hasWatchdogArgument = false; };
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QComboBox *m_transport = nullptr;
    QLineEdit *m_serialDevice = nullptr;
    QSpinBox *m_serialBaud = nullptr;
    QSpinBox *m_vehicleSystemId = nullptr;
    QSpinBox *m_watchdogMinutes = nullptr;
    QLabel *m_connectionState = nullptr;
    QLabel *m_canState = nullptr;
    QLabel *m_canInterfaceState = nullptr;
    QLabel *m_watchdogState = nullptr;
    QLabel *m_ilsState = nullptr;
    QLabel *m_faultCount = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QVector<QPushButton *> m_ledButtons;
    MavlinkStatusClient *m_client = nullptr;
    QMap<QString, LedCommandSpec> m_ledCommands;
};
