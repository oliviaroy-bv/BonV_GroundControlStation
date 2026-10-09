// // // // // #include "DroneManager.h"
// // // // // #include <QCoreApplication>
// // // // // #include <QFile>
// // // // // #include <QJsonDocument>
// // // // // #include <QJsonArray>
// // // // // #include <QJsonObject>
// // // // // #include <QUuid>
// // // // // #include <QDebug>

// // // // // // ── DroneRecord ───────────────────────────────────────────────────────────────

// // // // // QJsonObject DroneRecord::toJson() const
// // // // // {
// // // // //     QJsonObject o;
// // // // //     o["id"]          = id;
// // // // //     o["displayName"] = displayName;
// // // // //     o["active"]      = active;
// // // // //     o["maxRangeM"]   = maxRangeM;
// // // // //     o["maxSpeedMs"]  = maxSpeedMs;
// // // // //     o["capabilities"]= QJsonArray::fromStringList(capabilities);
// // // // //     o["rcSwitch"]    = rcSwitch;
// // // // //     o["rcCount"]     = rcCount;
// // // // //     o["takeoffAlt"]  = takeoffAlt;
// // // // //     o["nearHomeAlt"] = nearHomeAlt;
// // // // //     o["cruiseAlt"]   = cruiseAlt;
// // // // //     o["landAlt"]     = landAlt;
// // // // //     return o;
// // // // // }

// // // // // DroneRecord DroneRecord::fromJson(const QJsonObject& o)
// // // // // {
// // // // //     DroneRecord d;
// // // // //     d.id          = o["id"].toString();
// // // // //     d.displayName = o["displayName"].toString();
// // // // //     d.active      = o["active"].toBool(true);
// // // // //     d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
// // // // //     d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
// // // // //     d.rcSwitch    = o["rcSwitch"].toInt(0);
// // // // //     d.rcCount     = o["rcCount"].toInt(1);
// // // // //     d.takeoffAlt  = o["takeoffAlt"].toDouble(30.0);
// // // // //     d.nearHomeAlt = o["nearHomeAlt"].toDouble(60.0);
// // // // //     d.cruiseAlt   = o["cruiseAlt"].toDouble(120.0);
// // // // //     d.landAlt     = o["landAlt"].toDouble(5.0);

// // // // //     QJsonArray caps = o["capabilities"].toArray();
// // // // //     for (const auto& v : caps)
// // // // //         d.capabilities.append(v.toString());
// // // // //     if (d.capabilities.isEmpty())
// // // // //         d.capabilities = {"Delivery"};

// // // // //     return d;
// // // // // }

// // // // // // ── DroneManager ──────────────────────────────────────────────────────────────

// // // // // DroneManager::DroneManager(QObject* parent)
// // // // //     : QAbstractListModel(parent)
// // // // // {
// // // // //     reload();
// // // // // }

// // // // // QString DroneManager::jsonPath() const
// // // // // {
// // // // //     return QCoreApplication::applicationDirPath() + "/drones.json";
// // // // // }

// // // // // void DroneManager::loadDefaults()
// // // // // {
// // // // //     // Built-in fleet — mirrors drone_models.h
// // // // //     m_drones = {
// // // // //         { "AIR_ORCA",   "Air-Orca",   true, 7500,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // // // //         { "PELICAN",    "Pelican",    true, 4000,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // // // //         { "UTTAM",      "Uttam",      true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // // // //         { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // // // //     };
// // // // // }

// // // // // void DroneManager::loadFromFile()
// // // // // {
// // // // //     QFile f(jsonPath());
// // // // //     if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
// // // // //         qDebug() << "DroneManager: no drones.json found, using defaults";
// // // // //         loadDefaults();
// // // // //         return;
// // // // //     }

// // // // //     QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
// // // // //     f.close();

// // // // //     if (!doc.isObject() || !doc.object().contains("drones")) {
// // // // //         qWarning() << "DroneManager: invalid drones.json, using defaults";
// // // // //         loadDefaults();
// // // // //         return;
// // // // //     }

// // // // //     m_drones.clear();
// // // // //     QJsonArray arr = doc.object()["drones"].toArray();
// // // // //     for (const auto& v : arr)
// // // // //         m_drones.append(DroneRecord::fromJson(v.toObject()));

// // // // //     qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
// // // // // }

// // // // // void DroneManager::reload()
// // // // // {
// // // // //     beginResetModel();
// // // // //     loadFromFile();
// // // // //     endResetModel();
// // // // //     emit countChanged();
// // // // //     emit dronesChanged();
// // // // // }

// // // // // void DroneManager::save()
// // // // // {
// // // // //     QJsonArray arr;
// // // // //     for (const auto& d : m_drones)
// // // // //         arr.append(d.toJson());

// // // // //     QJsonObject root;
// // // // //     root["drones"] = arr;

// // // // //     QFile f(jsonPath());
// // // // //     if (!f.open(QIODevice::WriteOnly)) {
// // // // //         qWarning() << "DroneManager: cannot write" << jsonPath();
// // // // //         return;
// // // // //     }
// // // // //     f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
// // // // //     f.close();
// // // // //     qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
// // // // // }

// // // // // // ── QAbstractListModel ────────────────────────────────────────────────────────

// // // // // int DroneManager::rowCount(const QModelIndex& parent) const
// // // // // {
// // // // //     if (parent.isValid()) return 0;
// // // // //     return m_drones.size();
// // // // // }

// // // // // QHash<int, QByteArray> DroneManager::roleNames() const
// // // // // {
// // // // //     return {
// // // // //         { IdRole,               "droneId"           },
// // // // //         { NameRole,             "droneName"         },
// // // // //         { ActiveRole,           "droneActive"       },
// // // // //         { RangeKmRole,          "droneRangeKm"      },
// // // // //         { SpeedMsRole,          "droneSpeedMs"      },
// // // // //         { CapabilitiesRole,     "droneCapabilities" },
// // // // //         { RcSwitchRole,         "droneRcSwitch"     },
// // // // //         { TakeoffAltRole,       "droneTakeoffAlt"   },
// // // // //         { NearHomeAltRole,      "droneNearHomeAlt"  },
// // // // //         { CruiseAltRole,        "droneCruiseAlt"    },
// // // // //         { LandAltRole,          "droneLandAlt"      },
// // // // //         { NeedsRcRole,          "droneNeedsRc"      },
// // // // //         { SupportsDeliveryRole, "droneDelivery"     },
// // // // //         { SupportsDropRole,     "droneDrop"         },
// // // // //         { SupportsParadropRole, "droneParadrop"     },
// // // // //     };
// // // // // }

// // // // // QVariant DroneManager::data(const QModelIndex& index, int role) const
// // // // // {
// // // // //     if (!index.isValid() || index.row() >= m_drones.size()) return {};
// // // // //     const DroneRecord& d = m_drones[index.row()];

// // // // //     switch (role) {
// // // // //         case IdRole:               return d.id;
// // // // //         case NameRole:             return d.displayName;
// // // // //         case ActiveRole:           return d.active;
// // // // //         case RangeKmRole:          return d.maxRangeM / 1000.0;
// // // // //         case SpeedMsRole:          return d.maxSpeedMs;
// // // // //         case CapabilitiesRole:     return d.capabilities;
// // // // //         case RcSwitchRole:         return d.rcSwitch;
// // // // //         case RcCountRole:          return d.rcCount;
// // // // //         case TakeoffAltRole:       return d.takeoffAlt;
// // // // //         case NearHomeAltRole:      return d.nearHomeAlt;
// // // // //         case CruiseAltRole:        return d.cruiseAlt;
// // // // //         case LandAltRole:          return d.landAlt;
// // // // //         case NeedsRcRole:          return d.needsRcSwitch();
// // // // //         case SupportsDeliveryRole: return d.supportsDelivery();
// // // // //         case SupportsDropRole:     return d.supportsDrop();
// // // // //         case SupportsParadropRole: return d.supportsParadrop();
// // // // //         default: return {};
// // // // //     }
// // // // // }

// // // // // // ── Invokables ────────────────────────────────────────────────────────────────

// // // // // QVariantList DroneManager::activeDrones() const
// // // // // {
// // // // //     QVariantList list;
// // // // //     for (const DroneRecord& d : m_drones) {
// // // // //         if (!d.active) continue;
// // // // //         QVariantMap m;
// // // // //         m["id"]          = d.id;
// // // // //         m["name"]        = d.displayName;
// // // // //         m["maxOneWayKm"] = d.maxRangeM / 1000.0;
// // // // //         m["maxSpeedMs"]  = d.maxSpeedMs;
// // // // //         m["capabilities"]= d.capabilities;
// // // // //         m["rcSwitch"]    = d.rcSwitch;
// // // // //     m["rcCount"]     = d.rcCount;
// // // // //         m["rcCount"]     = d.rcCount;
// // // // //         m["takeoffAlt"]  = d.takeoffAlt;
// // // // //         m["nearHomeAlt"] = d.nearHomeAlt;
// // // // //         m["cruiseAlt"]   = d.cruiseAlt;
// // // // //         m["landAlt"]     = d.landAlt;
// // // // //         m["needsRc"]     = d.needsRcSwitch();
// // // // //         m["delivery"]    = d.supportsDelivery();
// // // // //         m["drop"]        = d.supportsDrop();
// // // // //         m["paradrop"]    = d.supportsParadrop();
// // // // //         list.append(m);
// // // // //     }
// // // // //     return list;
// // // // // }

// // // // // QVariantMap DroneManager::droneAt(int index) const
// // // // // {
// // // // //     if (index < 0 || index >= m_drones.size()) return {};
// // // // //     const DroneRecord& d = m_drones[index];
// // // // //     QVariantMap m;
// // // // //     m["id"]          = d.id;
// // // // //     m["name"]        = d.displayName;
// // // // //     m["active"]      = d.active;
// // // // //     m["rangeKm"]     = d.maxRangeM / 1000.0;
// // // // //     m["speedMs"]     = d.maxSpeedMs;
// // // // //     m["capabilities"]= d.capabilities;
// // // // //     m["rcSwitch"]    = d.rcSwitch;
// // // // //     m["rcCount"]     = d.rcCount;
// // // // //     m["takeoffAlt"]  = d.takeoffAlt;
// // // // //     m["nearHomeAlt"] = d.nearHomeAlt;
// // // // //     m["cruiseAlt"]   = d.cruiseAlt;
// // // // //     m["landAlt"]     = d.landAlt;
// // // // //     m["needsRc"]     = d.needsRcSwitch();
// // // // //     m["delivery"]    = d.supportsDelivery();
// // // // //     m["drop"]        = d.supportsDrop();
// // // // //     m["paradrop"]    = d.supportsParadrop();
// // // // //     return m;
// // // // // }

// // // // // int DroneManager::addDrone(const QString& name,
// // // // //                             double rangeKm, double speedMs,
// // // // //                             bool delivery, bool drop, bool paradrop,
// // // // //                             int rcSwitch, int rcCount,
// // // // //                             double takeoffAlt, double nearHomeAlt,
// // // // //                             double cruiseAlt,  double landAlt)
// // // // // {
// // // // //     DroneRecord d;
// // // // //     d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
// // // // //     d.displayName = name.trimmed();
// // // // //     d.active      = true;
// // // // //     d.maxRangeM   = rangeKm * 1000.0;
// // // // //     d.maxSpeedMs  = speedMs;
// // // // //     d.rcSwitch    = rcSwitch;
// // // // //     d.rcCount     = rcCount;
// // // // //     d.takeoffAlt  = takeoffAlt;
// // // // //     d.nearHomeAlt = nearHomeAlt;
// // // // //     d.cruiseAlt   = cruiseAlt;
// // // // //     d.landAlt     = landAlt;
// // // // //     if (delivery) d.capabilities.append("Delivery");
// // // // //     if (drop)     d.capabilities.append("Drop");
// // // // //     if (paradrop) d.capabilities.append("Paradrop");
// // // // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // // // //     int row = m_drones.size();
// // // // //     beginInsertRows({}, row, row);
// // // // //     m_drones.append(d);
// // // // //     endInsertRows();

// // // // //     save();
// // // // //     emit countChanged();
// // // // //     emit dronesChanged();
// // // // //     return row;
// // // // // }

// // // // // void DroneManager::updateDrone(int index,
// // // // //                                 const QString& name,
// // // // //                                 double rangeKm, double speedMs,
// // // // //                                 bool delivery, bool drop, bool paradrop,
// // // // //                                 int rcSwitch, int rcCount,
// // // // //                                 double takeoffAlt, double nearHomeAlt,
// // // // //                                 double cruiseAlt,  double landAlt)
// // // // // {
// // // // //     if (index < 0 || index >= m_drones.size()) return;
// // // // //     DroneRecord& d  = m_drones[index];
// // // // //     d.displayName   = name.trimmed();
// // // // //     d.maxRangeM     = rangeKm * 1000.0;
// // // // //     d.maxSpeedMs    = speedMs;
// // // // //     d.rcSwitch      = rcSwitch;
// // // // //     d.rcCount       = rcCount;
// // // // //     d.takeoffAlt    = takeoffAlt;
// // // // //     d.nearHomeAlt   = nearHomeAlt;
// // // // //     d.cruiseAlt     = cruiseAlt;
// // // // //     d.landAlt       = landAlt;
// // // // //     d.capabilities.clear();
// // // // //     if (delivery) d.capabilities.append("Delivery");
// // // // //     if (drop)     d.capabilities.append("Drop");
// // // // //     if (paradrop) d.capabilities.append("Paradrop");
// // // // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // // // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0));
// // // // //     save();
// // // // //     emit dronesChanged();
// // // // // }

// // // // // void DroneManager::setActive(int index, bool active)
// // // // // {
// // // // //     if (index < 0 || index >= m_drones.size()) return;
// // // // //     m_drones[index].active = active;
// // // // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
// // // // //     save();
// // // // //     emit dronesChanged();
// // // // // }

// // // // // void DroneManager::deactivate(int index)
// // // // // {
// // // // //     setActive(index, false);
// // // // // }


// // // // #include "DroneManager.h"
// // // // #include <QCoreApplication>
// // // // #include <QFile>
// // // // #include <QJsonDocument>
// // // // #include <QJsonArray>
// // // // #include <QJsonObject>
// // // // #include <QUuid>
// // // // #include <QDebug>

// // // // // ── DroneRecord ───────────────────────────────────────────────────────────────

// // // // QJsonObject DroneRecord::toJson() const
// // // // {
// // // //     QJsonObject o;
// // // //     o["id"]          = id;
// // // //     o["displayName"] = displayName;
// // // //     o["active"]      = active;
// // // //     o["maxRangeM"]   = maxRangeM;
// // // //     o["maxSpeedMs"]  = maxSpeedMs;
// // // //     o["capabilities"]= QJsonArray::fromStringList(capabilities);
// // // //     o["rcSwitch"]    = rcSwitch;
// // // //     o["rcCount"]     = rcCount;
// // // //     o["takeoffAlt"]  = takeoffAlt;
// // // //     o["nearHomeAlt"] = nearHomeAlt;
// // // //     o["cruiseAlt"]   = cruiseAlt;
// // // //     o["landAlt"]     = landAlt;
// // // //     return o;
// // // // }

// // // // DroneRecord DroneRecord::fromJson(const QJsonObject& o)
// // // // {
// // // //     DroneRecord d;
// // // //     d.id          = o["id"].toString();
// // // //     d.displayName = o["displayName"].toString();
// // // //     d.active      = o["active"].toBool(true);
// // // //     d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
// // // //     d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
// // // //     d.rcSwitch    = o["rcSwitch"].toInt(0);
// // // //     d.rcCount     = o["rcCount"].toInt(1);
// // // //     d.takeoffAlt  = o["takeoffAlt"].toDouble(30.0);
// // // //     d.nearHomeAlt = o["nearHomeAlt"].toDouble(60.0);
// // // //     d.cruiseAlt   = o["cruiseAlt"].toDouble(120.0);
// // // //     d.landAlt     = o["landAlt"].toDouble(5.0);

// // // //     QJsonArray caps = o["capabilities"].toArray();
// // // //     for (const auto& v : caps)
// // // //         d.capabilities.append(v.toString());
// // // //     if (d.capabilities.isEmpty())
// // // //         d.capabilities = {"Delivery"};

// // // //     return d;
// // // // }

// // // // // ── DroneManager ──────────────────────────────────────────────────────────────

// // // // DroneManager::DroneManager(QObject* parent)
// // // //     : QAbstractListModel(parent)
// // // // {
// // // //     reload();
// // // // }

// // // // QString DroneManager::jsonPath() const
// // // // {
// // // //     return QCoreApplication::applicationDirPath() + "/drones.json";
// // // // }

// // // // void DroneManager::loadDefaults()
// // // // {
// // // //     // Built-in fleet — mirrors drone_models.h
// // // //     m_drones = {
// // // //         { "AIR_ORCA",   "Air-Orca",   true, 7500,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // // //         { "PELICAN",    "Pelican",    true, 4000,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // // //         { "UTTAM",      "Uttam",      true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // // //         { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // // //     };
// // // // }

// // // // void DroneManager::loadFromFile()
// // // // {
// // // //     QFile f(jsonPath());
// // // //     if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
// // // //         qDebug() << "DroneManager: no drones.json found, using defaults";
// // // //         loadDefaults();
// // // //         return;
// // // //     }

// // // //     QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
// // // //     f.close();

// // // //     if (!doc.isObject() || !doc.object().contains("drones")) {
// // // //         qWarning() << "DroneManager: invalid drones.json, using defaults";
// // // //         loadDefaults();
// // // //         return;
// // // //     }

// // // //     m_drones.clear();
// // // //     QJsonArray arr = doc.object()["drones"].toArray();
// // // //     for (const auto& v : arr)
// // // //         m_drones.append(DroneRecord::fromJson(v.toObject()));

// // // //     qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
// // // // }

// // // // void DroneManager::reload()
// // // // {
// // // //     beginResetModel();
// // // //     loadFromFile();
// // // //     endResetModel();
// // // //     emit countChanged();
// // // //     emit dronesChanged();
// // // // }

// // // // void DroneManager::save()
// // // // {
// // // //     QJsonArray arr;
// // // //     for (const auto& d : m_drones)
// // // //         arr.append(d.toJson());

// // // //     QJsonObject root;
// // // //     root["drones"] = arr;

// // // //     QFile f(jsonPath());
// // // //     if (!f.open(QIODevice::WriteOnly)) {
// // // //         qWarning() << "DroneManager: cannot write" << jsonPath();
// // // //         return;
// // // //     }
// // // //     f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
// // // //     f.close();
// // // //     qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
// // // // }

// // // // // ── QAbstractListModel ────────────────────────────────────────────────────────

// // // // int DroneManager::rowCount(const QModelIndex& parent) const
// // // // {
// // // //     if (parent.isValid()) return 0;
// // // //     return m_drones.size();
// // // // }

// // // // QHash<int, QByteArray> DroneManager::roleNames() const
// // // // {
// // // //     return {
// // // //         { IdRole,               "droneId"           },
// // // //         { NameRole,             "droneName"         },
// // // //         { ActiveRole,           "droneActive"       },
// // // //         { RangeKmRole,          "droneRangeKm"      },
// // // //         { SpeedMsRole,          "droneSpeedMs"      },
// // // //         { CapabilitiesRole,     "droneCapabilities" },
// // // //         { RcSwitchRole,         "droneRcSwitch"     },
// // // //         { RcCountRole,          "droneRcCount"      },
// // // //         { TakeoffAltRole,       "droneTakeoffAlt"   },
// // // //         { NearHomeAltRole,      "droneNearHomeAlt"  },
// // // //         { CruiseAltRole,        "droneCruiseAlt"    },
// // // //         { LandAltRole,          "droneLandAlt"      },
// // // //         { NeedsRcRole,          "droneNeedsRc"      },
// // // //         { SupportsDeliveryRole, "droneDelivery"     },
// // // //         { SupportsDropRole,     "droneDrop"         },
// // // //         { SupportsParadropRole, "droneParadrop"     },
// // // //     };
// // // // }

// // // // QVariant DroneManager::data(const QModelIndex& index, int role) const
// // // // {
// // // //     if (!index.isValid() || index.row() >= m_drones.size()) return {};
// // // //     const DroneRecord& d = m_drones[index.row()];

// // // //     switch (role) {
// // // //         case IdRole:               return d.id;
// // // //         case NameRole:             return d.displayName;
// // // //         case ActiveRole:           return d.active;
// // // //         case RangeKmRole:          return d.maxRangeM / 1000.0;
// // // //         case SpeedMsRole:          return d.maxSpeedMs;
// // // //         case CapabilitiesRole:     return d.capabilities;
// // // //         case RcSwitchRole:         return d.rcSwitch;
// // // //         case RcCountRole:          return d.rcCount;
// // // //         case TakeoffAltRole:       return d.takeoffAlt;
// // // //         case NearHomeAltRole:      return d.nearHomeAlt;
// // // //         case CruiseAltRole:        return d.cruiseAlt;
// // // //         case LandAltRole:          return d.landAlt;
// // // //         case NeedsRcRole:          return d.needsRcSwitch();
// // // //         case SupportsDeliveryRole: return d.supportsDelivery();
// // // //         case SupportsDropRole:     return d.supportsDrop();
// // // //         case SupportsParadropRole: return d.supportsParadrop();
// // // //         default: return {};
// // // //     }
// // // // }

// // // // // ── Invokables ────────────────────────────────────────────────────────────────

// // // // QVariantList DroneManager::activeDrones() const
// // // // {
// // // //     QVariantList list;
// // // //     for (const DroneRecord& d : m_drones) {
// // // //         if (!d.active) continue;
// // // //         QVariantMap m;
// // // //         m["id"]          = d.id;
// // // //         m["name"]        = d.displayName;
// // // //         m["maxOneWayKm"] = d.maxRangeM / 1000.0;
// // // //         m["maxSpeedMs"]  = d.maxSpeedMs;
// // // //         m["capabilities"]= d.capabilities;
// // // //         m["rcSwitch"]    = d.rcSwitch;
// // // //     m["rcCount"]     = d.rcCount;
// // // //         m["rcCount"]     = d.rcCount;
// // // //         m["takeoffAlt"]  = d.takeoffAlt;
// // // //         m["nearHomeAlt"] = d.nearHomeAlt;
// // // //         m["cruiseAlt"]   = d.cruiseAlt;
// // // //         m["landAlt"]     = d.landAlt;
// // // //         m["needsRc"]     = d.needsRcSwitch();
// // // //         m["delivery"]    = d.supportsDelivery();
// // // //         m["drop"]        = d.supportsDrop();
// // // //         m["paradrop"]    = d.supportsParadrop();
// // // //         list.append(m);
// // // //     }
// // // //     return list;
// // // // }

// // // // QVariantMap DroneManager::droneAt(int index) const
// // // // {
// // // //     if (index < 0 || index >= m_drones.size()) return {};
// // // //     const DroneRecord& d = m_drones[index];
// // // //     QVariantMap m;
// // // //     m["id"]          = d.id;
// // // //     m["name"]        = d.displayName;
// // // //     m["active"]      = d.active;
// // // //     m["rangeKm"]     = d.maxRangeM / 1000.0;
// // // //     m["speedMs"]     = d.maxSpeedMs;
// // // //     m["capabilities"]= d.capabilities;
// // // //     m["rcSwitch"]    = d.rcSwitch;
// // // //     m["rcCount"]     = d.rcCount;
// // // //     m["takeoffAlt"]  = d.takeoffAlt;
// // // //     m["nearHomeAlt"] = d.nearHomeAlt;
// // // //     m["cruiseAlt"]   = d.cruiseAlt;
// // // //     m["landAlt"]     = d.landAlt;
// // // //     m["needsRc"]     = d.needsRcSwitch();
// // // //     m["delivery"]    = d.supportsDelivery();
// // // //     m["drop"]        = d.supportsDrop();
// // // //     m["paradrop"]    = d.supportsParadrop();
// // // //     return m;
// // // // }

// // // // int DroneManager::addDrone(const QString& name,
// // // //                             double rangeKm, double speedMs,
// // // //                             bool delivery, bool drop, bool paradrop,
// // // //                             int rcSwitch, int rcCount,
// // // //                             double takeoffAlt, double nearHomeAlt,
// // // //                             double cruiseAlt,  double landAlt)
// // // // {
// // // //     DroneRecord d;
// // // //     d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
// // // //     d.displayName = name.trimmed();
// // // //     d.active      = true;
// // // //     d.maxRangeM   = rangeKm * 1000.0;
// // // //     d.maxSpeedMs  = speedMs;
// // // //     d.rcSwitch    = rcSwitch;
// // // //     d.rcCount     = rcCount;
// // // //     d.takeoffAlt  = takeoffAlt;
// // // //     d.nearHomeAlt = nearHomeAlt;
// // // //     d.cruiseAlt   = cruiseAlt;
// // // //     d.landAlt     = landAlt;
// // // //     if (delivery) d.capabilities.append("Delivery");
// // // //     if (drop)     d.capabilities.append("Drop");
// // // //     if (paradrop) d.capabilities.append("Paradrop");
// // // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // // //     int row = m_drones.size();
// // // //     beginInsertRows({}, row, row);
// // // //     m_drones.append(d);
// // // //     endInsertRows();

// // // //     save();
// // // //     emit countChanged();
// // // //     emit dronesChanged();
// // // //     return row;
// // // // }

// // // // void DroneManager::updateDrone(int index,
// // // //                                 const QString& name,
// // // //                                 double rangeKm, double speedMs,
// // // //                                 bool delivery, bool drop, bool paradrop,
// // // //                                 int rcSwitch, int rcCount,
// // // //                                 double takeoffAlt, double nearHomeAlt,
// // // //                                 double cruiseAlt,  double landAlt)
// // // // {
// // // //     if (index < 0 || index >= m_drones.size()) return;
// // // //     DroneRecord& d  = m_drones[index];
// // // //     d.displayName   = name.trimmed();
// // // //     d.maxRangeM     = rangeKm * 1000.0;
// // // //     d.maxSpeedMs    = speedMs;
// // // //     d.rcSwitch      = rcSwitch;
// // // //     d.rcCount       = rcCount;
// // // //     d.takeoffAlt    = takeoffAlt;
// // // //     d.nearHomeAlt   = nearHomeAlt;
// // // //     d.cruiseAlt     = cruiseAlt;
// // // //     d.landAlt       = landAlt;
// // // //     d.capabilities.clear();
// // // //     if (delivery) d.capabilities.append("Delivery");
// // // //     if (drop)     d.capabilities.append("Drop");
// // // //     if (paradrop) d.capabilities.append("Paradrop");
// // // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0));
// // // //     save();
// // // //     emit dronesChanged();
// // // // }

// // // // void DroneManager::setActive(int index, bool active)
// // // // {
// // // //     if (index < 0 || index >= m_drones.size()) return;
// // // //     m_drones[index].active = active;
// // // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
// // // //     save();
// // // //     emit dronesChanged();
// // // // }

// // // // void DroneManager::deactivate(int index)
// // // // {
// // // //     setActive(index, false);
// // // // }


// // // #include "DroneManager.h"
// // // #include <QCoreApplication>
// // // #include <QFile>
// // // #include <QJsonDocument>
// // // #include <QJsonArray>
// // // #include <QJsonObject>
// // // #include <QUuid>
// // // #include <QDebug>

// // // // ── DroneRecord ───────────────────────────────────────────────────────────────

// // // QJsonObject DroneRecord::toJson() const
// // // {
// // //     QJsonObject o;
// // //     o["id"]          = id;
// // //     o["displayName"] = displayName;
// // //     o["active"]      = active;
// // //     o["maxRangeM"]   = maxRangeM;
// // //     o["maxSpeedMs"]  = maxSpeedMs;
// // //     o["capabilities"]= QJsonArray::fromStringList(capabilities);
// // //     o["rcSwitch"]    = rcSwitch;
// // //     o["rcCount"]     = rcCount;
// // //     o["takeoffAlt"]  = takeoffAlt;
// // //     o["nearHomeAlt"] = nearHomeAlt;
// // //     o["cruiseAlt"]   = cruiseAlt;
// // //     o["landAlt"]     = landAlt;
// // //     return o;
// // // }

// // // DroneRecord DroneRecord::fromJson(const QJsonObject& o)
// // // {
// // //     DroneRecord d;
// // //     d.id          = o["id"].toString();
// // //     d.displayName = o["displayName"].toString();
// // //     d.active      = o["active"].toBool(true);
// // //     d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
// // //     d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
// // //     d.rcSwitch    = o["rcSwitch"].toInt(0);
// // //     d.rcCount     = o["rcCount"].toInt(1);
// // //     d.takeoffAlt  = o["takeoffAlt"].toDouble(30.0);
// // //     d.nearHomeAlt = o["nearHomeAlt"].toDouble(60.0);
// // //     d.cruiseAlt   = o["cruiseAlt"].toDouble(120.0);
// // //     d.landAlt     = o["landAlt"].toDouble(5.0);

// // //     QJsonArray caps = o["capabilities"].toArray();
// // //     for (const auto& v : caps)
// // //         d.capabilities.append(v.toString());
// // //     if (d.capabilities.isEmpty())
// // //         d.capabilities = {"Delivery"};

// // //     return d;
// // // }

// // // // ── DroneManager ──────────────────────────────────────────────────────────────

// // // DroneManager::DroneManager(QObject* parent)
// // //     : QAbstractListModel(parent)
// // // {
// // //     reload();
// // // }

// // // QString DroneManager::jsonPath() const
// // // {
// // //     return QCoreApplication::applicationDirPath() + "/drones.json";
// // // }

// // // void DroneManager::loadDefaults()
// // // {
// // //     // Built-in fleet — mirrors drone_models.h
// // //     m_drones = {
// // //         { "AIR_ORCA",   "Air-Orca",   true, 7500,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // //         { "PELICAN",    "Pelican",    true, 4000,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 5 },
// // //         { "UTTAM",      "Uttam",      true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // //         { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  3, 1, 40, 80,  150, 5 },
// // //     };
// // // }

// // // void DroneManager::loadFromFile()
// // // {
// // //     QFile f(jsonPath());
// // //     if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
// // //         qDebug() << "DroneManager: no drones.json found, using defaults";
// // //         loadDefaults();
// // //         return;
// // //     }

// // //     QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
// // //     f.close();

// // //     if (!doc.isObject() || !doc.object().contains("drones")) {
// // //         qWarning() << "DroneManager: invalid drones.json, using defaults";
// // //         loadDefaults();
// // //         return;
// // //     }

// // //     m_drones.clear();
// // //     QJsonArray arr = doc.object()["drones"].toArray();
// // //     for (const auto& v : arr)
// // //         m_drones.append(DroneRecord::fromJson(v.toObject()));

// // //     qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
// // // }

// // // void DroneManager::resetToDefaults()
// // // {
// // //     QFile::remove(jsonPath());
// // //     reload();
// // //     qDebug() << "DroneManager: reset to factory defaults";
// // // }

// // // void DroneManager::reload()
// // // {
// // //     beginResetModel();
// // //     loadFromFile();
// // //     endResetModel();
// // //     emit countChanged();
// // //     emit dronesChanged();
// // // }

// // // void DroneManager::save()
// // // {
// // //     QJsonArray arr;
// // //     for (const auto& d : m_drones)
// // //         arr.append(d.toJson());

// // //     QJsonObject root;
// // //     root["drones"] = arr;

// // //     QFile f(jsonPath());
// // //     if (!f.open(QIODevice::WriteOnly)) {
// // //         qWarning() << "DroneManager: cannot write" << jsonPath();
// // //         return;
// // //     }
// // //     f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
// // //     f.close();
// // //     qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
// // // }

// // // // ── QAbstractListModel ────────────────────────────────────────────────────────

// // // int DroneManager::rowCount(const QModelIndex& parent) const
// // // {
// // //     if (parent.isValid()) return 0;
// // //     return m_drones.size();
// // // }

// // // QHash<int, QByteArray> DroneManager::roleNames() const
// // // {
// // //     return {
// // //         { IdRole,               "droneId"           },
// // //         { NameRole,             "droneName"         },
// // //         { ActiveRole,           "droneActive"       },
// // //         { RangeKmRole,          "droneRangeKm"      },
// // //         { SpeedMsRole,          "droneSpeedMs"      },
// // //         { CapabilitiesRole,     "droneCapabilities" },
// // //         { RcSwitchRole,         "droneRcSwitch"     },
// // //         { RcCountRole,          "droneRcCount"      },
// // //         { TakeoffAltRole,       "droneTakeoffAlt"   },
// // //         { NearHomeAltRole,      "droneNearHomeAlt"  },
// // //         { CruiseAltRole,        "droneCruiseAlt"    },
// // //         { LandAltRole,          "droneLandAlt"      },
// // //         { NeedsRcRole,          "droneNeedsRc"      },
// // //         { SupportsDeliveryRole, "droneDelivery"     },
// // //         { SupportsDropRole,     "droneDrop"         },
// // //         { SupportsParadropRole, "droneParadrop"     },
// // //     };
// // // }

// // // QVariant DroneManager::data(const QModelIndex& index, int role) const
// // // {
// // //     if (!index.isValid() || index.row() >= m_drones.size()) return {};
// // //     const DroneRecord& d = m_drones[index.row()];

// // //     switch (role) {
// // //         case IdRole:               return d.id;
// // //         case NameRole:             return d.displayName;
// // //         case ActiveRole:           return d.active;
// // //         case RangeKmRole:          return d.maxRangeM / 1000.0;
// // //         case SpeedMsRole:          return d.maxSpeedMs;
// // //         case CapabilitiesRole:     return d.capabilities;
// // //         case RcSwitchRole:         return d.rcSwitch;
// // //         case RcCountRole:          return d.rcCount;
// // //         case TakeoffAltRole:       return d.takeoffAlt;
// // //         case NearHomeAltRole:      return d.nearHomeAlt;
// // //         case CruiseAltRole:        return d.cruiseAlt;
// // //         case LandAltRole:          return d.landAlt;
// // //         case NeedsRcRole:          return d.needsRcSwitch();
// // //         case SupportsDeliveryRole: return d.supportsDelivery();
// // //         case SupportsDropRole:     return d.supportsDrop();
// // //         case SupportsParadropRole: return d.supportsParadrop();
// // //         default: return {};
// // //     }
// // // }

// // // // ── Invokables ────────────────────────────────────────────────────────────────

// // // QVariantList DroneManager::activeDrones() const
// // // {
// // //     QVariantList list;
// // //     for (const DroneRecord& d : m_drones) {
// // //         if (!d.active) continue;
// // //         QVariantMap m;
// // //         m["id"]          = d.id;
// // //         m["name"]        = d.displayName;
// // //         m["maxOneWayKm"] = d.maxRangeM / 1000.0;
// // //         m["maxSpeedMs"]  = d.maxSpeedMs;
// // //         m["capabilities"]= d.capabilities;
// // //         m["rcSwitch"]    = d.rcSwitch;
// // //     m["rcCount"]     = d.rcCount;
// // //         m["rcCount"]     = d.rcCount;
// // //         m["takeoffAlt"]  = d.takeoffAlt;
// // //         m["nearHomeAlt"] = d.nearHomeAlt;
// // //         m["cruiseAlt"]   = d.cruiseAlt;
// // //         m["landAlt"]     = d.landAlt;
// // //         m["needsRc"]     = d.needsRcSwitch();
// // //         m["delivery"]    = d.supportsDelivery();
// // //         m["drop"]        = d.supportsDrop();
// // //         m["paradrop"]    = d.supportsParadrop();
// // //         list.append(m);
// // //     }
// // //     return list;
// // // }

// // // QVariantMap DroneManager::droneAt(int index) const
// // // {
// // //     if (index < 0 || index >= m_drones.size()) return {};
// // //     const DroneRecord& d = m_drones[index];
// // //     QVariantMap m;
// // //     m["id"]          = d.id;
// // //     m["name"]        = d.displayName;
// // //     m["active"]      = d.active;
// // //     m["rangeKm"]     = d.maxRangeM / 1000.0;
// // //     m["speedMs"]     = d.maxSpeedMs;
// // //     m["capabilities"]= d.capabilities;
// // //     m["rcSwitch"]    = d.rcSwitch;
// // //     m["rcCount"]     = d.rcCount;
// // //     m["takeoffAlt"]  = d.takeoffAlt;
// // //     m["nearHomeAlt"] = d.nearHomeAlt;
// // //     m["cruiseAlt"]   = d.cruiseAlt;
// // //     m["landAlt"]     = d.landAlt;
// // //     m["needsRc"]     = d.needsRcSwitch();
// // //     m["delivery"]    = d.supportsDelivery();
// // //     m["drop"]        = d.supportsDrop();
// // //     m["paradrop"]    = d.supportsParadrop();
// // //     return m;
// // // }

// // // int DroneManager::addDrone(const QString& name,
// // //                             double rangeKm, double speedMs,
// // //                             bool delivery, bool drop, bool paradrop,
// // //                             int rcSwitch, int rcCount,
// // //                             double takeoffAlt, double nearHomeAlt,
// // //                             double cruiseAlt,  double landAlt)
// // // {
// // //     DroneRecord d;
// // //     d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
// // //     d.displayName = name.trimmed();
// // //     d.active      = true;
// // //     d.maxRangeM   = rangeKm * 1000.0;
// // //     d.maxSpeedMs  = speedMs;
// // //     d.rcSwitch    = rcSwitch;
// // //     d.rcCount     = rcCount;
// // //     d.takeoffAlt  = takeoffAlt;
// // //     d.nearHomeAlt = nearHomeAlt;
// // //     d.cruiseAlt   = cruiseAlt;
// // //     d.landAlt     = landAlt;
// // //     if (delivery) d.capabilities.append("Delivery");
// // //     if (drop)     d.capabilities.append("Drop");
// // //     if (paradrop) d.capabilities.append("Paradrop");
// // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // //     int row = m_drones.size();
// // //     beginInsertRows({}, row, row);
// // //     m_drones.append(d);
// // //     endInsertRows();

// // //     save();
// // //     emit countChanged();
// // //     emit dronesChanged();
// // //     return row;
// // // }

// // // void DroneManager::updateDrone(int index,
// // //                                 const QString& name,
// // //                                 double rangeKm, double speedMs,
// // //                                 bool delivery, bool drop, bool paradrop,
// // //                                 int rcSwitch, int rcCount,
// // //                                 double takeoffAlt, double nearHomeAlt,
// // //                                 double cruiseAlt,  double landAlt)
// // // {
// // //     if (index < 0 || index >= m_drones.size()) return;
// // //     DroneRecord& d  = m_drones[index];
// // //     d.displayName   = name.trimmed();
// // //     d.maxRangeM     = rangeKm * 1000.0;
// // //     d.maxSpeedMs    = speedMs;
// // //     d.rcSwitch      = rcSwitch;
// // //     d.rcCount       = rcCount;
// // //     d.takeoffAlt    = takeoffAlt;
// // //     d.nearHomeAlt   = nearHomeAlt;
// // //     d.cruiseAlt     = cruiseAlt;
// // //     d.landAlt       = landAlt;
// // //     d.capabilities.clear();
// // //     if (delivery) d.capabilities.append("Delivery");
// // //     if (drop)     d.capabilities.append("Drop");
// // //     if (paradrop) d.capabilities.append("Paradrop");
// // //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0));
// // //     save();
// // //     emit dronesChanged();
// // // }

// // // void DroneManager::setActive(int index, bool active)
// // // {
// // //     if (index < 0 || index >= m_drones.size()) return;
// // //     m_drones[index].active = active;
// // //     emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
// // //     save();
// // //     emit dronesChanged();
// // // }

// // // void DroneManager::deactivate(int index)
// // // {
// // //     setActive(index, false);
// // // }


// // #include "DroneManager.h"
// // #include <QCoreApplication>
// // #include <QFile>
// // #include <QJsonDocument>
// // #include <QJsonArray>
// // #include <QJsonObject>
// // #include <QUuid>
// // #include <QDebug>

// // // ── DroneRecord ───────────────────────────────────────────────────────────────

// // QJsonObject DroneRecord::toJson() const
// // {
// //     QJsonObject o;
// //     o["id"]          = id;
// //     o["displayName"] = displayName;
// //     o["active"]      = active;
// //     o["maxRangeM"]   = maxRangeM;
// //     o["maxSpeedMs"]  = maxSpeedMs;
// //     o["capabilities"]= QJsonArray::fromStringList(capabilities);
// //     o["rcSwitch"]    = rcSwitch;
// //     o["rcCount"]     = rcCount;
// //     o["takeoffAlt"]  = takeoffAlt;
// //     o["nearHomeAlt"] = nearHomeAlt;
// //     o["cruiseAlt"]   = cruiseAlt;
// //     o["landAlt"]     = landAlt;
// //     return o;
// // }

// // DroneRecord DroneRecord::fromJson(const QJsonObject& o)
// // {
// //     DroneRecord d;
// //     d.id          = o["id"].toString();
// //     d.displayName = o["displayName"].toString();
// //     d.active      = o["active"].toBool(true);
// //     d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
// //     d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
// //     d.rcSwitch    = o["rcSwitch"].toInt(0);
// //     d.rcCount     = o["rcCount"].toInt(1);
// //     d.takeoffAlt  = o["takeoffAlt"].toDouble(30.0);
// //     d.nearHomeAlt = o["nearHomeAlt"].toDouble(60.0);
// //     d.cruiseAlt   = o["cruiseAlt"].toDouble(120.0);
// //     d.landAlt     = o["landAlt"].toDouble(5.0);

// //     QJsonArray caps = o["capabilities"].toArray();
// //     for (const auto& v : caps)
// //         d.capabilities.append(v.toString());
// //     if (d.capabilities.isEmpty())
// //         d.capabilities = {"Delivery"};

// //     return d;
// // }

// // // ── DroneManager ──────────────────────────────────────────────────────────────

// // DroneManager::DroneManager(QObject* parent)
// //     : QAbstractListModel(parent)
// // {
// //     reload();
// // }

// // QString DroneManager::jsonPath() const
// // {
// //     return QCoreApplication::applicationDirPath() + "/drones.json";
// // }

// // void DroneManager::loadDefaults()
// // {
// //     // Built-in fleet — mirrors drone_models.h
// //     m_drones = {
// //         { "AIR_ORCA",   "Air-Orca",   true, 7500,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 80 },
// //         { "PELICAN",    "Pelican",    true, 4000,  5.0, {"Delivery"},                    0, 0, 30, 60,  120, 80 },
// //         { "UTTAM",      "Uttam",      true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 80,  150, 80 },
// //         { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 7.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 80,  150, 80 },
// //     };
// // }

// // void DroneManager::loadFromFile()
// // {
// //     QFile f(jsonPath());
// //     if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
// //         qDebug() << "DroneManager: no drones.json found, using defaults";
// //         loadDefaults();
// //         return;
// //     }

// //     QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
// //     f.close();

// //     if (!doc.isObject() || !doc.object().contains("drones")) {
// //         qWarning() << "DroneManager: invalid drones.json, using defaults";
// //         loadDefaults();
// //         return;
// //     }

// //     m_drones.clear();
// //     QJsonArray arr = doc.object()["drones"].toArray();
// //     for (const auto& v : arr)
// //         m_drones.append(DroneRecord::fromJson(v.toObject()));

// //     qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
// // }

// // void DroneManager::resetToDefaults()
// // {
// //     QFile::remove(jsonPath());
// //     reload();
// //     qDebug() << "DroneManager: reset to factory defaults";
// // }

// // void DroneManager::reload()
// // {
// //     beginResetModel();
// //     loadFromFile();
// //     endResetModel();
// //     emit countChanged();
// //     emit dronesChanged();
// // }

// // void DroneManager::save()
// // {
// //     QJsonArray arr;
// //     for (const auto& d : m_drones)
// //         arr.append(d.toJson());

// //     QJsonObject root;
// //     root["drones"] = arr;

// //     QFile f(jsonPath());
// //     if (!f.open(QIODevice::WriteOnly)) {
// //         qWarning() << "DroneManager: cannot write" << jsonPath();
// //         return;
// //     }
// //     f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
// //     f.close();
// //     qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
// // }

// // // ── QAbstractListModel ────────────────────────────────────────────────────────

// // int DroneManager::rowCount(const QModelIndex& parent) const
// // {
// //     if (parent.isValid()) return 0;
// //     return m_drones.size();
// // }

// // QHash<int, QByteArray> DroneManager::roleNames() const
// // {
// //     return {
// //         { IdRole,               "droneId"           },
// //         { NameRole,             "droneName"         },
// //         { ActiveRole,           "droneActive"       },
// //         { RangeKmRole,          "droneRangeKm"      },
// //         { SpeedMsRole,          "droneSpeedMs"      },
// //         { CapabilitiesRole,     "droneCapabilities" },
// //         { RcSwitchRole,         "droneRcSwitch"     },
// //         { RcCountRole,          "droneRcCount"      },
// //         { TakeoffAltRole,       "droneTakeoffAlt"   },
// //         { NearHomeAltRole,      "droneNearHomeAlt"  },
// //         { CruiseAltRole,        "droneCruiseAlt"    },
// //         { LandAltRole,          "droneLandAlt"      },
// //         { NeedsRcRole,          "droneNeedsRc"      },
// //         { SupportsDeliveryRole, "droneDelivery"     },
// //         { SupportsDropRole,     "droneDrop"         },
// //         { SupportsParadropRole, "droneParadrop"     },
// //     };
// // }

// // QVariant DroneManager::data(const QModelIndex& index, int role) const
// // {
// //     if (!index.isValid() || index.row() >= m_drones.size()) return {};
// //     const DroneRecord& d = m_drones[index.row()];

// //     switch (role) {
// //         case IdRole:               return d.id;
// //         case NameRole:             return d.displayName;
// //         case ActiveRole:           return d.active;
// //         case RangeKmRole:          return d.maxRangeM / 1000.0;
// //         case SpeedMsRole:          return d.maxSpeedMs;
// //         case CapabilitiesRole:     return d.capabilities;
// //         case RcSwitchRole:         return d.rcSwitch;
// //         case RcCountRole:          return d.rcCount;
// //         case TakeoffAltRole:       return d.takeoffAlt;
// //         case NearHomeAltRole:      return d.nearHomeAlt;
// //         case CruiseAltRole:        return d.cruiseAlt;
// //         case LandAltRole:          return d.landAlt;
// //         case NeedsRcRole:          return d.needsRcSwitch();
// //         case SupportsDeliveryRole: return d.supportsDelivery();
// //         case SupportsDropRole:     return d.supportsDrop();
// //         case SupportsParadropRole: return d.supportsParadrop();
// //         default: return {};
// //     }
// // }

// // // ── Invokables ────────────────────────────────────────────────────────────────

// // QVariantList DroneManager::activeDrones() const
// // {
// //     QVariantList list;
// //     for (const DroneRecord& d : m_drones) {
// //         if (!d.active) continue;
// //         QVariantMap m;
// //         m["id"]          = d.id;
// //         m["name"]        = d.displayName;
// //         m["maxOneWayKm"] = d.maxRangeM / 1000.0;
// //         m["maxSpeedMs"]  = d.maxSpeedMs;
// //         m["capabilities"]= d.capabilities;
// //         m["rcSwitch"]    = d.rcSwitch;
// //     m["rcCount"]     = d.rcCount;
// //         m["rcCount"]     = d.rcCount;
// //         m["takeoffAlt"]  = d.takeoffAlt;
// //         m["nearHomeAlt"] = d.nearHomeAlt;
// //         m["cruiseAlt"]   = d.cruiseAlt;
// //         m["landAlt"]     = d.landAlt;
// //         m["needsRc"]     = d.needsRcSwitch();
// //         m["delivery"]    = d.supportsDelivery();
// //         m["drop"]        = d.supportsDrop();
// //         m["paradrop"]    = d.supportsParadrop();
// //         list.append(m);
// //     }
// //     return list;
// // }

// // QVariantMap DroneManager::droneAt(int index) const
// // {
// //     if (index < 0 || index >= m_drones.size()) return {};
// //     const DroneRecord& d = m_drones[index];
// //     QVariantMap m;
// //     m["id"]          = d.id;
// //     m["name"]        = d.displayName;
// //     m["active"]      = d.active;
// //     m["rangeKm"]     = d.maxRangeM / 1000.0;
// //     m["speedMs"]     = d.maxSpeedMs;
// //     m["capabilities"]= d.capabilities;
// //     m["rcSwitch"]    = d.rcSwitch;
// //     m["rcCount"]     = d.rcCount;
// //     m["takeoffAlt"]  = d.takeoffAlt;
// //     m["nearHomeAlt"] = d.nearHomeAlt;
// //     m["cruiseAlt"]   = d.cruiseAlt;
// //     m["landAlt"]     = d.landAlt;
// //     m["needsRc"]     = d.needsRcSwitch();
// //     m["delivery"]    = d.supportsDelivery();
// //     m["drop"]        = d.supportsDrop();
// //     m["paradrop"]    = d.supportsParadrop();
// //     return m;
// // }

// // int DroneManager::addDrone(const QString& name,
// //                             double rangeKm, double speedMs,
// //                             bool delivery, bool drop, bool paradrop,
// //                             int rcSwitch, int rcCount,
// //                             double takeoffAlt, double nearHomeAlt,
// //                             double cruiseAlt,  double landAlt)
// // {
// //     DroneRecord d;
// //     d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
// //     d.displayName = name.trimmed();
// //     d.active      = true;
// //     d.maxRangeM   = rangeKm * 1000.0;
// //     d.maxSpeedMs  = speedMs;
// //     d.rcSwitch    = rcSwitch;
// //     d.rcCount     = rcCount;
// //     d.takeoffAlt  = takeoffAlt;
// //     d.nearHomeAlt = nearHomeAlt;
// //     d.cruiseAlt   = cruiseAlt;
// //     d.landAlt     = landAlt;
// //     if (delivery) d.capabilities.append("Delivery");
// //     if (drop)     d.capabilities.append("Drop");
// //     if (paradrop) d.capabilities.append("Paradrop");
// //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// //     int row = m_drones.size();
// //     beginInsertRows({}, row, row);
// //     m_drones.append(d);
// //     endInsertRows();

// //     save();
// //     emit countChanged();
// //     emit dronesChanged();
// //     return row;
// // }

// // void DroneManager::updateDrone(int index,
// //                                 const QString& name,
// //                                 double rangeKm, double speedMs,
// //                                 bool delivery, bool drop, bool paradrop,
// //                                 int rcSwitch, int rcCount,
// //                                 double takeoffAlt, double nearHomeAlt,
// //                                 double cruiseAlt,  double landAlt)
// // {
// //     if (index < 0 || index >= m_drones.size()) return;
// //     DroneRecord& d  = m_drones[index];
// //     d.displayName   = name.trimmed();
// //     d.maxRangeM     = rangeKm * 1000.0;
// //     d.maxSpeedMs    = speedMs;
// //     d.rcSwitch      = rcSwitch;
// //     d.rcCount       = rcCount;
// //     d.takeoffAlt    = takeoffAlt;
// //     d.nearHomeAlt   = nearHomeAlt;
// //     d.cruiseAlt     = cruiseAlt;
// //     d.landAlt       = landAlt;
// //     d.capabilities.clear();
// //     if (delivery) d.capabilities.append("Delivery");
// //     if (drop)     d.capabilities.append("Drop");
// //     if (paradrop) d.capabilities.append("Paradrop");
// //     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

// //     emit dataChanged(createIndex(index, 0), createIndex(index, 0));
// //     save();
// //     emit dronesChanged();
// // }

// // void DroneManager::setActive(int index, bool active)
// // {
// //     if (index < 0 || index >= m_drones.size()) return;
// //     m_drones[index].active = active;
// //     emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
// //     save();
// //     emit dronesChanged();
// // }

// // void DroneManager::deactivate(int index)
// // {
// //     setActive(index, false);
// // }


// #include "DroneManager.h"
// #include <QCoreApplication>
// #include <QFile>
// #include <QJsonDocument>
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QUuid>
// #include <QDebug>

// // ── DroneRecord ───────────────────────────────────────────────────────────────

// QJsonObject DroneRecord::toJson() const
// {
//     QJsonObject o;
//     o["id"]          = id;
//     o["displayName"] = displayName;
//     o["active"]      = active;
//     o["maxRangeM"]   = maxRangeM;
//     o["maxSpeedMs"]  = maxSpeedMs;
//     o["capabilities"]= QJsonArray::fromStringList(capabilities);
//     o["rcSwitch"]       = rcSwitch;
//     o["rcCount"]        = rcCount;
//     o["minSpeedMs"]     = minSpeedMs;
//     o["takeoffAlt"]     = takeoffAlt;
//     o["minTakeoffAlt"]  = minTakeoffAlt;
//     o["maxTakeoffAlt"]  = maxTakeoffAlt;
//     o["nearHomeAlt"]    = nearHomeAlt;
//     o["minNearHomeAlt"] = minNearHomeAlt;
//     o["maxNearHomeAlt"] = maxNearHomeAlt;
//     o["cruiseAlt"]      = cruiseAlt;
//     o["minCruiseAlt"]   = minCruiseAlt;
//     o["maxCruiseAlt"]   = maxCruiseAlt;
//     o["landAlt"]        = landAlt;
//     o["minLandAlt"]     = minLandAlt;
//     o["maxLandAlt"]     = maxLandAlt;
//     return o;
// }

// DroneRecord DroneRecord::fromJson(const QJsonObject& o)
// {
//     DroneRecord d;
//     d.id          = o["id"].toString();
//     d.displayName = o["displayName"].toString();
//     d.active      = o["active"].toBool(true);
//     d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
//     d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
//     d.rcSwitch       = o["rcSwitch"].toInt(0);
//     d.rcCount        = o["rcCount"].toInt(1);
//     d.minSpeedMs     = o["minSpeedMs"].toDouble(2.0);
//     d.takeoffAlt     = o["takeoffAlt"].toDouble(30.0);
//     d.minTakeoffAlt  = o["minTakeoffAlt"].toDouble(10.0);
//     d.maxTakeoffAlt  = o["maxTakeoffAlt"].toDouble(100.0);
//     d.nearHomeAlt    = o["nearHomeAlt"].toDouble(60.0);
//     d.minNearHomeAlt = o["minNearHomeAlt"].toDouble(20.0);
//     d.maxNearHomeAlt = o["maxNearHomeAlt"].toDouble(200.0);
//     d.cruiseAlt      = o["cruiseAlt"].toDouble(120.0);
//     d.minCruiseAlt   = o["minCruiseAlt"].toDouble(80.0);
//     d.maxCruiseAlt   = o["maxCruiseAlt"].toDouble(500.0);
//     d.landAlt        = o["landAlt"].toDouble(80.0);
//     d.minLandAlt     = o["minLandAlt"].toDouble(80.0);
//     d.maxLandAlt     = o["maxLandAlt"].toDouble(150.0);

//     QJsonArray caps = o["capabilities"].toArray();
//     for (const auto& v : caps)
//         d.capabilities.append(v.toString());
//     if (d.capabilities.isEmpty())
//         d.capabilities = {"Delivery"};

//     return d;
// }

// // ── DroneManager ──────────────────────────────────────────────────────────────

// DroneManager::DroneManager(QObject* parent)
//     : QAbstractListModel(parent)
// {
//     reload();
// }

// QString DroneManager::jsonPath() const
// {
//     return QCoreApplication::applicationDirPath() + "/drones.json";
// }

// void DroneManager::loadDefaults()
// {
//     // Built-in fleet — mirrors drone_models.h
//     m_drones = {
//         { "AIR_ORCA",   "Air-Orca",   true, 7500, 2.0, 8.0, {"Delivery"},                    0, 0, 30, 10, 80, 60, 20, 150, 120, 80, 300, 80, 80, 150 },
//         { "PELICAN",    "Pelican",    true, 4000, 2.0, 8.0, {"Delivery"},                    0, 0, 30, 10, 80, 60, 20, 150, 120, 80, 300, 80, 80, 150 },
//         { "UTTAM",      "Uttam",      true, 20000, 3.0, 12.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 20, 100, 80, 40, 200, 150, 100, 500, 80, 80, 150 },
//         { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 3.0, 12.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 20, 100, 80, 40, 200, 150, 100, 500, 80, 80, 150 },
//     };
// }

// void DroneManager::loadFromFile()
// {
//     QFile f(jsonPath());
//     if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
//         qDebug() << "DroneManager: no drones.json found, using defaults";
//         loadDefaults();
//         return;
//     }

//     QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
//     f.close();

//     if (!doc.isObject() || !doc.object().contains("drones")) {
//         qWarning() << "DroneManager: invalid drones.json, using defaults";
//         loadDefaults();
//         return;
//     }

//     m_drones.clear();
//     QJsonArray arr = doc.object()["drones"].toArray();
//     for (const auto& v : arr)
//         m_drones.append(DroneRecord::fromJson(v.toObject()));

//     qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
// }

// void DroneManager::resetToDefaults()
// {
//     QFile::remove(jsonPath());
//     reload();
//     qDebug() << "DroneManager: reset to factory defaults";
// }

// void DroneManager::reload()
// {
//     beginResetModel();
//     loadFromFile();
//     endResetModel();
//     emit countChanged();
//     emit dronesChanged();
// }

// void DroneManager::save()
// {
//     QJsonArray arr;
//     for (const auto& d : m_drones)
//         arr.append(d.toJson());

//     QJsonObject root;
//     root["drones"] = arr;

//     QFile f(jsonPath());
//     if (!f.open(QIODevice::WriteOnly)) {
//         qWarning() << "DroneManager: cannot write" << jsonPath();
//         return;
//     }
//     f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
//     f.close();
//     qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
// }

// // ── QAbstractListModel ────────────────────────────────────────────────────────

// int DroneManager::rowCount(const QModelIndex& parent) const
// {
//     if (parent.isValid()) return 0;
//     return m_drones.size();
// }

// QHash<int, QByteArray> DroneManager::roleNames() const
// {
//     return {
//         { IdRole,               "droneId"           },
//         { NameRole,             "droneName"         },
//         { ActiveRole,           "droneActive"       },
//         { RangeKmRole,          "droneRangeKm"      },
//         { SpeedMsRole,          "droneSpeedMs"      },
//         { CapabilitiesRole,     "droneCapabilities" },
//         { RcSwitchRole,         "droneRcSwitch"     },
//         { RcCountRole,          "droneRcCount"      },
//         { MinSpeedRole,         "droneMinSpeedMs"   },
//         { MinTakeoffAltRole,    "droneMinTakeoffAlt"},
//         { MaxTakeoffAltRole,    "droneMaxTakeoffAlt"},
//         { MinNearHomeAltRole,   "droneMinNearHomeAlt"},
//         { MaxNearHomeAltRole,   "droneMaxNearHomeAlt"},
//         { MinCruiseAltRole,     "droneMinCruiseAlt" },
//         { MaxCruiseAltRole,     "droneMaxCruiseAlt" },
//         { MinLandAltRole,       "droneMinLandAlt"   },
//         { MaxLandAltRole,       "droneMaxLandAlt"   },
//         { TakeoffAltRole,       "droneTakeoffAlt"   },
//         { NearHomeAltRole,      "droneNearHomeAlt"  },
//         { CruiseAltRole,        "droneCruiseAlt"    },
//         { LandAltRole,          "droneLandAlt"      },
//         { NeedsRcRole,          "droneNeedsRc"      },
//         { SupportsDeliveryRole, "droneDelivery"     },
//         { SupportsDropRole,     "droneDrop"         },
//         { SupportsParadropRole, "droneParadrop"     },
//     };
// }

// QVariant DroneManager::data(const QModelIndex& index, int role) const
// {
//     if (!index.isValid() || index.row() >= m_drones.size()) return {};
//     const DroneRecord& d = m_drones[index.row()];

//     switch (role) {
//         case IdRole:               return d.id;
//         case NameRole:             return d.displayName;
//         case ActiveRole:           return d.active;
//         case RangeKmRole:          return d.maxRangeM / 1000.0;
//         case SpeedMsRole:          return d.maxSpeedMs;
//         case CapabilitiesRole:     return d.capabilities;
//         case RcSwitchRole:         return d.rcSwitch;
//         case RcCountRole:          return d.rcCount;
//         case MinSpeedRole:         return d.minSpeedMs;
//         case MinTakeoffAltRole:    return d.minTakeoffAlt;
//         case MaxTakeoffAltRole:    return d.maxTakeoffAlt;
//         case MinNearHomeAltRole:   return d.minNearHomeAlt;
//         case MaxNearHomeAltRole:   return d.maxNearHomeAlt;
//         case MinCruiseAltRole:     return d.minCruiseAlt;
//         case MaxCruiseAltRole:     return d.maxCruiseAlt;
//         case MinLandAltRole:       return d.minLandAlt;
//         case MaxLandAltRole:       return d.maxLandAlt;
//         case TakeoffAltRole:       return d.takeoffAlt;
//         case NearHomeAltRole:      return d.nearHomeAlt;
//         case CruiseAltRole:        return d.cruiseAlt;
//         case LandAltRole:          return d.landAlt;
//         case NeedsRcRole:          return d.needsRcSwitch();
//         case SupportsDeliveryRole: return d.supportsDelivery();
//         case SupportsDropRole:     return d.supportsDrop();
//         case SupportsParadropRole: return d.supportsParadrop();
//         default: return {};
//     }
// }

// // ── Invokables ────────────────────────────────────────────────────────────────

// QVariantList DroneManager::activeDrones() const
// {
//     QVariantList list;
//     for (const DroneRecord& d : m_drones) {
//         if (!d.active) continue;
//         QVariantMap m;
//         m["id"]          = d.id;
//         m["name"]        = d.displayName;
//         m["maxOneWayKm"] = d.maxRangeM / 1000.0;
//         m["maxSpeedMs"]  = d.maxSpeedMs;
//         m["capabilities"]= d.capabilities;
//         m["rcSwitch"]    = d.rcSwitch;
//     m["rcCount"]     = d.rcCount;
//         m["rcCount"]     = d.rcCount;
//         m["takeoffAlt"]  = d.takeoffAlt;
//         m["nearHomeAlt"] = d.nearHomeAlt;
//         m["cruiseAlt"]   = d.cruiseAlt;
//         m["landAlt"]     = d.landAlt;
//         m["needsRc"]     = d.needsRcSwitch();
//         m["delivery"]    = d.supportsDelivery();
//         m["drop"]        = d.supportsDrop();
//         m["paradrop"]    = d.supportsParadrop();
//         list.append(m);
//     }
//     return list;
// }

// QVariantMap DroneManager::droneAt(int index) const
// {
//     if (index < 0 || index >= m_drones.size()) return {};
//     const DroneRecord& d = m_drones[index];
//     QVariantMap m;
//     m["id"]          = d.id;
//     m["name"]        = d.displayName;
//     m["active"]      = d.active;
//     m["rangeKm"]     = d.maxRangeM / 1000.0;
//     m["speedMs"]     = d.maxSpeedMs;
//     m["capabilities"]= d.capabilities;
//     m["rcSwitch"]    = d.rcSwitch;
//     m["rcCount"]     = d.rcCount;
//     m["takeoffAlt"]  = d.takeoffAlt;
//     m["nearHomeAlt"] = d.nearHomeAlt;
//     m["cruiseAlt"]   = d.cruiseAlt;
//     m["landAlt"]     = d.landAlt;
//     m["needsRc"]     = d.needsRcSwitch();
//     m["delivery"]    = d.supportsDelivery();
//     m["drop"]        = d.supportsDrop();
//     m["paradrop"]    = d.supportsParadrop();
//     return m;
// }

// int DroneManager::addDrone(const QString& name,
//                             double rangeKm, double minSpeedMs, double maxSpeedMs,
//                             bool delivery, bool drop, bool paradrop,
//                             int rcSwitch, int rcCount,
//                             double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
//                             double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
//                             double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
//                             double landAlt, double minLandAlt, double maxLandAlt)
// {
//     DroneRecord d;
//     d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
//     d.displayName = name.trimmed();
//     d.active      = true;
//     d.maxRangeM   = rangeKm * 1000.0;
//     d.maxSpeedMs  = speedMs;
//     d.rcSwitch       = rcSwitch;
//     d.rcCount        = rcCount;
//     d.minSpeedMs     = minSpeedMs;
//     d.takeoffAlt     = takeoffAlt;
//     d.minTakeoffAlt  = minTakeoffAlt;
//     d.maxTakeoffAlt  = maxTakeoffAlt;
//     d.nearHomeAlt    = nearHomeAlt;
//     d.minNearHomeAlt = minNearHomeAlt;
//     d.maxNearHomeAlt = maxNearHomeAlt;
//     d.cruiseAlt      = cruiseAlt;
//     d.minCruiseAlt   = minCruiseAlt;
//     d.maxCruiseAlt   = maxCruiseAlt;
//     d.landAlt        = landAlt;
//     d.minLandAlt     = minLandAlt;
//     d.maxLandAlt     = maxLandAlt;
//     if (delivery) d.capabilities.append("Delivery");
//     if (drop)     d.capabilities.append("Drop");
//     if (paradrop) d.capabilities.append("Paradrop");
//     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

//     int row = m_drones.size();
//     beginInsertRows({}, row, row);
//     m_drones.append(d);
//     endInsertRows();

//     save();
//     emit countChanged();
//     emit dronesChanged();
//     return row;
// }

// void DroneManager::updateDrone(int index,
//                                 const QString& name,
//                                 double rangeKm, double minSpeedMs, double maxSpeedMs,
//                                 bool delivery, bool drop, bool paradrop,
//                                 int rcSwitch, int rcCount,
//                                 double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
//                                 double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
//                                 double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
//                                 double landAlt, double minLandAlt, double maxLandAlt)
// {
//     if (index < 0 || index >= m_drones.size()) return;
//     DroneRecord& d  = m_drones[index];
//     d.displayName   = name.trimmed();
//     d.maxRangeM     = rangeKm * 1000.0;
//     d.maxSpeedMs    = speedMs;
//     d.rcSwitch       = rcSwitch;
//     d.rcCount        = rcCount;
//     d.minSpeedMs     = minSpeedMs;
//     d.takeoffAlt     = takeoffAlt;
//     d.minTakeoffAlt  = minTakeoffAlt;
//     d.maxTakeoffAlt  = maxTakeoffAlt;
//     d.nearHomeAlt    = nearHomeAlt;
//     d.minNearHomeAlt = minNearHomeAlt;
//     d.maxNearHomeAlt = maxNearHomeAlt;
//     d.cruiseAlt      = cruiseAlt;
//     d.minCruiseAlt   = minCruiseAlt;
//     d.maxCruiseAlt   = maxCruiseAlt;
//     d.landAlt        = landAlt;
//     d.minLandAlt     = minLandAlt;
//     d.maxLandAlt     = maxLandAlt;
//     d.capabilities.clear();
//     if (delivery) d.capabilities.append("Delivery");
//     if (drop)     d.capabilities.append("Drop");
//     if (paradrop) d.capabilities.append("Paradrop");
//     if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

//     emit dataChanged(createIndex(index, 0), createIndex(index, 0));
//     save();
//     emit dronesChanged();
// }

// void DroneManager::setActive(int index, bool active)
// {
//     if (index < 0 || index >= m_drones.size()) return;
//     m_drones[index].active = active;
//     emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
//     save();
//     emit dronesChanged();
// }

// void DroneManager::deactivate(int index)
// {
//     setActive(index, false);
// }


#include "DroneManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

// ── DroneRecord ───────────────────────────────────────────────────────────────

QJsonObject DroneRecord::toJson() const
{
    QJsonObject o;
    o["id"]          = id;
    o["displayName"] = displayName;
    o["active"]      = active;
    o["maxRangeM"]   = maxRangeM;
    o["maxSpeedMs"]  = maxSpeedMs;
    o["capabilities"]= QJsonArray::fromStringList(capabilities);
    o["rcSwitch"]       = rcSwitch;
    o["rcCount"]        = rcCount;
    o["minSpeedMs"]     = minSpeedMs;
    o["takeoffAlt"]     = takeoffAlt;
    o["minTakeoffAlt"]  = minTakeoffAlt;
    o["maxTakeoffAlt"]  = maxTakeoffAlt;
    o["nearHomeAlt"]    = nearHomeAlt;
    o["minNearHomeAlt"] = minNearHomeAlt;
    o["maxNearHomeAlt"] = maxNearHomeAlt;
    o["cruiseAlt"]      = cruiseAlt;
    o["minCruiseAlt"]   = minCruiseAlt;
    o["maxCruiseAlt"]   = maxCruiseAlt;
    o["landAlt"]        = landAlt;
    o["minLandAlt"]     = minLandAlt;
    o["maxLandAlt"]     = maxLandAlt;
    return o;
}

DroneRecord DroneRecord::fromJson(const QJsonObject& o)
{
    DroneRecord d;
    d.id          = o["id"].toString();
    d.displayName = o["displayName"].toString();
    d.active      = o["active"].toBool(true);
    d.maxRangeM   = o["maxRangeM"].toDouble(5000.0);
    d.maxSpeedMs  = o["maxSpeedMs"].toDouble(5.0);
    d.rcSwitch       = o["rcSwitch"].toInt(0);
    d.rcCount        = o["rcCount"].toInt(1);
    d.minSpeedMs     = o["minSpeedMs"].toDouble(2.0);
    d.takeoffAlt     = o["takeoffAlt"].toDouble(30.0);
    d.minTakeoffAlt  = o["minTakeoffAlt"].toDouble(10.0);
    d.maxTakeoffAlt  = o["maxTakeoffAlt"].toDouble(100.0);
    d.nearHomeAlt    = o["nearHomeAlt"].toDouble(60.0);
    d.minNearHomeAlt = o["minNearHomeAlt"].toDouble(20.0);
    d.maxNearHomeAlt = o["maxNearHomeAlt"].toDouble(200.0);
    d.cruiseAlt      = o["cruiseAlt"].toDouble(120.0);
    d.minCruiseAlt   = o["minCruiseAlt"].toDouble(80.0);
    d.maxCruiseAlt   = o["maxCruiseAlt"].toDouble(500.0);
    d.landAlt        = o["landAlt"].toDouble(80.0);
    d.minLandAlt     = o["minLandAlt"].toDouble(80.0);
    d.maxLandAlt     = o["maxLandAlt"].toDouble(150.0);

    auto fixRange = [](double& minValue, double& maxValue,
                       double fallbackMin, double fallbackMax) {
        if (minValue <= 0.0)
            minValue = fallbackMin;
        if (maxValue <= 0.0)
            maxValue = fallbackMax;
        if (minValue > maxValue) {
            double tmp = minValue;
            minValue = maxValue;
            maxValue = tmp;
        }
    };
    auto clampValue = [](double& value, double minValue, double maxValue,
                         double fallbackValue) {
        if (value <= 0.0)
            value = fallbackValue;
        if (value < minValue)
            value = minValue;
        if (value > maxValue)
            value = maxValue;
    };

    fixRange(d.minSpeedMs, d.maxSpeedMs, 1.0, 15.0);
    fixRange(d.minTakeoffAlt, d.maxTakeoffAlt, 10.0, 100.0);
    fixRange(d.minNearHomeAlt, d.maxNearHomeAlt, 20.0, 200.0);
    fixRange(d.minCruiseAlt, d.maxCruiseAlt, 80.0, 500.0);
    fixRange(d.minLandAlt, d.maxLandAlt, 80.0, 150.0);
    clampValue(d.takeoffAlt, d.minTakeoffAlt, d.maxTakeoffAlt, 30.0);
    clampValue(d.nearHomeAlt, d.minNearHomeAlt, d.maxNearHomeAlt, 60.0);
    clampValue(d.cruiseAlt, d.minCruiseAlt, d.maxCruiseAlt, 120.0);
    clampValue(d.landAlt, d.minLandAlt, d.maxLandAlt, 80.0);

    QJsonArray caps = o["capabilities"].toArray();
    for (const auto& v : caps)
        d.capabilities.append(v.toString());
    if (d.capabilities.isEmpty())
        d.capabilities = {"Delivery"};

    return d;
}

// ── DroneManager ──────────────────────────────────────────────────────────────

DroneManager::DroneManager(QObject* parent)
    : QAbstractListModel(parent)
{
    reload();
}

QString DroneManager::jsonPath() const
{
#ifdef Q_OS_ANDROID
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::homePath() + "/SkyRouteX";
    QDir().mkpath(baseDir);
    return baseDir + "/drones.json";
#else
    return QCoreApplication::applicationDirPath() + "/drones.json";
#endif
}

void DroneManager::loadDefaults()
{
    // Built-in fleet — mirrors drone_models.h
    m_drones = {
                { "AIR_ORCA",   "Air-Orca",   true, 7500, 2.0, 8.0, {"Delivery"},                    0, 0, 30, 10, 80, 60, 20, 150, 120, 80, 300, 80, 80, 150 },
                { "PELICAN",    "Pelican",    true, 4000, 2.0, 8.0, {"Delivery"},                    0, 0, 30, 10, 80, 60, 20, 150, 120, 80, 300, 80, 80, 150 },
                { "UTTAM",      "Uttam",      true, 20000, 3.0, 12.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 20, 100, 80, 40, 200, 150, 100, 500, 80, 80, 150 },
                { "SARV_UTTAM", "Sarv-Uttam", true, 20000, 3.0, 12.0, {"Delivery","Drop","Paradrop"},  9, 1, 40, 20, 100, 80, 40, 200, 150, 100, 500, 80, 80, 150 },
                };
}

void DroneManager::loadFromFile()
{
    QFile f(jsonPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        qDebug() << "DroneManager: no drones.json found, using defaults";
        loadDefaults();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject() || !doc.object().contains("drones")) {
        qWarning() << "DroneManager: invalid drones.json, using defaults";
        loadDefaults();
        return;
    }

    m_drones.clear();
    QJsonArray arr = doc.object()["drones"].toArray();
    for (const auto& v : arr)
        m_drones.append(DroneRecord::fromJson(v.toObject()));

    qDebug() << "DroneManager: loaded" << m_drones.size() << "drones from" << jsonPath();
}

void DroneManager::resetToDefaults()
{
    QFile::remove(jsonPath());
    reload();
    qDebug() << "DroneManager: reset to factory defaults";
}

void DroneManager::reload()
{
    beginResetModel();
    loadFromFile();
    endResetModel();
    emit countChanged();
    emit dronesChanged();
}

void DroneManager::save()
{
    QJsonArray arr;
    for (const auto& d : m_drones)
        arr.append(d.toJson());

    QJsonObject root;
    root["drones"] = arr;

    QFile f(jsonPath());
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "DroneManager: cannot write" << jsonPath();
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    qDebug() << "DroneManager: saved" << m_drones.size() << "drones to" << jsonPath();
}

// ── QAbstractListModel ────────────────────────────────────────────────────────

int DroneManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_drones.size();
}

QHash<int, QByteArray> DroneManager::roleNames() const
{
    return {
            { IdRole,               "droneId"           },
            { NameRole,             "droneName"         },
            { ActiveRole,           "droneActive"       },
            { RangeKmRole,          "droneRangeKm"      },
            { SpeedMsRole,          "droneSpeedMs"      },
            { CapabilitiesRole,     "droneCapabilities" },
            { RcSwitchRole,         "droneRcSwitch"     },
            { RcCountRole,          "droneRcCount"      },
            { MinSpeedRole,         "droneMinSpeedMs"   },
            { MinTakeoffAltRole,    "droneMinTakeoffAlt"},
            { MaxTakeoffAltRole,    "droneMaxTakeoffAlt"},
            { MinNearHomeAltRole,   "droneMinNearHomeAlt"},
            { MaxNearHomeAltRole,   "droneMaxNearHomeAlt"},
            { MinCruiseAltRole,     "droneMinCruiseAlt" },
            { MaxCruiseAltRole,     "droneMaxCruiseAlt" },
            { MinLandAltRole,       "droneMinLandAlt"   },
            { MaxLandAltRole,       "droneMaxLandAlt"   },
            { TakeoffAltRole,       "droneTakeoffAlt"   },
            { NearHomeAltRole,      "droneNearHomeAlt"  },
            { CruiseAltRole,        "droneCruiseAlt"    },
            { LandAltRole,          "droneLandAlt"      },
            { NeedsRcRole,          "droneNeedsRc"      },
            { SupportsDeliveryRole, "droneDelivery"     },
            { SupportsDropRole,     "droneDrop"         },
            { SupportsParadropRole, "droneParadrop"     },
            };
}

QVariant DroneManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_drones.size()) return {};
    const DroneRecord& d = m_drones[index.row()];

    switch (role) {
    case IdRole:               return d.id;
    case NameRole:             return d.displayName;
    case ActiveRole:           return d.active;
    case RangeKmRole:          return d.maxRangeM / 1000.0;
    case SpeedMsRole:          return d.maxSpeedMs;
    case CapabilitiesRole:     return d.capabilities;
    case RcSwitchRole:         return d.rcSwitch;
    case RcCountRole:          return d.rcCount;
    case MinSpeedRole:         return d.minSpeedMs;
    case MinTakeoffAltRole:    return d.minTakeoffAlt;
    case MaxTakeoffAltRole:    return d.maxTakeoffAlt;
    case MinNearHomeAltRole:   return d.minNearHomeAlt;
    case MaxNearHomeAltRole:   return d.maxNearHomeAlt;
    case MinCruiseAltRole:     return d.minCruiseAlt;
    case MaxCruiseAltRole:     return d.maxCruiseAlt;
    case MinLandAltRole:       return d.minLandAlt;
    case MaxLandAltRole:       return d.maxLandAlt;
    case TakeoffAltRole:       return d.takeoffAlt;
    case NearHomeAltRole:      return d.nearHomeAlt;
    case CruiseAltRole:        return d.cruiseAlt;
    case LandAltRole:          return d.landAlt;
    case NeedsRcRole:          return d.needsRcSwitch();
    case SupportsDeliveryRole: return d.supportsDelivery();
    case SupportsDropRole:     return d.supportsDrop();
    case SupportsParadropRole: return d.supportsParadrop();
    default: return {};
    }
}

// ── Invokables ────────────────────────────────────────────────────────────────

QVariantList DroneManager::activeDrones() const
{
    QVariantList list;
    for (const DroneRecord& d : m_drones) {
        if (!d.active) continue;
        QVariantMap m;
        m["id"]          = d.id;
        m["name"]        = d.displayName;
        m["maxOneWayKm"] = d.maxRangeM / 1000.0;
        m["minSpeedMs"]  = d.minSpeedMs;
        m["maxSpeedMs"]  = d.maxSpeedMs;
        m["capabilities"]= d.capabilities;
        m["rcSwitch"]    = d.rcSwitch;
        m["rcCount"]     = d.rcCount;
        m["takeoffAlt"]  = d.takeoffAlt;
        m["minTakeoffAlt"]  = d.minTakeoffAlt;
        m["maxTakeoffAlt"]  = d.maxTakeoffAlt;
        m["nearHomeAlt"] = d.nearHomeAlt;
        m["minNearHomeAlt"] = d.minNearHomeAlt;
        m["maxNearHomeAlt"] = d.maxNearHomeAlt;
        m["cruiseAlt"]   = d.cruiseAlt;
        m["minCruiseAlt"]   = d.minCruiseAlt;
        m["maxCruiseAlt"]   = d.maxCruiseAlt;
        m["landAlt"]     = d.landAlt;
        m["minLandAlt"]     = d.minLandAlt;
        m["maxLandAlt"]     = d.maxLandAlt;
        m["needsRc"]     = d.needsRcSwitch();
        m["delivery"]    = d.supportsDelivery();
        m["drop"]        = d.supportsDrop();
        m["paradrop"]    = d.supportsParadrop();
        list.append(m);
    }
    return list;
}

QVariantMap DroneManager::droneAt(int index) const
{
    if (index < 0 || index >= m_drones.size()) return {};
    const DroneRecord& d = m_drones[index];
    QVariantMap m;
    m["id"]          = d.id;
    m["name"]        = d.displayName;
    m["active"]      = d.active;
    m["rangeKm"]     = d.maxRangeM / 1000.0;
    m["speedMs"]     = d.maxSpeedMs;
    m["minSpeedMs"]  = d.minSpeedMs;
    m["maxSpeedMs"]  = d.maxSpeedMs;
    m["capabilities"]= d.capabilities;
    m["rcSwitch"]    = d.rcSwitch;
    m["rcCount"]     = d.rcCount;
    m["takeoffAlt"]  = d.takeoffAlt;
    m["minTakeoffAlt"]  = d.minTakeoffAlt;
    m["maxTakeoffAlt"]  = d.maxTakeoffAlt;
    m["nearHomeAlt"] = d.nearHomeAlt;
    m["minNearHomeAlt"] = d.minNearHomeAlt;
    m["maxNearHomeAlt"] = d.maxNearHomeAlt;
    m["cruiseAlt"]   = d.cruiseAlt;
    m["minCruiseAlt"]   = d.minCruiseAlt;
    m["maxCruiseAlt"]   = d.maxCruiseAlt;
    m["landAlt"]     = d.landAlt;
    m["minLandAlt"]     = d.minLandAlt;
    m["maxLandAlt"]     = d.maxLandAlt;
    m["needsRc"]     = d.needsRcSwitch();
    m["delivery"]    = d.supportsDelivery();
    m["drop"]        = d.supportsDrop();
    m["paradrop"]    = d.supportsParadrop();
    return m;
}

int DroneManager::addDrone(const QString& name,
                           double rangeKm, double minSpeedMs, double maxSpeedMs,
                           bool delivery, bool drop, bool paradrop,
                           int rcSwitch, int rcCount,
                           double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
                           double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
                           double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
                           double landAlt, double minLandAlt, double maxLandAlt)
{
    DroneRecord d;
    d.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
    d.displayName = name.trimmed();
    d.active      = true;
    d.maxRangeM   = rangeKm * 1000.0;
    d.maxSpeedMs  = maxSpeedMs;
    d.rcSwitch       = rcSwitch;
    d.rcCount        = rcCount;
    d.minSpeedMs     = minSpeedMs;
    d.takeoffAlt     = takeoffAlt;
    d.minTakeoffAlt  = minTakeoffAlt;
    d.maxTakeoffAlt  = maxTakeoffAlt;
    d.nearHomeAlt    = nearHomeAlt;
    d.minNearHomeAlt = minNearHomeAlt;
    d.maxNearHomeAlt = maxNearHomeAlt;
    d.cruiseAlt      = cruiseAlt;
    d.minCruiseAlt   = minCruiseAlt;
    d.maxCruiseAlt   = maxCruiseAlt;
    d.landAlt        = landAlt;
    d.minLandAlt     = minLandAlt;
    d.maxLandAlt     = maxLandAlt;
    if (delivery) d.capabilities.append("Delivery");
    if (drop)     d.capabilities.append("Drop");
    if (paradrop) d.capabilities.append("Paradrop");
    if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

    int row = m_drones.size();
    beginInsertRows({}, row, row);
    m_drones.append(d);
    endInsertRows();

    save();
    emit countChanged();
    emit dronesChanged();
    return row;
}

void DroneManager::updateDrone(int index,
                               const QString& name,
                               double rangeKm, double minSpeedMs, double maxSpeedMs,
                               bool delivery, bool drop, bool paradrop,
                               int rcSwitch, int rcCount,
                               double takeoffAlt, double minTakeoffAlt, double maxTakeoffAlt,
                               double nearHomeAlt, double minNearHomeAlt, double maxNearHomeAlt,
                               double cruiseAlt, double minCruiseAlt, double maxCruiseAlt,
                               double landAlt, double minLandAlt, double maxLandAlt)
{
    if (index < 0 || index >= m_drones.size()) return;
    DroneRecord& d  = m_drones[index];
    d.displayName   = name.trimmed();
    d.maxRangeM     = rangeKm * 1000.0;
    d.maxSpeedMs    = maxSpeedMs;
    d.rcSwitch       = rcSwitch;
    d.rcCount        = rcCount;
    d.minSpeedMs     = minSpeedMs;
    d.takeoffAlt     = takeoffAlt;
    d.minTakeoffAlt  = minTakeoffAlt;
    d.maxTakeoffAlt  = maxTakeoffAlt;
    d.nearHomeAlt    = nearHomeAlt;
    d.minNearHomeAlt = minNearHomeAlt;
    d.maxNearHomeAlt = maxNearHomeAlt;
    d.cruiseAlt      = cruiseAlt;
    d.minCruiseAlt   = minCruiseAlt;
    d.maxCruiseAlt   = maxCruiseAlt;
    d.landAlt        = landAlt;
    d.minLandAlt     = minLandAlt;
    d.maxLandAlt     = maxLandAlt;
    d.capabilities.clear();
    if (delivery) d.capabilities.append("Delivery");
    if (drop)     d.capabilities.append("Drop");
    if (paradrop) d.capabilities.append("Paradrop");
    if (d.capabilities.isEmpty()) d.capabilities = {"Delivery"};

    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    save();
    emit dronesChanged();
}

void DroneManager::setActive(int index, bool active)
{
    if (index < 0 || index >= m_drones.size()) return;
    m_drones[index].active = active;
    emit dataChanged(createIndex(index, 0), createIndex(index, 0), {ActiveRole});
    save();
    emit dronesChanged();
}

void DroneManager::deactivate(int index)
{
    setActive(index, false);
}
