#include "AppBackend.h"

#include "JoystickController.h"
#include "KeyManager.h"
#include "ScreenRecorder.h"
#include "storage/AppDataPaths.h"

#include <QWindow>

namespace {
const QString LOGIN_PASSWORD = QStringLiteral("098765");
}

AppBackend::AppBackend(QObject *parent)
    : QObject(parent)
    , screenRecorder(new ScreenRecorder(this))
    , joystickController(new JoystickController(this))
{
    AppDataPaths::ensureAllDirectories();
    connect(screenRecorder, &ScreenRecorder::recordingChanged, this, &AppBackend::screenRecordingChanged);
    connect(screenRecorder, &ScreenRecorder::recordingFilePathChanged, this, &AppBackend::screenRecordingFilePathChanged);
    connect(screenRecorder, &ScreenRecorder::errorStringChanged, this, &AppBackend::screenRecordingErrorChanged);

    connect(joystickController, &JoystickController::devicesChanged, this, &AppBackend::joystickDevicesChanged);
    connect(joystickController, &JoystickController::selectedDeviceChanged, this, &AppBackend::joystickSelectedDeviceChanged);
    connect(joystickController, &JoystickController::statusChanged, this, &AppBackend::joystickStatusChanged);
    connect(joystickController, &JoystickController::enabledChanged, this, &AppBackend::joystickEnabledChanged);
    connect(joystickController, &JoystickController::calibrationChanged, this, &AppBackend::joystickCalibrationChanged);
    connect(joystickController, &JoystickController::messageChanged, this, &AppBackend::joystickMessageChanged);
    connect(joystickController, &JoystickController::liveInputChanged, this, &AppBackend::joystickLiveInputChanged);
    connect(joystickController, &JoystickController::axisRangesChanged, this, &AppBackend::joystickAxisRangesChanged);
    connect(joystickController, &JoystickController::assignmentsChanged, this, &AppBackend::joystickAssignmentsChanged);
    connect(joystickController, &JoystickController::buttonAssignmentsChanged, this, &AppBackend::joystickButtonAssignmentsChanged);
}

AppBackend::~AppBackend() = default;

bool AppBackend::screenRecording() const
{
    return screenRecorder && screenRecorder->recording();
}

QString AppBackend::screenRecordingFilePath() const
{
    return screenRecorder ? screenRecorder->recordingFilePath() : QString();
}

QString AppBackend::screenRecordingError() const
{
    return screenRecorder ? screenRecorder->errorString() : QString();
}

QStringList AppBackend::joystickDevices() const
{
    return joystickController ? joystickController->devices() : QStringList();
}

QString AppBackend::joystickSelectedDevice() const
{
    return joystickController ? joystickController->selectedDevice() : QString();
}

bool AppBackend::joystickAvailable() const
{
    return joystickController && joystickController->available();
}

bool AppBackend::joystickEnabled() const
{
    return joystickController && joystickController->enabled();
}

bool AppBackend::joystickCalibrating() const
{
    return joystickController && joystickController->calibrating();
}

bool AppBackend::joystickCalibrated() const
{
    return joystickController && joystickController->calibrated();
}

QString AppBackend::joystickMessage() const
{
    return joystickController ? joystickController->message() : QString();
}

QVariantMap AppBackend::joystickStatus() const
{
    return joystickController ? joystickController->status() : QVariantMap();
}

QVariantMap AppBackend::joystickLiveInput() const
{
    return joystickController ? joystickController->liveInput() : QVariantMap();
}

QVariantList AppBackend::joystickAxisRanges() const
{
    return joystickController ? joystickController->axisRanges() : QVariantList();
}

QVariantList AppBackend::joystickAssignments() const
{
    return joystickController ? joystickController->assignments() : QVariantList();
}

QVariantList AppBackend::joystickButtonAssignments() const
{
    return joystickController ? joystickController->buttonAssignments() : QVariantList();
}

bool AppBackend::isActivated()
{
    return KeyManager::isActivated();
}

bool AppBackend::isAndroid() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

QString AppBackend::activationId() const
{
    return KeyManager::getMachineId();
}

QString AppBackend::activationStatus() const
{
    const QString key = KeyManager::getActivatedKey();

    if (key.isEmpty())
        return QString();

    switch (KeyManager::validateKeyDetailed(key)) {
    case KeyManager::Expired:
        return QStringLiteral("EXPIRED");
    case KeyManager::Valid:
        return QStringLiteral("VALID");
    default:
        return QStringLiteral("INVALID");
    }
}

QString AppBackend::applicationDataRoot() const
{
    AppDataPaths::ensureAllDirectories();
    return AppDataPaths::rootDirectory();
}

QString AppBackend::logsDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::logsDirectory());
    return AppDataPaths::logsDirectory();
}

QString AppBackend::missionsDirectoryUrl() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::missionsDirectory());
    return AppDataPaths::directoryUrl(AppDataPaths::missionsDirectory());
}

QString AppBackend::telemetryLogDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::telemetryLogsDirectory());
    return AppDataPaths::telemetryLogsDirectory();
}

QString AppBackend::ardupilotLogDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::ardupilotLogsDirectory());
    return AppDataPaths::ardupilotLogsDirectory();
}

QString AppBackend::videoRecordingsDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::videoRecordingsDirectory());
    return AppDataPaths::videoRecordingsDirectory();
}

QString AppBackend::screenRecordingsDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::screenRecordingsDirectory());
    return AppDataPaths::screenRecordingsDirectory();
}

QString AppBackend::terrainDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::terrainDirectory());
    return AppDataPaths::terrainDirectory();
}

QString AppBackend::rasterOverlaysDirectory() const
{
    AppDataPaths::ensureDirectory(AppDataPaths::rasterOverlaysDirectory());
    return AppDataPaths::rasterOverlaysDirectory();
}

bool AppBackend::startScreenRecording(QObject *windowObject)
{
    QWindow *window = qobject_cast<QWindow *>(windowObject);
    return screenRecorder && screenRecorder->start(window);
}

void AppBackend::stopScreenRecording()
{
    if (screenRecorder) {
        screenRecorder->stop();
    }
}

bool AppBackend::toggleScreenRecording(QObject *windowObject)
{
    QWindow *window = qobject_cast<QWindow *>(windowObject);
    return screenRecorder && screenRecorder->toggle(window);
}

void AppBackend::refreshJoystickDevices()
{
    if (joystickController)
        joystickController->refreshDevices();
}

bool AppBackend::selectJoystickDevice(const QString &deviceName)
{
    return joystickController && joystickController->selectDevice(deviceName);
}

void AppBackend::setJoystickMavlink(QObject *mavlinkObject)
{
    if (joystickController)
        joystickController->setMavlink(mavlinkObject);
}

bool AppBackend::beginJoystickCalibration()
{
    return joystickController && joystickController->beginCalibration();
}

bool AppBackend::finishJoystickCalibration()
{
    return joystickController && joystickController->finishCalibration();
}

void AppBackend::cancelJoystickCalibration()
{
    if (joystickController)
        joystickController->cancelCalibration();
}

void AppBackend::beginJoystickAxisDetection(const QString &controlName)
{
    if (joystickController)
        joystickController->beginAxisDetection(controlName);
}

void AppBackend::beginJoystickButtonDetection(int row)
{
    if (joystickController)
        joystickController->beginButtonDetection(row);
}

void AppBackend::setJoystickAxisInverted(const QString &controlName, bool inverted)
{
    if (joystickController)
        joystickController->setAxisInverted(controlName, inverted);
}

void AppBackend::setJoystickButtonAction(int row, const QString &actionName)
{
    if (joystickController)
        joystickController->setButtonAction(row, actionName);
}

void AppBackend::setJoystickTuning(int deadzonePercent, int expoPercent)
{
    if (joystickController)
        joystickController->setTuning(deadzonePercent, expoPercent);
}

void AppBackend::setJoystickEnabled(bool enabled)
{
    if (joystickController)
        joystickController->setEnabled(enabled);
}

void AppBackend::onValidateKey(const QString &key, const QString &activationType)
{
    Q_UNUSED(activationType);

    switch (KeyManager::validateKeyDetailed(key)) {
    case KeyManager::Valid:
        if (KeyManager::activateKey(key))
            emit activationSucceeded();
        else
            emit activationFailed(QStringLiteral("Activation failed."));
        break;

    case KeyManager::Expired:
        emit activationFailed(QStringLiteral("This license key has expired. Please enter a valid key."));
        break;

    case KeyManager::InvalidSignature:
        emit activationFailed(QStringLiteral("This key does not belong to this machine."));
        break;

    default:
        emit activationFailed(QStringLiteral("Invalid activation key."));
        break;
    }
}

bool AppBackend::verifyLoginPassword(const QString &password) const
{
    return password == LOGIN_PASSWORD;
}
