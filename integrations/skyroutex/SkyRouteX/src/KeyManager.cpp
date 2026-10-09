#include "KeyManager.h"

#include <QSettings>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDebug>

#define SECRET_SALT "BonV_Secret_2026_Key_SkyRouteX"

#define ORG_NAME "BonV"
#define APP_NAME "SkyRouteX"

QString KeyManager::getMachineId()
{
    QSettings reg(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
        QSettings::NativeFormat);

    QString machineGuid =
        reg.value("MachineGuid").toString();

    machineGuid.remove("-");

    return machineGuid
        .toUpper()
        .trimmed();
}

QString KeyManager::generateSignature(
    const QString& machineId,
    const QString& expiry)
{
    QString normalizedId = machineId;
    normalizedId.remove("-");
    normalizedId = normalizedId.toUpper().trimmed();

    QString normalizedExpiry =
        expiry.trimmed();

    QString data =
        normalizedId +
        normalizedExpiry +
        SECRET_SALT;

    QByteArray hash =
        QCryptographicHash::hash(
            data.toUtf8(),
            QCryptographicHash::Sha256);

    return QString(hash.toHex())
        .left(12)
        .toUpper();
}

KeyManager::ValidationResult
KeyManager::validateKeyDetailed(
    const QString& key)
{
    QStringList parts =
        key.split("-");

    if (parts.size() != 7)
    {
        qDebug() << "Invalid key format";
        return InvalidFormat;
    }

    QString expiry =
        parts.at(5).trimmed();

    QString providedSignature =
        parts.at(6).trimmed();

    QDate expiryDate =
        QDate::fromString(
            expiry,
            "yyyyMMdd");

    if (!expiryDate.isValid())
    {
        qDebug() << "Invalid expiry format";
        return InvalidFormat;
    }

    if (expiryDate < QDate::currentDate())
    {
        qDebug() << "Key expired";
        return Expired;
    }

    QString machineId =
        getMachineId();

    QString expectedSignature =
        generateSignature(
            machineId,
            expiry);

    if (expectedSignature != providedSignature)
    {
        qDebug()
        << "Signature mismatch"
        << expectedSignature
        << providedSignature;

        return InvalidSignature;
    }

    return Valid;
}

bool KeyManager::activateKey(
    const QString& key)
{
    ValidationResult result =
        validateKeyDetailed(key);

    if (result != Valid)
        return false;

    QSettings settings(
        ORG_NAME,
        APP_NAME);

    settings.setValue(
        "license/key",
        key);

    settings.setValue(
        "license/activated",
        true);

    settings.setValue(
        "license/activated_on",
        QDateTime::currentDateTime()
            .toString(Qt::ISODate));

    qDebug()
        << "Key activated and stored locally";

    return true;
}

bool KeyManager::isActivated()
{
    QString key =
        getActivatedKey();

    if (key.isEmpty())
        return false;

    ValidationResult result =
        validateKeyDetailed(key);

    if (result == Expired)
    {
        clearLocalActivation();
        return false;
    }

    return result == Valid;
}

QString KeyManager::getActivatedKey()
{
    QSettings settings(
        ORG_NAME,
        APP_NAME);

    return settings.value(
                       "license/key",
                       "").toString();
}

void KeyManager::clearLocalActivation()
{
    QSettings settings(
        ORG_NAME,
        APP_NAME);

    settings.remove("license");

    qDebug()
        << "Local activation cleared";
}
