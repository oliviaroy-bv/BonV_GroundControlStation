// // #pragma once

// // #include <QAbstractListModel>
// // #include <QJsonArray>
// // #include <QJsonObject>
// // #include <QString>
// // #include <QStringList>
// // #include <QVector>

// // // ── DroneRecord ───────────────────────────────────────────────────────────────
// // struct DroneRecord {
// //     QString     id;
// //     QString     displayName;
// //     bool        active        = true;
// //     double      maxRangeM     = 5000.0;   // one-way metres
// //     double      maxSpeedMs    = 5.0;
// //     QStringList capabilities;              // subset of {"Delivery","Drop","Paradrop"}
// //     int         rcSwitch      = 0;         // RC channel (Drop/Paradrop only)
// //     int         rcCount       = 1;         // number of RC triggers (pulses) at destination
// //     double      takeoffAlt    = 30.0;
// //     double      nearHomeAlt   = 60.0;
// //     double      cruiseAlt     = 120.0;
// //     double      landAlt       = 5.0;

// //     bool supportsDelivery() const { return capabilities.contains("Delivery"); }
// //     bool supportsDrop()     const { return capabilities.contains("Drop"); }
// //     bool supportsParadrop() const { return capabilities.contains("Paradrop"); }
// //     bool needsRcSwitch()    const { return supportsDrop() || supportsParadrop(); }

// //     QJsonObject toJson() const;
// //     static DroneRecord fromJson(const QJsonObject& obj);
// // };

// // // ── DroneManager ──────────────────────────────────────────────────────────────
// // // QAbstractListModel exposing the drone fleet to QML.
// // // Loads from / saves to drones.json beside the exe.
// // // Falls back to built-in defaults if file absent.
// // //
// // // Expose to QML via:
// // //   engine.rootContext()->setContextProperty("droneManager", &dm);
// // //
// // // QML roles:
// // //   droneId, droneName, droneActive, droneRangeKm, droneSpeedMs,
// // //   droneCapabilities, droneRcSwitch,
// // //   droneTakeoffAlt, droneNearHomeAlt, droneCruiseAlt, droneLandAlt

// // class DroneManager : public QAbstractListModel
// // {
// //     Q_OBJECT

// //     Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

// // public:
// //     enum DroneRoles {
// //         IdRole = Qt::UserRole + 1,
// //         NameRole,
// //         ActiveRole,
// //         RangeKmRole,
// //         SpeedMsRole,
// //         CapabilitiesRole,
// //         RcSwitchRole,
// //         TakeoffAltRole,
// //         NearHomeAltRole,
// //         CruiseAltRole,
// //         LandAltRole,
// //         NeedsRcRole,
// //         RcCountRole,
// //         SupportsDeliveryRole,
// //         SupportsDropRole,
// //         SupportsParadropRole,
// //     };

// //     explicit DroneManager(QObject* parent = nullptr);

// //     // QAbstractListModel
// //     int      rowCount(const QModelIndex& parent = {}) const override;
// //     QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
// //     QHash<int, QByteArray> roleNames() const override;

// //     // ── Invokables called from QML ────────────────────────────────────────────

// //     // Returns list of active drones as QVariantList for mission form combo
// //     Q_INVOKABLE QVariantList activeDrones() const;

// //     // Add a brand-new drone — returns new index
// //     Q_INVOKABLE int addDrone(const QString& name,
// //                               double rangeKm, double speedMs,
// //                               bool delivery, bool drop, bool paradrop,
// //                               int rcSwitch, int rcCount,
// //                               double takeoffAlt, double nearHomeAlt,
// //                               double cruiseAlt,  double landAlt);

// //     // Update an existing drone by index
// //     Q_INVOKABLE void updateDrone(int index,
// //                                   const QString& name,
// //                                   double rangeKm, double speedMs,
// //                                   bool delivery, bool drop, bool paradrop,
// //                                   int rcSwitch, int rcCount,
// //                                   double takeoffAlt, double nearHomeAlt,
// //                                   double cruiseAlt,  double landAlt);

// //     // Toggle active/inactive
// //     Q_INVOKABLE void setActive(int index, bool active);

// //     // Deactivate (soft-delete) — never fully removes from file
// //     Q_INVOKABLE void deactivate(int index);

// //     // Persist to drones.json
// //     Q_INVOKABLE void save();

// //     // Reload from file
// //     Q_INVOKABLE void reload();

// //     // Return one drone as QVariantMap (for edit form pre-fill)
// //     Q_INVOKABLE QVariantMap droneAt(int index) const;

// // signals:
// //     void countChanged();
// //     void dronesChanged();   // emit after any mutation — QML list refreshes

// // private:
// //     void    loadDefaults();
// //     void    loadFromFile();
// //     QString jsonPath() const;

// //     QVector<DroneRecord> m_drones;
// // };


// #pragma once

// #include <QAbstractListModel>
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QString>
// #include <QStringList>
// #include <QVector>

// // ── DroneRecord ───────────────────────────────────────────────────────────────
// struct DroneRecord {
//     QString     id;
//     QString     displayName;
//     bool        active        = true;
//     double      maxRangeM     = 5000.0;   // one-way metres
//     double      maxSpeedMs    = 5.0;
//     QStringList capabilities;              // subset of {"Delivery","Drop","Paradrop"}
//     int         rcSwitch      = 0;         // RC channel (Drop/Paradrop only)
//     int         rcCount       = 1;         // number of RC triggers (pulses) at destination
//     double      takeoffAlt    = 30.0;
//     double      nearHomeAlt   = 60.0;
//     double      cruiseAlt     = 120.0;
//     double      landAlt       = 5.0;

//     bool supportsDelivery() const { return capabilities.contains("Delivery"); }
//     bool supportsDrop()     const { return capabilities.contains("Drop"); }
//     bool supportsParadrop() const { return capabilities.contains("Paradrop"); }
//     bool needsRcSwitch()    const { return supportsDrop() || supportsParadrop(); }

//     QJsonObject toJson() const;
//     static DroneRecord fromJson(const QJsonObject& obj);
// };

// // ── DroneManager ──────────────────────────────────────────────────────────────
// // QAbstractListModel exposing the drone fleet to QML.
// // Loads from / saves to drones.json beside the exe.
// // Falls back to built-in defaults if file absent.
// //
// // Expose to QML via:
// //   engine.rootContext()->setContextProperty("droneManager", &dm);
// //
// // QML roles:
// //   droneId, droneName, droneActive, droneRangeKm, droneSpeedMs,
// //   droneCapabilities, droneRcSwitch,
// //   droneTakeoffAlt, droneNearHomeAlt, droneCruiseAlt, droneLandAlt

// class DroneManager : public QAbstractListModel
// {
//     Q_OBJECT

//     Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

// public:
//     enum DroneRoles {
//         IdRole = Qt::UserRole + 1,
//         NameRole,
//         ActiveRole,
//         RangeKmRole,
//         SpeedMsRole,
//         CapabilitiesRole,
//         RcSwitchRole,
//         TakeoffAltRole,
//         NearHomeAltRole,
//         CruiseAltRole,
//         LandAltRole,
//         NeedsRcRole,
//         RcCountRole,
//         SupportsDeliveryRole,
//         SupportsDropRole,
//         SupportsParadropRole,
//     };

//     explicit DroneManager(QObject* parent = nullptr);

//     // QAbstractListModel
//     int      rowCount(const QModelIndex& parent = {}) const override;
//     QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
//     QHash<int, QByteArray> roleNames() const override;

//     // ── Invokables called from QML ────────────────────────────────────────────

//     // Returns list of active drones as QVariantList for mission form combo
//     Q_INVOKABLE QVariantList activeDrones() const;

//     // Add a brand-new drone — returns new index
//     Q_INVOKABLE int addDrone(const QString& name,
//                               double rangeKm, double speedMs,
//                               bool delivery, bool drop, bool paradrop,
//                               int rcSwitch, int rcCount,
//                               double takeoffAlt, double nearHomeAlt,
//                               double cruiseAlt,  double landAlt);

//     // Update an existing drone by index
//     Q_INVOKABLE void updateDrone(int index,
//                                   const QString& name,
//                                   double rangeKm, double speedMs,
//                                   bool delivery, bool drop, bool paradrop,
//                                   int rcSwitch, int rcCount,
//                                   double takeoffAlt, double nearHomeAlt,
//                                   double cruiseAlt,  double landAlt);

//     // Toggle active/inactive
//     Q_INVOKABLE void setActive(int index, bool active);

//     // Deactivate (soft-delete) — never fully removes from file
//     Q_INVOKABLE void deactivate(int index);

//     // Persist to drones.json
//     Q_INVOKABLE void save();

//     // Reload from file
//     Q_INVOKABLE void reload();

//     // Delete drones.json and reload factory defaults
//     Q_INVOKABLE void resetToDefaults();

//     // Return one drone as QVariantMap (for edit form pre-fill)
//     Q_INVOKABLE QVariantMap droneAt(int index) const;

// signals:
//     void countChanged();
//     void dronesChanged();   // emit after any mutation — QML list refreshes

// private:
//     void    loadDefaults();
//     void    loadFromFile();
//     QString jsonPath() const;

//     QVector<DroneRecord> m_drones;
// };



#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ── DroneRecord ───────────────────────────────────────────────────────────────
struct DroneRecord {
    QString     id;
    QString     displayName;
    bool        active        = true;
    double      maxRangeM     = 5000.0;   // one-way metres
    double      minSpeedMs    = 2.0;
    double      maxSpeedMs    = 10.0;
    QStringList capabilities;              // subset of {"Delivery","Drop","Paradrop"}
    int         rcSwitch      = 0;         // RC channel (Drop/Paradrop only)
    int         rcCount       = 1;         // number of RC triggers (pulses) at destination
    // Default + min/max for each altitude parameter
    double      takeoffAlt    = 30.0;
    double      minTakeoffAlt = 10.0;
    double      maxTakeoffAlt = 100.0;
    double      nearHomeAlt   = 60.0;
    double      minNearHomeAlt= 20.0;
    double      maxNearHomeAlt= 200.0;
    double      cruiseAlt     = 120.0;
    double      minCruiseAlt  = 80.0;
    double      maxCruiseAlt  = 500.0;
    double      landAlt       = 80.0;
    double      minLandAlt    = 80.0;
    double      maxLandAlt    = 150.0;

    bool supportsDelivery() const { return capabilities.contains("Delivery"); }
    bool supportsDrop()     const { return capabilities.contains("Drop"); }
    bool supportsParadrop() const { return capabilities.contains("Paradrop"); }
    bool needsRcSwitch()    const { return supportsDrop() || supportsParadrop(); }

    QJsonObject toJson() const;
    static DroneRecord fromJson(const QJsonObject& obj);
};

// ── DroneManager ──────────────────────────────────────────────────────────────
// QAbstractListModel exposing the drone fleet to QML.
// Loads from / saves to drones.json beside the exe.
// Falls back to built-in defaults if file absent.
//
// Expose to QML via:
//   engine.rootContext()->setContextProperty("droneManager", &dm);
//
// QML roles:
//   droneId, droneName, droneActive, droneRangeKm, droneSpeedMs,
//   droneCapabilities, droneRcSwitch,
//   droneTakeoffAlt, droneNearHomeAlt, droneCruiseAlt, droneLandAlt

class DroneManager : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum DroneRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ActiveRole,
        RangeKmRole,
        SpeedMsRole,
        CapabilitiesRole,
        RcSwitchRole,
        TakeoffAltRole,
        NearHomeAltRole,
        CruiseAltRole,
        LandAltRole,
        NeedsRcRole,
        RcCountRole,
        MinSpeedRole,
        SupportsDeliveryRole,
        MinTakeoffAltRole,
        MaxTakeoffAltRole,
        MinNearHomeAltRole,
        MaxNearHomeAltRole,
        MinCruiseAltRole,
        MaxCruiseAltRole,
        MinLandAltRole,
        MaxLandAltRole,
        SupportsDropRole,
        SupportsParadropRole,
    };

    explicit DroneManager(QObject* parent = nullptr);

    // QAbstractListModel
    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ── Invokables called from QML ────────────────────────────────────────────

    // Returns list of active drones as QVariantList for mission form combo
    Q_INVOKABLE QVariantList activeDrones() const;

    // Add a brand-new drone — returns new index
    Q_INVOKABLE int addDrone(const QString& name,
                              double rangeKm, double minSpeedMs, double maxSpeedMs,
                              bool delivery, bool drop, bool paradrop,
                              int rcSwitch, int rcCount,
                              double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
                              double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
                              double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
                              double landAlt, double minLandAlt, double maxLandAlt);

    // Update an existing drone by index
    Q_INVOKABLE void updateDrone(int index,
                                  const QString& name,
                                  double rangeKm, double minSpeedMs, double maxSpeedMs,
                                  bool delivery, bool drop, bool paradrop,
                                  int rcSwitch, int rcCount,
                                  double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
                                  double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
                                  double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
                                  double landAlt, double minLandAlt, double maxLandAlt);

    // Toggle active/inactive
    Q_INVOKABLE void setActive(int index, bool active);

    // Deactivate (soft-delete) — never fully removes from file
    Q_INVOKABLE void deactivate(int index);

    // Persist to drones.json
    Q_INVOKABLE void save();

    // Reload from file
    Q_INVOKABLE void reload();

    // Delete drones.json and reload factory defaults
    Q_INVOKABLE void resetToDefaults();

    // Return one drone as QVariantMap (for edit form pre-fill)
    Q_INVOKABLE QVariantMap droneAt(int index) const;

signals:
    void countChanged();
    void dronesChanged();   // emit after any mutation — QML list refreshes

private:
    void    loadDefaults();
    void    loadFromFile();
    QString jsonPath() const;

    QVector<DroneRecord> m_drones;
};