#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QWindow;
class JoystickController;
class ScreenRecorder;

// AppBackend is the QML-facing bridge for activation and login.
// Registered in main.cpp as:
//   engine.rootContext()->setContextProperty("backend", &appBackend);
class AppBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool screenRecording READ screenRecording NOTIFY screenRecordingChanged)
    Q_PROPERTY(QString screenRecordingFilePath READ screenRecordingFilePath NOTIFY screenRecordingFilePathChanged)
    Q_PROPERTY(QString screenRecordingError READ screenRecordingError NOTIFY screenRecordingErrorChanged)
    Q_PROPERTY(QStringList joystickDevices READ joystickDevices NOTIFY joystickDevicesChanged)
    Q_PROPERTY(QString joystickSelectedDevice READ joystickSelectedDevice WRITE selectJoystickDevice NOTIFY joystickSelectedDeviceChanged)
    Q_PROPERTY(bool joystickAvailable READ joystickAvailable NOTIFY joystickStatusChanged)
    Q_PROPERTY(bool joystickEnabled READ joystickEnabled WRITE setJoystickEnabled NOTIFY joystickEnabledChanged)
    Q_PROPERTY(bool joystickCalibrating READ joystickCalibrating NOTIFY joystickCalibrationChanged)
    Q_PROPERTY(bool joystickCalibrated READ joystickCalibrated NOTIFY joystickCalibrationChanged)
    Q_PROPERTY(QString joystickMessage READ joystickMessage NOTIFY joystickMessageChanged)
    Q_PROPERTY(QVariantMap joystickStatus READ joystickStatus NOTIFY joystickStatusChanged)
    Q_PROPERTY(QVariantMap joystickLiveInput READ joystickLiveInput NOTIFY joystickLiveInputChanged)
    Q_PROPERTY(QVariantList joystickAxisRanges READ joystickAxisRanges NOTIFY joystickAxisRangesChanged)
    Q_PROPERTY(QVariantList joystickAssignments READ joystickAssignments NOTIFY joystickAssignmentsChanged)
    Q_PROPERTY(QVariantList joystickButtonAssignments READ joystickButtonAssignments NOTIFY joystickButtonAssignmentsChanged)

public:
    explicit AppBackend(QObject *parent = nullptr);
    ~AppBackend() override;

    bool screenRecording() const;
    QString screenRecordingFilePath() const;
    QString screenRecordingError() const;
    QStringList joystickDevices() const;
    QString joystickSelectedDevice() const;
    bool joystickAvailable() const;
    bool joystickEnabled() const;
    bool joystickCalibrating() const;
    bool joystickCalibrated() const;
    QString joystickMessage() const;
    QVariantMap joystickStatus() const;
    QVariantMap joystickLiveInput() const;
    QVariantList joystickAxisRanges() const;
    QVariantList joystickAssignments() const;
    QVariantList joystickButtonAssignments() const;

signals:
    void activationSucceeded();
    void activationFailed(QString message);
    void screenRecordingChanged();
    void screenRecordingFilePathChanged();
    void screenRecordingErrorChanged();
    void joystickDevicesChanged();
    void joystickSelectedDeviceChanged();
    void joystickStatusChanged();
    void joystickEnabledChanged();
    void joystickCalibrationChanged();
    void joystickMessageChanged();
    void joystickLiveInputChanged();
    void joystickAxisRangesChanged();
    void joystickAssignmentsChanged();
    void joystickButtonAssignmentsChanged();

public slots:
    Q_INVOKABLE bool isActivated();
    Q_INVOKABLE bool isAndroid() const;
    Q_INVOKABLE QString activationId() const;
    Q_INVOKABLE QString activationStatus() const;
    Q_INVOKABLE QString applicationDataRoot() const;
    Q_INVOKABLE QString logsDirectory() const;
    Q_INVOKABLE QString missionsDirectoryUrl() const;
    Q_INVOKABLE QString telemetryLogDirectory() const;
    Q_INVOKABLE QString ardupilotLogDirectory() const;
    Q_INVOKABLE QString videoRecordingsDirectory() const;
    Q_INVOKABLE QString screenRecordingsDirectory() const;
    Q_INVOKABLE QString terrainDirectory() const;
    Q_INVOKABLE QString rasterOverlaysDirectory() const;
    Q_INVOKABLE bool startScreenRecording(QObject *windowObject);
    Q_INVOKABLE void stopScreenRecording();
    Q_INVOKABLE bool toggleScreenRecording(QObject *windowObject);
    Q_INVOKABLE void refreshJoystickDevices();
    Q_INVOKABLE bool selectJoystickDevice(const QString &deviceName);
    Q_INVOKABLE void setJoystickMavlink(QObject *mavlinkObject);
    Q_INVOKABLE bool beginJoystickCalibration();
    Q_INVOKABLE bool finishJoystickCalibration();
    Q_INVOKABLE void cancelJoystickCalibration();
    Q_INVOKABLE void beginJoystickAxisDetection(const QString &controlName);
    Q_INVOKABLE void beginJoystickButtonDetection(int row);
    Q_INVOKABLE void setJoystickAxisInverted(const QString &controlName, bool inverted);
    Q_INVOKABLE void setJoystickButtonAction(int row, const QString &actionName);
    Q_INVOKABLE void setJoystickTuning(int deadzonePercent, int expoPercent);
    Q_INVOKABLE void setJoystickEnabled(bool enabled);
    Q_INVOKABLE void onValidateKey(const QString &key, const QString &activationType);

    Q_INVOKABLE bool verifyLoginPassword(const QString &password) const;

private:
    ScreenRecorder *screenRecorder = nullptr;
    JoystickController *joystickController = nullptr;
};
