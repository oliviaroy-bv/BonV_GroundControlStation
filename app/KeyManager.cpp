// #ifdef Q_OS_ANDROID
// #include <QJniObject>
// #include <QJniEnvironment>
// #endif

#include "KeyManager.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif


#include <QSettings>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDebug>

#define SECRET_SALT "BonV_Secret_2026_Key_BonVAeroGcs"

// QString KeyManager::getMachineId()
// {
//     QSettings reg(
//         "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
//         QSettings::NativeFormat);

//     QString machineGuid =
//         reg.value("MachineGuid").toString();

//     machineGuid.remove("-");

//     return machineGuid
//         .toUpper()
//         .trimmed();
// }

// QString KeyManager::getMachineId()
// {
// #ifdef Q_OS_ANDROID

//     QJniObject context =
//         QNativeInterface::QAndroidApplication::context();

//     QJniObject resolver =
//         context.callObjectMethod(
//             "getContentResolver",
//             "()Landroid/content/ContentResolver;");

//     QJniObject androidId =
//         QJniObject::callStaticObjectMethod(
//             "android/provider/Settings$Secure",
//             "getString",
//             "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
//             resolver.object(),
//             QJniObject::fromString("android_id").object());

//     QString id =
//         androidId.toString()
//             .toUpper()
//             .trimmed();

//     qDebug() << "ANDROID_ID:" << id;

//     return id;

// #else

//     QSettings reg(
//         "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
//         QSettings::NativeFormat);

//     QString id =
//         reg.value("MachineGuid").toString();

//     id.remove("-");

//     id = id.toUpper().trimmed();

//     qDebug() << "MachineGuid:" << id;

//     return id;

// #endif
// }

#ifdef Q_OS_ANDROID
QJniObject testObject;
#endif

// QString KeyManager::getMachineId()
// {
// // #ifdef Q_OS_ANDROID

// //     QJniObject context =
// //         QJniObject::callStaticObjectMethod(
// //             "org/qtproject/qt/android/QtNative",
// //             "context",
// //             "()Landroid/content/Context;");

// //     if (!context.isValid()) {
// //         qDebug() << "Failed to get Android context";
// //         return QString();
// //     }

// //     QJniObject resolver =
// //         context.callObjectMethod(
// //             "getContentResolver",
// //             "()Landroid/content/ContentResolver;");

// //     QJniObject androidId =
// //         QJniObject::callStaticObjectMethod(
// //             "android/provider/Settings$Secure",
// //             "getString",
// //             "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
// //             resolver.object(),
// //             QJniObject::fromString("android_id").object<jstring>());

// //     QString id =
// //         androidId.toString()
// //             .toUpper()
// //             .trimmed();

// //     qDebug() << "ANDROID_ID:" << id;

// //     return id;

// #ifdef Q_OS_ANDROID

//     // qDebug() << "ANDROID PATH ENTERED";

//     // QJniObject context =
//     //     QJniObject::callStaticObjectMethod(
//     //         "org/qtproject/qt/android/QtNative",
//     //         "context",
//     //         "()Landroid/content/Context;");

//     // qDebug() << "Context valid:" << context.isValid();

//     // if (!context.isValid()) {
//     //     qDebug() << "Failed to get Android context";
//     //     return QString();
//     // }

//     // QJniObject resolver =
//     //     context.callObjectMethod(
//     //         "getContentResolver",
//     //         "()Landroid/content/ContentResolver;");

//     // qDebug() << "Resolver valid:" << resolver.isValid();

//     // QJniObject androidId =
//     //     QJniObject::callStaticObjectMethod(
//     //         "android/provider/Settings$Secure",
//     //         "getString",
//     //         "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
//     //         resolver.object(),
//     //         QJniObject::fromString("android_id").object());

//     // qDebug() << "Android ID object valid:" << androidId.isValid();

//     // QString id = androidId.toString();

//     // qDebug() << "Raw Android ID:" << id;

//     // return id.toUpper().trimmed();
//     // return "ANDROID_TEST_123";
// #else

//     QSettings reg(
//         "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
//         QSettings::NativeFormat);

//     QString id =
//         reg.value("MachineGuid").toString();

//     id.remove("-");

//     id = id.toUpper().trimmed();

//     qDebug() << "MachineGuid:" << id;

//     return id;

// #endif
// }

QString KeyManager::getMachineId()
{
#ifdef Q_OS_ANDROID

    qDebug() << "===== ANDROID DEVICE ID FETCH =====";

    // Get Qt Activity
    QJniObject activity =
        QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;");

    qDebug() << "Activity valid:" << activity.isValid();

    if (!activity.isValid()) {
        qDebug() << "Failed to get Android Activity";
        return QString();
    }

    // ContentResolver
    QJniObject resolver =
        activity.callObjectMethod(
            "getContentResolver",
            "()Landroid/content/ContentResolver;");

    qDebug() << "Resolver valid:" << resolver.isValid();

    if (!resolver.isValid()) {
        qDebug() << "Failed to get ContentResolver";
        return QString();
    }

    // android_id string
    QJniObject androidIdKey =
        QJniObject::fromString("android_id");

    // Settings.Secure.getString(...)
    QJniObject androidId =
        QJniObject::callStaticObjectMethod(
            "android/provider/Settings$Secure",
            "getString",
            "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
            resolver.object(),
            androidIdKey.object());

    qDebug() << "AndroidId object valid:" << androidId.isValid();

    if (!androidId.isValid()) {
        qDebug() << "Failed to fetch ANDROID_ID";
        return QString();
    }

    QString id =
        androidId.toString()
            .toUpper()
            .trimmed();

    qDebug() << "ANDROID_ID =" << id;
    qDebug() << "===================================";

    return id;

#else

    QSettings reg(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
        QSettings::NativeFormat);

    QString machineGuid =
        reg.value("MachineGuid").toString();

    machineGuid.remove("-");

    machineGuid =
        machineGuid.toUpper().trimmed();

    qDebug() << "MachineGuid =" << machineGuid;

    return machineGuid;

#endif
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

    QSettings settings;

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
    QSettings settings;

    return settings.value(
                       "license/key",
                       "").toString();
}

void KeyManager::clearLocalActivation()
{
    QSettings settings;

    settings.remove("license");

    qDebug()
        << "Local activation cleared";
}
