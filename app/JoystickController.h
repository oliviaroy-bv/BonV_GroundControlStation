#pragma once

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class Mavlink;
class QTimer;

class JoystickController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString selectedDevice READ selectedDevice WRITE selectDevice NOTIFY selectedDeviceChanged)
    Q_PROPERTY(bool available READ available NOTIFY statusChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool calibrating READ calibrating NOTIFY calibrationChanged)
    Q_PROPERTY(bool calibrated READ calibrated NOTIFY calibrationChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap liveInput READ liveInput NOTIFY liveInputChanged)
    Q_PROPERTY(QVariantList axisRanges READ axisRanges NOTIFY axisRangesChanged)
    Q_PROPERTY(QVariantList assignments READ assignments NOTIFY assignmentsChanged)
    Q_PROPERTY(QVariantList buttonAssignments READ buttonAssignments NOTIFY buttonAssignmentsChanged)

public:
    explicit JoystickController(QObject *parent = nullptr);

    QStringList devices() const;
    QString selectedDevice() const;
    bool available() const;
    bool enabled() const;
    bool calibrating() const;
    bool calibrated() const;
    QString message() const;
    QVariantMap status() const;
    QVariantMap liveInput() const;
    QVariantList axisRanges() const;
    QVariantList assignments() const;
    QVariantList buttonAssignments() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE bool selectDevice(const QString &deviceName);
    Q_INVOKABLE void setMavlink(QObject *mavlinkObject);
    Q_INVOKABLE bool beginCalibration();
    Q_INVOKABLE bool finishCalibration();
    Q_INVOKABLE void cancelCalibration();
    Q_INVOKABLE void beginAxisDetection(const QString &controlName);
    Q_INVOKABLE void beginButtonDetection(int row);
    Q_INVOKABLE void setAxisInverted(const QString &controlName, bool inverted);
    Q_INVOKABLE void setButtonAction(int row, const QString &actionName);
    Q_INVOKABLE void setTuning(int deadzonePercent, int expoPercent);
    Q_INVOKABLE void setEnabled(bool enabled);

signals:
    void devicesChanged();
    void selectedDeviceChanged();
    void enabledChanged();
    void calibrationChanged();
    void messageChanged();
    void statusChanged();
    void liveInputChanged();
    void axisRangesChanged();
    void assignmentsChanged();
    void buttonAssignmentsChanged();

private slots:
    void poll();

private:
    struct DeviceInfo {
        int id = -1;
        QString name;
        int axes = 0;
        int buttons = 0;
        bool hasPov = false;
    };

    struct AxisState {
        QString label;
        int defaultMinimum = 0;
        int defaultMaximum = 65535;
        int minimum = 0;
        int center = 32768;
        int maximum = 65535;
        int current = 32768;
        bool present = false;
    };

    struct ControlAssignment {
        QString control;
        QString icon;
        QString accent;
        QString binding;
        bool inverted = false;
    };

    struct ButtonAssignment {
        int button = -1;
        QString action;
        bool calibrated = false;
    };

    void loadSettings();
    void saveSettings() const;
    void setMessage(const QString &newMessage);
    bool readCurrentState(QVariantMap *state = nullptr);
    void updateDeviceStatus(const DeviceInfo &device);
    void updateAxisFromRaw(const QString &label, int value, int minimum, int maximum, bool present);
    AxisState axisState(const QString &label) const;
    int normalizeSigned(const ControlAssignment &assignment) const;
    int normalizeThrottle(const ControlAssignment &assignment) const;
    int applyTuning(int value) const;
    int signedControlToPwm(int value) const;
    int throttleControlToPwm(int value) const;
    int buttonsLow16() const;
    int buttonsHigh16() const;
    int totalButtonCount() const;
    int calibratedButtonCount() const;
    quint32 povButtons(int pov) const;
    QString buttonLabel(int buttonIndex) const;
    bool requiredAssignmentsAvailable(QStringList *missingControls = nullptr) const;
    bool minimumButtonsCalibrated() const;
    void handleButtonActions();
    void executeButtonAction(const ButtonAssignment &assignment);
    void sendManualControl();
    QVariantMap axisDisplayMap(const ControlAssignment &assignment) const;
    QVariantMap buttonDisplayMap(const ButtonAssignment &assignment, int row) const;
    ControlAssignment *assignmentForControl(const QString &controlName);
    const ControlAssignment *assignmentForControl(const QString &controlName) const;
    DeviceInfo selectedDeviceInfo() const;
    void resetDefaultAssignments();
    void emitLiveStateChanged();

    QTimer *pollTimer = nullptr;
    QPointer<Mavlink> mavlink;
    QVector<DeviceInfo> deviceInfos;
    QStringList deviceNames;
    QString selectedDeviceName;
    QVariantMap currentLiveInput;
    QHash<QString, AxisState> axisStates;
    QHash<QString, AxisState> calibrationAxisBackup;
    QVector<ControlAssignment> controlAssignments;
    QVector<ButtonAssignment> buttonAssignmentsState;
    QString pendingDetectionControl;
    int pendingButtonDetectionRow = -1;
    QVariantMap detectionBaseline;
    quint32 detectionButtonsBaseline = 0;
    QString statusText = QStringLiteral("No joystick selected");
    bool enabledState = false;
    bool calibratingState = false;
    bool calibratedState = false;
    bool deviceConnectedState = false;
    int selectedDeviceId = -1;
    int selectedAxes = 0;
    int selectedButtons = 0;
    bool selectedHasPov = false;
    quint32 currentButtons = 0;
    quint32 previousButtons = 0;
    int currentPov = -1;
    int deviceRefreshTick = 0;
    int deadzone = 8;
    int expo = 25;
};
