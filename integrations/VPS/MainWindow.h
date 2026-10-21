#pragma once
#include <QMainWindow>
#include <QMap>
#include "MavlinkTcpLink.h"
#include "VpsProtocol.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class VioMapWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void toggleConnection();
    void onTunnel(const TunnelMsg &m);
    void onStatus(const Vps::Status &s);
    void onConfig(const Vps::Config &c);
    void onGpsPosition(const GpsPosition &g);

private:
    QWidget *buildConnectionBar();
    QWidget *buildStatusTab();
    QWidget *buildMapTab();
    QWidget *buildControlTab();
    QWidget *buildConfigTab();
    QWidget *buildInfoTab();

    void send(quint16 type, const QByteArray &payload, const QString &what);
    void log(const QString &s);
    QLabel *addRow(class QFormLayout *f, const QString &key, const QString &label);
    void setLabel(const QString &key, const QString &text);
    void setProcessLabel(const QString &key, quint8 v);
    Vps::GpsCheck   readGpsCheck() const;
    Vps::GpsJamming readJamming() const;

    MavlinkTcpLink m_link;

    // connection
    QLineEdit *m_host; QSpinBox *m_port;
    QSpinBox *m_tgtSys, *m_tgtComp;
    QCheckBox *m_hb, *m_autoTarget;
    QPushButton *m_connBtn;
    QLabel *m_linkState, *m_hbState;
    QLabel *m_vioState;

    // status labels by key
    QMap<QString, QLabel *> m_lbl;

    // control widgets
    QComboBox *m_procCombo;
    QDoubleSpinBox *m_homeLat, *m_homeLon;
    QDoubleSpinBox *m_thresh;
    QSpinBox *m_rcCh;
    QComboBox *m_sensorSpeed;
    QCheckBox *m_logAuto;

    // config widgets
    QComboBox *m_gpsFix; QSpinBox *m_gpsSats;
    QDoubleSpinBox *m_gpsHdop, *m_gpsHealthy, *m_gpsUnhealthy;
    QSpinBox *m_gpsHAcc, *m_gpsVAcc;
    QCheckBox *m_jamEn; QDoubleSpinBox *m_jamTime, *m_jamUnhealthy;
    QSpinBox *m_jamDH, *m_jamDV;

    QPlainTextEdit *m_log;
    VioMapWidget *m_map;
    bool m_vpsTargetLearned = false;
    quint8 m_fcGpsSys = 0, m_fcGpsComp = 0;
    bool m_manualAiding = false;
};
