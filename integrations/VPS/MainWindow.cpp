#include "MainWindow.h"
#include "VioMapWidget.h"

#include <QApplication>
#include <functional>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Vps;

static QDoubleSpinBox *dspin(double lo, double hi, int dec, double val, double step = 1.0)
{
    auto *s = new QDoubleSpinBox;
    s->setDecimals(dec); s->setRange(lo, hi); s->setValue(val); s->setSingleStep(step);
    return s;
}
static QSpinBox *ispin(int lo, int hi, int val)
{
    auto *s = new QSpinBox;
    s->setRange(lo, hi); s->setValue(val);
    return s;
}
static QPushButton *btn(const QString &t, QWidget *parent, const std::function<void()> &fn)
{
    auto *b = new QPushButton(t, parent);
    QObject::connect(b, &QPushButton::clicked, parent, [fn] { fn(); });
    return b;
}

static void styleToggle(QPushButton *button, bool on)
{
    button->setText(on ? "ON" : "OFF");
    button->setStyleSheet(on
        ? "QPushButton { background:#168b4b; border:1px solid #45ed88; color:white; font-weight:bold; padding:6px 18px; }"
          "QPushButton:hover { background:#20aa5d; }"
        : "QPushButton { background:#a83232; border:1px solid #ff7777; color:white; font-weight:bold; padding:6px 18px; }"
          "QPushButton:hover { background:#c74343; }");
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("VPS Ground Station (MAVLink TUNNEL over TCP)");
    qApp->setStyleSheet(R"(
        QMainWindow, QWidget { background: #111a22; color: #e7f1f6; }
        QGroupBox { border: 1px solid #355062; border-radius: 6px; margin-top: 10px; padding: 8px; font-weight: bold; color: #77dbff; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { background: #176b8f; border: 1px solid #3fbcdf; border-radius: 5px; color: white; font-weight: bold; padding: 6px 10px; }
        QPushButton:hover { background: #239cc6; } QPushButton:pressed { background: #0d4b65; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit { background: #202d38; border: 1px solid #426274; border-radius: 4px; padding: 3px; color: #f2f7f9; }
        QTabBar::tab { background: #22313d; padding: 8px 18px; color: #bdced7; } QTabBar::tab:selected { background: #1688b5; color: white; }
        QCheckBox::indicator:checked { background: #2ccc70; border: 1px solid #9af0be; }
    )");

    auto *central = new QWidget;
    auto *v = new QVBoxLayout(central);
    v->addWidget(buildConnectionBar());

    auto *tabs = new QTabWidget;
    tabs->addTab(buildMapTab(),     "Live Map");
    tabs->addTab(buildConfigTab(),  "GPS Config");
    tabs->addTab(buildInfoTab(),    "Info / Anchors");

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setFont(QFont("monospace"));

    auto *split = new QSplitter(Qt::Vertical);
    split->addWidget(tabs);
    split->addWidget(m_log);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    v->addWidget(split);
    setCentralWidget(central);

    connect(&m_link, &MavlinkTcpLink::opened, this, [this] {
        m_vpsTargetLearned = false;
        m_connBtn->setText("Disconnect");
        m_linkState->setText("Connected");
        m_vioState->setText("Waiting for VPS MAVLink…");
        m_vioState->setStyleSheet("color:#ffbd4a; font-weight:bold;");
        m_link.setHeartbeatEnabled(m_hb->isChecked());
        log("TCP connected");
    });
    connect(&m_link, &MavlinkTcpLink::closed, this, [this] {
        m_connBtn->setText("Connect");
        m_linkState->setText("Disconnected");
        m_hbState->setText("-");
        m_vioState->setText("VPS disconnected");
        m_vioState->setStyleSheet("color:#ff5d5d; font-weight:bold;");
        m_link.setHeartbeatEnabled(false);
        log("TCP disconnected");
    });
    connect(&m_link, &MavlinkTcpLink::error, this, [this](const QString &e) { log("Socket error: " + e); });
    connect(&m_link, &MavlinkTcpLink::heartbeatReceived, this, [this](quint8 s, quint8 c) {
        if (s == 200 && c == 14) {
            m_hbState->setText("VPS sys 200 / comp 14");
            m_vioState->setText("VPS MAVLink connected");
            m_vioState->setStyleSheet("color:#45ed88; font-weight:bold;");
        } else if (s == 1 && c == 1) m_hbState->setText("FC sys 1 / comp 1");
    });
    connect(&m_link, &MavlinkTcpLink::tunnelReceived, this, &MainWindow::onTunnel);
    connect(&m_link, &MavlinkTcpLink::gpsPositionReceived, this, &MainWindow::onGpsPosition);
    connect(m_hb, &QCheckBox::toggled, this, [this](bool on) { m_link.setHeartbeatEnabled(on && m_link.isOpen()); });
}

// ---------------------------------------------------------------- UI builders

QWidget *MainWindow::buildConnectionBar()
{
    auto *g = new QGroupBox("Connection");
    auto *h = new QHBoxLayout(g);
    m_host = new QLineEdit("127.0.0.1");
    m_port = ispin(1, 65535, 5760);
    m_tgtSys  = ispin(0, 255, 1);
    m_tgtComp = ispin(0, 255, 191);   // MAV_COMP_ID_ONBOARD_COMPUTER; adjust to your VPS
    m_hb = new QCheckBox("Send GCS heartbeat"); m_hb->setChecked(true);
    m_autoTarget = new QCheckBox("Auto-target VPS"); m_autoTarget->setChecked(true);
    m_connBtn = new QPushButton("Connect");
    m_linkState = new QLabel("Disconnected");
    m_hbState = new QLabel("-");
    m_vioState = new QLabel("VPS disconnected");
    m_vioState->setStyleSheet("color:#ff5d5d; font-weight:bold;");

    h->addWidget(new QLabel("Host:")); h->addWidget(m_host, 1);
    h->addWidget(new QLabel("Port:")); h->addWidget(m_port);
    h->addWidget(new QLabel("Target sys:")); h->addWidget(m_tgtSys);
    h->addWidget(new QLabel("comp:")); h->addWidget(m_tgtComp);
    h->addWidget(m_hb);
    h->addWidget(m_autoTarget);
    h->addWidget(m_connBtn);
    h->addWidget(new QLabel("Link:")); h->addWidget(m_linkState);
    h->addWidget(new QLabel("Peer HB:")); h->addWidget(m_hbState);
    h->addWidget(new QLabel("VIO:")); h->addWidget(m_vioState);
    connect(m_connBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    return g;
}

QLabel *MainWindow::addRow(QFormLayout *f, const QString &key, const QString &label)
{
    auto *l = new QLabel("-");
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    f->addRow(label + ":", l);
    m_lbl[key] = l;
    return l;
}

QWidget *MainWindow::buildStatusTab()
{
    auto *w = new QWidget;
    auto *grid = new QHBoxLayout(w);

    auto *gPos = new QGroupBox("Positions (lat / lon)");
    auto *fp = new QFormLayout(gPos);
    addRow(fp, "vio",   "VIO");
    addRow(fp, "vpr",   "VPR (map matching)");
    addRow(fp, "fused", "Fused (VPS)");
    addRow(fp, "gte",   "Ground Truth Error [m]");
    addRow(fp, "nav_fc", "Navigation source sys1");
    addRow(fp, "nav_vps", "Navigation source sys200");
    addRow(fp, "nav_active", "Active navigation");

    auto *gLocal = new QGroupBox("Local frame");
    auto *fl = new QFormLayout(gLocal);
    addRow(fl, "pos", "Position X/Y/Z [m]");
    addRow(fl, "vel", "Velocity X/Y/Z [m/s]");
    addRow(fl, "health", "VIO health");
    addRow(fl, "feat",   "Feature count");

    auto *gProc = new QGroupBox("Processes / system");
    auto *fs = new QFormLayout(gProc);
    addRow(fs, "sensor", "Sensors");
    addRow(fs, "viop",   "VIO");
    addRow(fs, "vprp",   "VPR");
    addRow(fs, "logp",   "Logging");
    addRow(fs, "aid",    "Aiding (manual)");
    addRow(fs, "autoaid", "Auto-Aiding");
    addRow(fs, "temp",   "Chip temps [°C]");
    addRow(fs, "disk",   "Free disk [GB]");
    addRow(fs, "vpsid",  "VPS ID (boot count)");

    auto *c1 = new QVBoxLayout; c1->addWidget(gPos); c1->addWidget(gLocal); c1->addStretch();
    auto *c2 = new QVBoxLayout; c2->addWidget(gProc); c2->addStretch();
    grid->addLayout(c1, 1);
    grid->addLayout(c2, 1);
    return w;
}

QWidget *MainWindow::buildMapTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    auto *summary = new QGroupBox("VIO navigation overview");
    auto *h = new QHBoxLayout(summary);
    auto *error = new QLabel("VIO → fused: waiting for data");
    error->setObjectName("mapError");
    auto *distance = new QLabel("VIO path: 0 m");
    distance->setObjectName("mapDistance");
    h->addWidget(error); h->addSpacing(28); h->addWidget(distance); h->addStretch();
    auto *clear = new QPushButton("Clear tracks");
    connect(clear, &QPushButton::clicked, this, [this, error, distance] {
        m_map->clearTracks(); error->setText("VIO → fused: waiting for data"); distance->setText("VIO path: 0 m");
    });
    h->addWidget(clear);
    m_map = new VioMapWidget;
    auto *content = new QHBoxLayout;
    auto *controls = new QScrollArea; controls->setWidgetResizable(true); controls->setWidget(buildControlTab()); controls->setMinimumWidth(315); controls->setMaximumWidth(430);
    auto *telemetry = new QScrollArea; telemetry->setWidgetResizable(true); telemetry->setWidget(buildStatusTab()); telemetry->setMinimumWidth(280); telemetry->setMaximumWidth(420);
    content->addWidget(controls); content->addWidget(m_map, 1); content->addWidget(telemetry);
    v->addWidget(summary); v->addLayout(content, 1);
    auto *note = new QLabel("Satellite tiles require a Bing Maps Enterprise key. Tracks: cyan VIO, purple VPR, orange fused VPS, green ArduPilot FC GPS.");
    note->setWordWrap(true); note->setStyleSheet("color:#556; padding: 4px;");
    v->addWidget(note);
    return w;
}

QWidget *MainWindow::buildControlTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    // 5.1 process on/off
    auto *g1 = new QGroupBox("Processes (5.1)");
    auto *h1 = new QHBoxLayout(g1);
    m_procCombo = new QComboBox;
    m_procCombo->addItems({"Sensors", "VIO", "VPR", "Logging"});
    h1->addWidget(m_procCombo);
    h1->addWidget(btn("Start", this, [this] {
        send(TX_PROCESS, buildProcess(Process(m_procCombo->currentIndex()), true),
             "Start " + m_procCombo->currentText()); }));
    h1->addWidget(btn("Stop", this, [this] {
        send(TX_PROCESS, buildProcess(Process(m_procCombo->currentIndex()), false),
             "Stop " + m_procCombo->currentText()); }));
    auto *vioToggle = new QPushButton(this);
    auto *vprToggle = new QPushButton(this);
    styleToggle(vioToggle, false);
    styleToggle(vprToggle, false);
    h1->addWidget(new QLabel("VIO:"));
    h1->addWidget(vioToggle);
    h1->addWidget(new QLabel("VPR:"));
    h1->addWidget(vprToggle);
    connect(vioToggle, &QPushButton::clicked, this, [this, vioToggle] {
        const bool on = vioToggle->text() != "ON";
        send(TX_PROCESS, buildProcess(PROC_VIO, on), on ? "Start VIO" : "Stop VIO");
        styleToggle(vioToggle, on);
    });
    connect(vprToggle, &QPushButton::clicked, this, [this, vprToggle] {
        const bool on = vprToggle->text() != "ON";
        send(TX_PROCESS, buildProcess(PROC_VPR, on), on ? "Start VPR" : "Stop VPR");
        styleToggle(vprToggle, on);
    });
    h1->addStretch();

    // 5.2 / 5.10 home / initial
    auto *g2 = new QGroupBox("VPS Home / Initial position (5.2, 5.10)");
    auto *h2 = new QHBoxLayout(g2);
    m_homeLat = dspin(-90, 90, 8, 0);
    m_homeLon = dspin(-180, 180, 8, 0);
    h2->addWidget(new QLabel("Lat:")); h2->addWidget(m_homeLat);
    h2->addWidget(new QLabel("Lon:")); h2->addWidget(m_homeLon);
    h2->addWidget(btn("Set VPS Home", this, [this] {
        send(TX_SET_HOME, buildHome(m_homeLat->value(), m_homeLon->value()), "Set VPS home"); }));
    h2->addWidget(btn("Set Initial Lat/Lon", this, [this] {
        send(TX_SET_HOME, buildHome(m_homeLat->value(), m_homeLon->value()), "Set VPS initial lat/lon"); }));
    h2->addStretch();

    // 5.3 / 5.4
    auto *g3 = new QGroupBox("Alignment (5.3, 5.4)");
    auto *h3 = new QHBoxLayout(g3);
    h3->addWidget(btn("Auto VPS Position", this, [this] {
        send(TX_AUTO_POSITION, buildByte(100), "Auto VPS position"); }));
    h3->addWidget(btn("Align VPS Heading", this, [this] {
        send(TX_ALIGN_HEADING, buildByte(100), "Align VPS heading"); }));
    h3->addStretch();

    // 5.7 / 5.13 / 5.8
    auto *g4 = new QGroupBox("Aiding (5.7, 5.13, 5.8)");
    auto *h4 = new QHBoxLayout(g4);
    auto *aidingToggle = new QPushButton(this);
    auto *autoAidingToggle = new QPushButton(this);
    styleToggle(aidingToggle, false);
    styleToggle(autoAidingToggle, false);
    h4->addWidget(new QLabel("Aiding:")); h4->addWidget(aidingToggle);
    h4->addWidget(new QLabel("Auto-Aiding:")); h4->addWidget(autoAidingToggle);
    connect(aidingToggle, &QPushButton::clicked, this, [this, aidingToggle] {
        const bool on = aidingToggle->text() != "ON";
        send(TX_AIDING, buildByte(on ? 1 : 0), on ? "Aiding ON" : "Aiding OFF");
        m_manualAiding = on;
        setLabel("aid", on ? "ON (commanded)" : "OFF (commanded)");
        styleToggle(aidingToggle, on);
    });
    connect(autoAidingToggle, &QPushButton::clicked, this, [this, autoAidingToggle] {
        const bool on = autoAidingToggle->text() != "ON";
        send(TX_AUTO_AIDING, buildByte(on ? 1 : 0), on ? "Auto aiding ON" : "Auto aiding OFF");
        styleToggle(autoAidingToggle, on);
    });
    m_rcCh = ispin(5, 100, 100);
    m_rcCh->setToolTip("5..16 = RC channel, 100 = disabled");
    h4->addWidget(new QLabel("RC channel (100=off):")); h4->addWidget(m_rcCh);
    h4->addWidget(btn("Set RC Channel", this, [this] {
        const int c = m_rcCh->value();
        if (c != 100 && (c < 5 || c > 16)) { log("RC channel must be 5..16 or 100"); return; }
        send(TX_RC_CHANNEL, buildByte(quint8(c)), QString("Set RC channel %1").arg(c)); }));
    h4->addStretch();

    // 5.5
    auto *g5 = new QGroupBox("GPS-to-VPS threshold (5.5)");
    auto *h5 = new QHBoxLayout(g5);
    m_thresh = dspin(-1, 100000, 2, 10);
    h5->addWidget(new QLabel("Threshold [m] (-1 = disable):")); h5->addWidget(m_thresh);
    h5->addWidget(btn("Set", this, [this] {
        const double t = m_thresh->value();
        if (t != -1 && t <= 0) { log("Threshold must be -1 or > 0"); return; }
        send(TX_GPS_THRESHOLD, buildFloat(float(t)), "Set GPS->VPS threshold"); }));
    h5->addStretch();

    // 5.9 / 5.11
    auto *g6 = new QGroupBox("Logging / Sensors (5.9, 5.11)");
    auto *h6 = new QHBoxLayout(g6);
    m_logAuto = new QCheckBox("Logging auto-start");
    h6->addWidget(m_logAuto);
    h6->addWidget(btn("Apply", this, [this] {
        send(TX_LOG_AUTOSTART, buildByte(m_logAuto->isChecked()), "Set logging auto-start"); }));
    h6->addSpacing(30);
    m_sensorSpeed = new QComboBox; m_sensorSpeed->addItems({"High (0)", "Low (1)"});
    h6->addWidget(new QLabel("Sensor speed:")); h6->addWidget(m_sensorSpeed);
    h6->addWidget(btn("Apply", this, [this] {
        send(TX_SENSOR_SPEED, buildByte(quint8(m_sensorSpeed->currentIndex())), "Set sensor speed"); }));
    h6->addStretch();

    for (auto *g : {g1, g2, g3, g4, g5, g6}) v->addWidget(g);
    v->addStretch();
    return w;
}

QWidget *MainWindow::buildConfigTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *top = new QHBoxLayout;
    top->addWidget(btn("Refresh User Config (5.12)", this, [this] {
        send(TX_REFRESH_CONFIG, buildByte(1), "Refresh user config"); }));
    top->addStretch();
    v->addLayout(top);

    auto *gc = new QGroupBox("GPS check parameters (5.14)");
    auto *fc = new QFormLayout(gc);
    m_gpsFix = new QComboBox;
    m_gpsFix->addItem("3D_FIX", 3); m_gpsFix->addItem("DGPS", 4);
    m_gpsFix->addItem("RTK_FLOAT", 5); m_gpsFix->addItem("RTK_FIXED", 6);
    m_gpsSats = ispin(0, 255, 10);
    m_gpsHdop = dspin(0, 100, 2, 2.0, 0.1);
    m_gpsHAcc = ispin(0, 1000000, 2000);
    m_gpsVAcc = ispin(0, 1000000, 500);
    m_gpsHealthy = dspin(0, 3600, 1, 5.0, 0.5);
    m_gpsUnhealthy = dspin(0, 3600, 1, 2.0, 0.5);
    fc->addRow("Min fix type:", m_gpsFix);
    fc->addRow("Min satellites:", m_gpsSats);
    fc->addRow("Max HDOP:", m_gpsHdop);
    fc->addRow("Max horiz. accuracy [mm]:", m_gpsHAcc);
    fc->addRow("Max vel. accuracy [mm/s]:", m_gpsVAcc);
    fc->addRow("GPS healthy duration [s]:", m_gpsHealthy);
    fc->addRow("GPS unhealthy duration [s]:", m_gpsUnhealthy);
    fc->addRow(btn("Apply GPS Check", this, [this] {
        send(TX_GPS_CHECK, buildGpsCheck(readGpsCheck()), "Set GPS check params"); }));

    auto *gj = new QGroupBox("GPS jamming detection (5.15)");
    auto *fj = new QFormLayout(gj);
    m_jamEn = new QCheckBox("Enable");
    m_jamTime = dspin(0, 3600, 1, 1.0, 0.5);
    m_jamDH = ispin(0, 1000000, 1000);
    m_jamDV = ispin(0, 1000000, 500);
    m_jamUnhealthy = dspin(0, 3600, 1, 2.0, 0.5);
    fj->addRow(m_jamEn);
    fj->addRow("Time sample [s]:", m_jamTime);
    fj->addRow("Delta horiz. accuracy [mm]:", m_jamDH);
    fj->addRow("Delta vel. accuracy [mm/s]:", m_jamDV);
    fj->addRow("Jamming unhealthy duration [s]:", m_jamUnhealthy);
    fj->addRow(btn("Apply Jamming Params", this, [this] {
        send(TX_GPS_JAMMING, buildGpsJamming(readJamming()), "Set GPS jamming params"); }));

    auto *h = new QHBoxLayout; h->addWidget(gc); h->addWidget(gj);
    v->addLayout(h);
    v->addStretch();
    return w;
}

QWidget *MainWindow::buildInfoTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *gv = new QGroupBox("VPS version (5.17)");
    auto *fv = new QFormLayout(gv);
    addRow(fv, "v_vps", "VPS software");
    addRow(fv, "v_obc", "OBC2");
    addRow(fv, "v_mcusw", "MCU software");
    addRow(fv, "v_mcuhw", "MCU hardware");
    addRow(fv, "v_board", "Board version");
    addRow(fv, "v_sn", "Serial number");
    fv->addRow(btn("Request Version", this, [this] { send(TX_VERSION, buildByte(1), "Request version info"); }));

    auto *ga = new QGroupBox("Anchor areas (5.6, 5.16)");
    auto *fa = new QFormLayout(ga);
    addRow(fa, "a_count", "Anchor area count");
    addRow(fa, "a_zoom",  "Zoom level / index");
    addRow(fa, "a_tl",    "Top-left lat/lon");
    addRow(fa, "a_br",    "Bottom-right lat/lon");
    auto *hb = new QHBoxLayout;
    hb->addWidget(btn("Request Count", this, [this] { send(TX_ANCHOR_COUNT, buildByte(100), "Request anchor count"); }));
    hb->addWidget(btn("Request Metadata", this, [this] { send(TX_ANCHOR_META, buildByte(1), "Request anchor metadata"); }));
    fa->addRow(hb);

    v->addWidget(gv); v->addWidget(ga); v->addStretch();
    return w;
}

// ---------------------------------------------------------------- logic

void MainWindow::toggleConnection()
{
    if (m_link.isOpen()) { m_link.close(); return; }
    m_link.open(m_host->text().trimmed(), quint16(m_port->value()));
    m_linkState->setText("Connecting...");
    log(QString("Connecting to %1:%2 ...").arg(m_host->text()).arg(m_port->value()));
}

void MainWindow::send(quint16 type, const QByteArray &payload, const QString &what)
{
    if (!m_link.isOpen()) { log("Not connected - cannot send: " + what); return; }
    const bool ok = m_link.sendTunnel(quint8(m_tgtSys->value()), quint8(m_tgtComp->value()), type, payload);
    log(QString("TX [%1] type=%2 len=%3 : %4 %5")
            .arg(what).arg(type).arg(payload.size()).arg(QString(payload.toHex(' ')), QString(ok ? "" : "(FAILED)")));
}

void MainWindow::onTunnel(const TunnelMsg &m)
{
    if (m.srcSys != 200 || m.srcComp != 14) return;
    m_vioState->setText("VPS MAVLink connected");
    m_vioState->setStyleSheet("color:#45ed88; font-weight:bold;");
    // Status traffic identifies the actual VPS MAVLink endpoint; do not rely
    // on a guessed component id when returning commands.
    if (m.payloadType == RX_STATUS && m_autoTarget->isChecked()) {
        const bool changed = m_tgtSys->value() != m.srcSys || m_tgtComp->value() != m.srcComp;
        m_tgtSys->setValue(m.srcSys); m_tgtComp->setValue(m.srcComp);
        if (changed || !m_vpsTargetLearned)
            log(QString("VPS endpoint learned: sys %1 / comp %2; commands will target it")
                    .arg(m.srcSys).arg(m.srcComp));
        m_vpsTargetLearned = true;
    }
    switch (m.payloadType) {
    case RX_STATUS: {
        Status s;
        if (parseStatus(m.payload, s)) onStatus(s);
        else log(QString("Status message too short (%1 bytes)").arg(m.payload.size()));
        break;
    }
    case RX_ANCHOR_COUNT:
        if (!m.payload.isEmpty()) {
            setLabel("a_count", QString::number(quint8(m.payload[0])));
            log(QString("RX anchor area count = %1").arg(quint8(m.payload[0])));
        }
        break;
    case RX_CONFIG: {
        Config c;
        if (parseConfig(m.payload, c)) onConfig(c);
        else log(QString("Config message too short (%1 bytes)").arg(m.payload.size()));
        break;
    }
    case RX_ANCHOR_META: {
        AnchorMeta a;
        if (parseAnchorMeta(m.payload, a)) {
            setLabel("a_zoom", QString("%1 / %2").arg(a.zoom).arg(a.index));
            setLabel("a_tl", QString("%1, %2").arg(a.tlLat, 0, 'f', 6).arg(a.tlLon, 0, 'f', 6));
            setLabel("a_br", QString("%1, %2").arg(a.brLat, 0, 'f', 6).arg(a.brLon, 0, 'f', 6));
            log("RX anchor area metadata");
        }
        break;
    }
    case RX_VERSION: {
        VersionInfo v;
        if (parseVersion(m.payload, v)) {
            setLabel("v_vps", v.vps); setLabel("v_obc", v.obc2); setLabel("v_mcusw", v.mcuSw);
            setLabel("v_mcuhw", v.mcuHw); setLabel("v_board", v.board); setLabel("v_sn", v.serial);
            log("RX version info");
        }
        break;
    }
    default:
        log(QString("RX unknown TUNNEL payload_type=%1 len=%2: %3")
                .arg(m.payloadType).arg(m.payload.size()).arg(QString(m.payload.toHex(' '))));
    }
}

void MainWindow::onStatus(const Status &s)
{
    m_map->addSample(s.vioLat, s.vioLon, s.vprLat, s.vprLon, s.fusedLat, s.fusedLon);
    if (auto *l = findChild<QLabel *>("mapError")) l->setText(QString("VIO → GPS: %1 m | VIO → fused: %2 m")
        .arg(m_map->vioGpsErrorMetres(), 0, 'f', 2).arg(m_map->vioFusedErrorMetres(), 0, 'f', 2));
    if (auto *l = findChild<QLabel *>("mapDistance")) l->setText(QString("VIO path: %1 m").arg(m_map->vioDistanceMetres(), 0, 'f', 1));
    auto ll = [](double a, double b) { return QString("%1, %2").arg(a, 0, 'f', 7).arg(b, 0, 'f', 7); };
    setLabel("vio",   ll(s.vioLat, s.vioLon));
    setLabel("vpr",   ll(s.vprLat, s.vprLon));
    setLabel("fused", ll(s.fusedLat, s.fusedLon));
    setLabel("gte",   QString::number(s.gte, 'f', 2));
    setLabel("nav_fc", "FC_GPS");
    setLabel("nav_vps", "VPS");
    setLabel("nav_active", s.gpsInUse ? "FC_GPS (sys1)" : "VPS (sys200)");
    setLabel("pos",   QString("%1 / %2 / %3").arg(s.posX, 0, 'f', 2).arg(s.posY, 0, 'f', 2).arg(s.posZ, 0, 'f', 2));
    setLabel("vel",   QString("%1 / %2 / %3").arg(s.velX, 0, 'f', 2).arg(s.velY, 0, 'f', 2).arg(s.velZ, 0, 'f', 2));
    setLabel("health", QString::number(s.health, 'f', 2));
    setLabel("feat",   QString::number(s.featureCount, 'f', 0));
    setProcessLabel("sensor", s.sensorStatus);
    setProcessLabel("viop",   s.vioStatus);
    if (s.vioStatus == 2) {
        m_vioState->setText("VIO running");
        m_vioState->setStyleSheet("color:#45ed88; font-weight:bold;");
    } else if (s.vioStatus == 1 || s.vioStatus == 3) {
        m_vioState->setText("VIO " + processStatusText(s.vioStatus).toLower());
        m_vioState->setStyleSheet("color:#ffbd4a; font-weight:bold;");
    }
    setProcessLabel("vprp",   s.vprStatus);
    setProcessLabel("logp",   s.logStatus);
    setLabel("aid", m_manualAiding ? "ON (commanded)" : "OFF (commanded)");
    setLabel("autoaid", s.aidFlag ? "ON (reported)" : "OFF (reported)");
    setLabel("temp",  QString("%1 / %2 / %3").arg(s.chip1Temp).arg(s.chip2Temp).arg(s.chip3Temp));
    setLabel("disk",  QString::number(s.diskGB));
    setLabel("vpsid", QString::number(s.vpsId, 'f', 0));
}

void MainWindow::onGpsPosition(const GpsPosition &g)
{
    if (!m_map) return;
    if (g.srcSys != 1 || g.srcComp != 1) return;
    m_fcGpsSys = g.srcSys; m_fcGpsComp = g.srcComp;
    m_map->addGpsSample(g.lat, g.lon);
    if (auto *l = findChild<QLabel *>("mapError"))
        l->setText(QString("VIO → GPS: %1 m | VIO → fused: %2 m")
                   .arg(m_map->vioGpsErrorMetres(), 0, 'f', 2)
                   .arg(m_map->vioFusedErrorMetres(), 0, 'f', 2));
}

void MainWindow::onConfig(const Config &c)
{
    m_thresh->setValue(c.gpsSwitchThreshold);
    if (c.rcChannel == 100 || (c.rcChannel > 4 && c.rcChannel < 17)) m_rcCh->setValue(c.rcChannel);
    m_logAuto->setChecked(c.logAutoStart);
    m_homeLat->setValue(c.initLat); m_homeLon->setValue(c.initLon);
    int idx = m_gpsFix->findData(c.gps.fixType); if (idx >= 0) m_gpsFix->setCurrentIndex(idx);
    m_gpsSats->setValue(c.gps.satCount);
    m_gpsHdop->setValue(c.gps.hdop);
    m_gpsHAcc->setValue(c.gps.hAccMm); m_gpsVAcc->setValue(c.gps.vAccMmS);
    m_gpsHealthy->setValue(c.gps.healthySec); m_gpsUnhealthy->setValue(c.gps.unhealthySec);
    m_jamEn->setChecked(c.jam.enable);
    m_jamTime->setValue(c.jam.timeSampleSec);
    m_jamDH->setValue(c.jam.deltaHAccMm); m_jamDV->setValue(c.jam.deltaVAccMmS);
    m_jamUnhealthy->setValue(c.jam.unhealthySec);
    m_sensorSpeed->setCurrentIndex(c.sensorSpeed ? 1 : 0);
    log("RX user config -> UI updated");
}

GpsCheck MainWindow::readGpsCheck() const
{
    GpsCheck g;
    g.fixType = quint8(m_gpsFix->currentData().toInt());
    g.satCount = quint8(m_gpsSats->value());
    g.hdop = float(m_gpsHdop->value());
    g.hAccMm = m_gpsHAcc->value(); g.vAccMmS = m_gpsVAcc->value();
    g.healthySec = float(m_gpsHealthy->value()); g.unhealthySec = float(m_gpsUnhealthy->value());
    return g;
}

GpsJamming MainWindow::readJamming() const
{
    GpsJamming j;
    j.enable = m_jamEn->isChecked();
    j.timeSampleSec = float(m_jamTime->value());
    j.deltaHAccMm = m_jamDH->value(); j.deltaVAccMmS = m_jamDV->value();
    j.unhealthySec = float(m_jamUnhealthy->value());
    return j;
}

void MainWindow::setLabel(const QString &key, const QString &text)
{
    if (auto *l = m_lbl.value(key)) l->setText(text);
}

void MainWindow::setProcessLabel(const QString &key, quint8 v)
{
    auto *l = m_lbl.value(key);
    if (!l) return;
    l->setText(processStatusText(v));
    const char *col = (v == 2) ? "green" : (v == 0 ? "red" : "darkorange");
    l->setStyleSheet(QString("color:%1; font-weight:bold;").arg(col));
}

void MainWindow::log(const QString &s)
{
    m_log->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss.zzz  ") + s);
}
