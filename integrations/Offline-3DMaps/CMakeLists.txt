cmake_minimum_required(VERSION 3.21)

project(DroneTerrainSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Network Qml Quick Quick3D QuickControls2 Test ZlibPrivate
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(DroneTerrainSim
    src/main.cpp
    src/elevationprovider.cpp
    src/terraintilegeometry.cpp
    src/terrainmanager.cpp
    src/mavlinkreceiver.cpp
    src/imagerytileprovider.cpp
    src/srtmdownloader.cpp
)

qt_add_qml_module(DroneTerrainSim
    URI DroneTerrainSim
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/TerrainView.qml
        qml/DroneEntity.qml
    SOURCES
        src/geomath.h
        src/elevationprovider.h
        src/terraintilegeometry.h
        src/terrainmanager.h
        src/mavlinkreceiver.h
        src/imagerytileprovider.h
        src/srtmdownloader.h
)

target_link_libraries(DroneTerrainSim PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::QuickControls2
    Qt6::ZlibPrivate
)

target_include_directories(DroneTerrainSim PRIVATE src)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(GeoMathTests tests/test_geomath.cpp)
    target_include_directories(GeoMathTests PRIVATE src)
    target_link_libraries(GeoMathTests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME GeoMathTests COMMAND GeoMathTests)

    qt_add_executable(ElevationProviderTests
        tests/test_elevationprovider.cpp
        src/elevationprovider.cpp
        src/elevationprovider.h
    )
    target_include_directories(ElevationProviderTests PRIVATE src)
    target_link_libraries(ElevationProviderTests PRIVATE Qt6::Core Qt6::Test Qt6::ZlibPrivate)
    add_test(NAME ElevationProviderTests COMMAND ElevationProviderTests)

    qt_add_executable(TerrainManagerTests
        tests/test_terrainmanager.cpp
        src/terrainmanager.cpp
        src/terrainmanager.h
        src/elevationprovider.cpp
        src/elevationprovider.h
        src/imagerytileprovider.cpp
        src/imagerytileprovider.h
        src/srtmdownloader.cpp
        src/srtmdownloader.h
    )
    target_include_directories(TerrainManagerTests PRIVATE src)
    target_link_libraries(TerrainManagerTests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Test Qt6::ZlibPrivate
    )
    add_test(NAME TerrainManagerTests COMMAND TerrainManagerTests)

    qt_add_executable(MavlinkReceiverTests
        tests/test_mavlinkreceiver.cpp
        src/mavlinkreceiver.cpp
        src/mavlinkreceiver.h
    )
    target_include_directories(MavlinkReceiverTests PRIVATE src)
    target_link_libraries(MavlinkReceiverTests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Test
    )
    add_test(NAME MavlinkReceiverTests COMMAND MavlinkReceiverTests)
endif()

install(TARGETS DroneTerrainSim
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
