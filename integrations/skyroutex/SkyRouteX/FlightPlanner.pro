# # # QT += core gui network widgets qml quick quickcontrols2

# # # CONFIG += c++17

# # # TARGET   = FlightPlanner
# # # TEMPLATE = app

# # # LIBS += -lz

# # # INCLUDEPATH += include

# # # # ── Headers ───────────────────────────────────────────────────────────────────
# # # HEADERS += \
# # #     include/drone_models.h      \
# # #     include/mainwindow.h        \
# # #     include/mapwidget.h         \
# # #     include/mission_validator.h \
# # #     include/path_planner.h      \
# # #     include/srtm_reader.h       \
# # #     include/waypoint_writer.h

# # # # ── Sources ───────────────────────────────────────────────────────────────────
# # # SOURCES += \
# # #     src/main.cpp                \
# # #     src/mainwindow.cpp          \
# # #     src/mapwidget.cpp           \
# # #     src/mission_validator.cpp   \
# # #     src/path_planner.cpp        \
# # #     src/srtm_reader.cpp         \
# # #     src/waypoint_writer.cpp

# # # # ── QML files ─────────────────────────────────────────────────────────────────
# # # QML_FILES += \
# # #     qml/qmldir              \
# # #     qml/main.qml            \
# # #     qml/Theme.qml           \
# # #     qml/MainView.qml        \
# # #     qml/MapView.qml         \
# # #     qml/NavButton.qml       \
# # #     qml/SectionBlock.qml    \
# # #     qml/TerrainProfile.qml  \
# # #     qml/TooltipRow.qml      \
# # #     qml/LegendDot.qml       \
# # #     qml/FormRow.qml         \
# # #     qml/TitleBar.qml        \
# # #     qml/SideNav.qml         \
# # #     qml/ActivityLog.qml     \
# # #     qml/StatusBar.qml       \
# # #     qml/TripToggle.qml      \
# # #     qml/CoordGroup.qml      \
# # #     qml/DistanceBadge.qml   \
# # #     qml/FpInput.qml         \
# # #     qml/FpButton.qml        \
# # #     qml/FpComboBox.qml      \
# # #     qml/FpSpinBox.qml       \
# # #     qml/FpSectionTitle.qml

# # # # Pass the source qml/ folder path to the compiler so main.cpp can use it
# # # # This lets the app load QML directly from source — no .qrc needed
# # # DEFINES += QML_SOURCE_DIR=\\\"$$PWD/qml/\\\"

# # # # QML import path for Qt Creator tooling
# # # QML_IMPORT_PATH += $$PWD/qml

# # # # ── Copy terrain folder to build output ───────────────────────────────────────
# # # terrain.files   = terrain
# # # terrain.path    = $$OUT_PWD
# # # COPIES         += terrain

# # # # ── Deploy target (Linux) ─────────────────────────────────────────────────────
# # # qnx: target.path = /tmp/$${TARGET}/bin
# # # else: unix:!android: target.path = /opt/$${TARGET}/bin
# # # !isEmpty(target.path): INSTALLS += target



# # QT += core gui network widgets qml quick quickcontrols2 quick3d

# # CONFIG += c++17

# # TARGET   = FlightPlanner
# # TEMPLATE = app

# # LIBS += -lz

# # INCLUDEPATH += include

# # # ── Headers ───────────────────────────────────────────────────────────────────
# # HEADERS += \
# #     include/drone_models.h      \
# #     include/mainwindow.h        \
# #     include/mapwidget.h         \
# #     include/TerrainMeshGeometry.h \
# #     include/RouteLineGeometry.h  \
# #     include/mission_validator.h \
# #     include/path_planner.h      \
# #     include/srtm_reader.h       \
# #     include/waypoint_writer.h

# # # ── Sources ───────────────────────────────────────────────────────────────────
# # SOURCES += \
# #     src/main.cpp                \
# #     src/mainwindow.cpp          \
# #     src/mapwidget.cpp           \
# #     src/TerrainMeshGeometry.cpp \
# #     src/RouteLineGeometry.cpp   \
# #     src/mission_validator.cpp   \
# #     src/path_planner.cpp        \
# #     src/srtm_reader.cpp         \
# #     src/waypoint_writer.cpp

# # # ── QML files ─────────────────────────────────────────────────────────────────
# # QML_FILES += \
# #     qml/qmldir              \
# #     qml/main.qml            \
# #     qml/Theme.qml           \
# #     qml/MainView.qml        \
# #     qml/MapView.qml         \
# #     qml/NavButton.qml       \
# #     qml/PinGate.qml         \
# #     qml/View3D.qml          \
# #     qml/SectionBlock.qml    \
# #     qml/TerrainProfile.qml  \
# #     qml/TooltipRow.qml      \
# #     qml/LegendDot.qml       \
# #     qml/FormRow.qml         \
# #     qml/TitleBar.qml        \
# #     qml/SideNav.qml         \
# #     qml/ActivityLog.qml     \
# #     qml/StatusBar.qml       \
# #     qml/TripToggle.qml      \
# #     qml/CoordGroup.qml      \
# #     qml/DistanceBadge.qml   \
# #     qml/FpInput.qml         \
# #     qml/FpButton.qml        \
# #     qml/FpComboBox.qml      \
# #     qml/FpSpinBox.qml       \
# #     qml/FpSectionTitle.qml

# # # Pass the source qml/ folder path to the compiler so main.cpp can use it
# # # This lets the app load QML directly from source — no .qrc needed
# # DEFINES += QML_SOURCE_DIR=\\\"$$PWD/qml/\\\"

# # # QML import path for Qt Creator tooling
# # QML_IMPORT_PATH += $$PWD/qml

# # # ── Copy terrain folder to build output ───────────────────────────────────────
# # terrain.files   = terrain
# # terrain.path    = $$OUT_PWD
# # COPIES         += terrain

# # # ── Deploy target (Linux) ─────────────────────────────────────────────────────
# # qnx: target.path = /tmp/$${TARGET}/bin
# # else: unix:!android: target.path = /opt/$${TARGET}/bin
# # !isEmpty(target.path): INSTALLS += target


# QT += core gui network widgets qml quick quickcontrols2 quick3d

# CONFIG += c++17

# TARGET   = FlightPlanner
# TEMPLATE = app

# LIBS += -lz

# INCLUDEPATH += include

# # ── Headers ───────────────────────────────────────────────────────────────────
# HEADERS += \
#     include/drone_models.h      \
#     include/mainwindow.h        \
#     include/mapwidget.h         \
#     include/TerrainMeshGeometry.h \
#     include/RouteLineGeometry.h  \
#     include/TileStitcher.h       \
#     include/GeoTerrainGeometry.h \
#     include/GeoRouteGeometry.h   \
#     include/mission_validator.h \
#     include/path_planner.h      \
#     include/srtm_reader.h       \
#     include/waypoint_writer.h

# # ── Sources ───────────────────────────────────────────────────────────────────
# SOURCES += \
#     src/main.cpp                \
#     src/mainwindow.cpp          \
#     src/mapwidget.cpp           \
#     src/TerrainMeshGeometry.cpp \
#     src/RouteLineGeometry.cpp   \
#     src/TileStitcher.cpp        \
#     src/GeoTerrainGeometry.cpp  \
#     src/GeoRouteGeometry.cpp    \
#     src/mission_validator.cpp   \
#     src/path_planner.cpp        \
#     src/srtm_reader.cpp         \
#     src/waypoint_writer.cpp

# # ── QML files ─────────────────────────────────────────────────────────────────
# QML_FILES += \
#     qml/qmldir              \
#     qml/main.qml            \
#     qml/Theme.qml           \
#     qml/MainView.qml        \
#     qml/MapView.qml         \
#     qml/NavButton.qml       \
#     qml/PinGate.qml         \
#     qml/View3D.qml          \
#     qml/SectionBlock.qml    \
#     qml/TerrainProfile.qml  \
#     qml/TooltipRow.qml      \
#     qml/LegendDot.qml       \
#     qml/FormRow.qml         \
#     qml/TitleBar.qml        \
#     qml/SideNav.qml         \
#     qml/ActivityLog.qml     \
#     qml/StatusBar.qml       \
#     qml/TripToggle.qml      \
#     qml/CoordGroup.qml      \
#     qml/DistanceBadge.qml   \
#     qml/FpInput.qml         \
#     qml/FpButton.qml        \
#     qml/FpComboBox.qml      \
#     qml/FpSpinBox.qml       \
#     qml/FpSectionTitle.qml

# # Pass the source qml/ folder path to the compiler so main.cpp can use it
# # This lets the app load QML directly from source — no .qrc needed
# DEFINES += QML_SOURCE_DIR=\\\"$$PWD/qml/\\\"

# # QML import path for Qt Creator tooling
# QML_IMPORT_PATH += $$PWD/qml

# # ── Copy terrain folder to build output ───────────────────────────────────────
# terrain.files   = terrain
# terrain.path    = $$OUT_PWD
# COPIES         += terrain

# # ── Deploy target (Linux) ─────────────────────────────────────────────────────
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target



QT += core gui network widgets qml quick quickcontrols2 quick3d

CONFIG += c++17

TARGET   = FlightPlanner
TEMPLATE = app

LIBS += -lz

INCLUDEPATH += include

# ── Headers ───────────────────────────────────────────────────────────────────
HEADERS += \
    include/KeyManager.h \
    include/drone_models.h      \
    include/mainwindow.h        \
    include/mapwidget.h         \
    include/TerrainMeshGeometry.h \
    include/RouteLineGeometry.h  \
    include/TileStitcher.h       \
    include/DroneManager.h      \
    include/GeoTerrainGeometry.h \
    include/GeoRouteGeometry.h   \
    include/mission_validator.h \
    include/path_planner.h      \
    include/srtm_reader.h       \
    include/waypoint_writer.h

# ── Sources ───────────────────────────────────────────────────────────────────
SOURCES += \
    src/KeyManager.cpp \
    src/main.cpp                \
    src/mainwindow.cpp          \
    src/mapwidget.cpp           \
    src/TerrainMeshGeometry.cpp \
    src/RouteLineGeometry.cpp   \
    src/TileStitcher.cpp        \
    src/DroneManager.cpp        \
    src/GeoTerrainGeometry.cpp  \
    src/GeoRouteGeometry.cpp    \
    src/mission_validator.cpp   \
    src/path_planner.cpp        \
    src/srtm_reader.cpp         \
    src/waypoint_writer.cpp

# ── QML files ─────────────────────────────────────────────────────────────────
QML_FILES += \
    qml/qmldir              \
    qml/main.qml            \
    qml/Theme.qml           \
    qml/MainView.qml        \
    qml/MapView.qml         \
    qml/NavButton.qml       \
    qml/DroneFleetPanel.qml \
    qml/PinGate.qml         \
    qml/View3D.qml          \
    qml/SectionBlock.qml    \
    qml/TerrainProfile.qml  \
    qml/TooltipRow.qml      \
    qml/LegendDot.qml       \
    qml/FormRow.qml         \
    qml/TitleBar.qml        \
    qml/SideNav.qml         \
    qml/ActivityLog.qml     \
    qml/StatusBar.qml       \
    qml/TripToggle.qml      \
    qml/CoordGroup.qml      \
    qml/DistanceBadge.qml   \
    qml/FpInput.qml         \
    qml/FpButton.qml        \
    qml/FpComboBox.qml      \
    qml/FpSpinBox.qml       \
    qml/FpSectionTitle.qml

# Pass the source qml/ folder path to the compiler so main.cpp can use it
# This lets the app load QML directly from source — no .qrc needed

# QML import path for Qt Creator tooling
QML_IMPORT_PATH += $$PWD/qml

# ── Copy terrain folder to build output ───────────────────────────────────────
terrain.files   = terrain
terrain.path    = $$OUT_PWD
COPIES         += terrain

# ── Deploy target (Linux) ─────────────────────────────────────────────────────
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += resources.qrc

android {
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    # Qt's Android TLS plugin needs OpenSSL runtime libraries in the APK/AAB.
    # Put KDAB android_openssl at 3rdparty/android_openssl, or adjust this path.
    exists($$PWD/3rdparty/android_openssl/openssl.pri) {
        include($$PWD/3rdparty/android_openssl/openssl.pri)
    } else {
        warning("Android OpenSSL package not found: add KDAB android_openssl to 3rdparty/android_openssl for HTTPS downloads")
    }
}
