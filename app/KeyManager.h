#pragma once

#include <QString>

class KeyManager
{
public:
    enum ValidationResult {
        Valid,
        Expired,
        InvalidFormat,
        InvalidSignature
    };

    static QString getMachineId();

    static QString generateSignature(
        const QString& machineId,
        const QString& expiry);

    static ValidationResult validateKeyDetailed(
        const QString& key);

    static bool activateKey(
        const QString& key);

    static bool isActivated();

    static QString getActivatedKey();

    static void clearLocalActivation();
};
