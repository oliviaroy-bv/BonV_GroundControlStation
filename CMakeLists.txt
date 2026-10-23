cmake_minimum_required(VERSION 3.16)

project(MavlinkPOC VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(CTest)
enable_testing()

# Qt setup
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Concurrent
    Gui 
    Qml 
    Quick 
    Quick3D
    QuickDialogs2
    Network 
    Location 
    Positioning 
    Multimedia 
    SerialPort
    Svg
    WebEngineQuick
    WebEngineCore
    Test
    ZlibPrivate
)

set(BONV_DEFAULT_GSTREAMER_ROOT "${CMAKE_SOURCE_DIR}/integrations/gstreamer")
set(BONV_BUNDLED_GSTREAMER_ROOT "${BONV_DEFAULT_GSTREAMER_ROOT}" CACHE PATH "Bundled GStreamer SDK root")
set(BONV_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH "Runtime deployment output directory")
file(MAKE_DIRECTORY "${BONV_DEPLOY_DIR}")
if(NOT EXISTS "${BONV_BUNDLED_GSTREAMER_ROOT}")
    set(BONV_BUNDLED_GSTREAMER_ROOT "${BONV_DEFAULT_GSTREAMER_ROOT}" CACHE PATH "Bundled GStreamer SDK root" FORCE)
endif()
set(BONV_GSTREAMER_ROOT "")
foreach(_gst_candidate
        "${BONV_BUNDLED_GSTREAMER_ROOT}"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/1.0/msvc_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/gstreamer/msvc_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/1.0/mingw_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/mingw_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/1.0/msvc_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/msvc_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/1.0/mingw_x86_64"
        "${BONV_BUNDLED_GSTREAMER_ROOT}/mingw_x86_64")
    if(EXISTS "${_gst_candidate}/bin" AND EXISTS "${_gst_candidate}/lib/pkgconfig")
        set(BONV_GSTREAMER_ROOT "${_gst_candidate}")
        break()
    endif()
endforeach()
unset(_gst_candidate)

if(BONV_GSTREAMER_ROOT)
    if(WIN32 AND EXISTS "${BONV_GSTREAMER_ROOT}/bin/pkg-config.exe")
        set(PKG_CONFIG_EXECUTABLE "${BONV_GSTREAMER_ROOT}/bin/pkg-config.exe" CACHE FILEPATH "pkg-config executable" FORCE)
    elseif(EXISTS "${BONV_GSTREAMER_ROOT}/bin/pkg-config")
        set(PKG_CONFIG_EXECUTABLE "${BONV_GSTREAMER_ROOT}/bin/pkg-config" CACHE FILEPATH "pkg-config executable" FORCE)
    endif()

    file(TO_NATIVE_PATH "${BONV_GSTREAMER_ROOT}/lib/pkgconfig" _gst_pkg_config_path)
    if(WIN32)
        set(_gst_path_separator ";")
    else()
        set(_gst_path_separator ":")
    endif()
    if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
        set(ENV{PKG_CONFIG_PATH} "${_gst_pkg_config_path}${_gst_path_separator}$ENV{PKG_CONFIG_PATH}")
    else()
        set(ENV{PKG_CONFIG_PATH} "${_gst_pkg_config_path}")
    endif()
    message(STATUS "Using bundled GStreamer SDK: ${BONV_GSTREAMER_ROOT}")
    unset(_gst_path_separator)
    unset(_gst_pkg_config_path)
endif()

option(BONV_REQUIRE_GDAL "Fail CMake configure when GDAL is not available" OFF)
find_package(GDAL CONFIG QUIET)
if(NOT GDAL_FOUND)
    find_package(GDAL QUIET)
endif()
if(BONV_REQUIRE_GDAL AND NOT GDAL_FOUND)
    message(FATAL_ERROR "GDAL was requested with BONV_REQUIRE_GDAL=ON, but CMake could not find it. Set GDAL_DIR or add the GDAL installation prefix to CMAKE_PREFIX_PATH.")
endif()
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER QUIET IMPORTED_TARGET gstreamer-1.0)
    pkg_check_modules(GSTREAMER_APP QUIET IMPORTED_TARGET gstreamer-app-1.0)
    pkg_check_modules(GSTREAMER_VIDEO QUIET IMPORTED_TARGET gstreamer-video-1.0)
    pkg_check_modules(BONV_ZLIB QUIET IMPORTED_TARGET zlib)
endif()

set(BONV_HAS_GSTREAMER FALSE)
if(TARGET PkgConfig::GSTREAMER AND TARGET PkgConfig::GSTREAMER_APP AND TARGET PkgConfig::GSTREAMER_VIDEO)
    set(BONV_HAS_GSTREAMER TRUE)
endif()

# Source files
set(CORE_SOURCES
    comms/transport/tcp/TcpLink.cpp
    comms/transport/udp/UdpLink.cpp
    comms/transport/serial/SerialLink.cpp
    comms/mavlink/Mavlink.cpp
    core/mavlink/MavlinkFrame.cpp
    core/mavlink/MavlinkForwarder.cpp
    core/mavlink/MavlinkInspectorController.cpp
    core/mavlink/MavlinkInspectorFilterModel.cpp
    core/mavlink/MavlinkInspectorGraphModel.cpp
    core/mavlink/MavlinkInspectorModel.cpp
    core/mavlink/MavlinkMessageMetadata.cpp
    core/gimbal/GimbalController.cpp
    core/mission/MissionFileWriter.cpp
    core/map/RasterOverlayManager.cpp
    core/map/RasterOverlaySchemeHandler.cpp
    core/map/RasterOverlayStorage.cpp
    core/map/TileCacheManager.cpp
    core/map/TileCacheRequestInterceptor.cpp
    core/map/TileCacheSchemeHandler.cpp
    core/storage/AppDataPaths.cpp
    core/terrain/TerrainProvider.cpp
    core/vehicle/Vehicle.cpp
    core/vehicle/VehicleManager.cpp
    core/video/Video.cpp
    core/video/VideoItem.cpp
    integrations/Offline-3DMaps/src/elevationprovider.cpp
    integrations/Offline-3DMaps/src/imagerytileprovider.cpp
    integrations/Offline-3DMaps/src/srtmdownloader.cpp
    integrations/Offline-3DMaps/src/terrainmanager.cpp
    integrations/Offline-3DMaps/src/terraintilegeometry.cpp
    integrations/VPS/VpsProtocol.cpp
    integrations/SkyRouteX_New/src/path_planner.cpp
    integrations/SkyRouteX_New/src/waypoint_writer.cpp
)

set(CORE_HEADERS
    comms/transport/ILink.h
    comms/transport/tcp/TcpLink.h
    comms/transport/udp/UdpLink.h
    comms/transport/serial/SerialLink.h
    comms/mavlink/Mavlink.h
    core/mavlink/MavlinkFrame.h
    core/mavlink/MavlinkForwarder.h
    core/mavlink/MavlinkInspectorController.h
    core/mavlink/MavlinkInspectorFilterModel.h
    core/mavlink/MavlinkInspectorGraphModel.h
    core/mavlink/MavlinkInspectorModel.h
    core/mavlink/MavlinkMessageMetadata.h
    core/gimbal/GimbalController.h
    core/mission/MissionFileWriter.h
    core/map/RasterOverlayManager.h
    core/map/RasterOverlaySchemeHandler.h
    core/map/RasterOverlayStorage.h
    core/map/TileCacheManager.h
    core/map/TileCacheRequestInterceptor.h
    core/map/TileCacheSchemeHandler.h
    core/storage/AppDataPaths.h
    core/terrain/TerrainProvider.h
    core/vehicle/Vehicle.h
    core/vehicle/VehicleManager.h
    core/video/Video.h
    core/video/VideoItem.h
    integrations/Offline-3DMaps/src/elevationprovider.h
    integrations/Offline-3DMaps/src/geomath.h
    integrations/Offline-3DMaps/src/imagerytileprovider.h
    integrations/Offline-3DMaps/src/srtmdownloader.h
    integrations/Offline-3DMaps/src/terrainmanager.h
    integrations/Offline-3DMaps/src/terraintilegeometry.h
    integrations/VPS/VpsProtocol.h
    integrations/SkyRouteX_New/include/drone_models.h
    integrations/SkyRouteX_New/include/path_planner.h
    integrations/SkyRouteX_New/include/waypoint_writer.h
)

set(QML_FILES
    ${CMAKE_SOURCE_DIR}/ui/qml/accessories/Palette.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/accessories/StateText.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/accessories/TelemetryIcon.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/accessories/TelemetryText.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/activation/ActivationWindow.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/activation/LoginWindow.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/buttons/CommandButton.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/core/App.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/core/FlyView.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/core/MainPage.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/map/FlyMap.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/map/OfflineTerrainMap.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/BottomPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/BottomTelemetryText.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/LeftSidePanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/MissionBuilderPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/MotorTestPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/ServoPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/TopPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/panels/VerticalTelemetryPanel.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/settings/OemSettingsPage.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/settings/MavlinkInspectorPage.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/settings/MavlinkForwardingPage.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/settings/SettingsWindow.qml
    ${CMAKE_SOURCE_DIR}/ui/qml/video/Video.qml
)

add_library(MavlinkPOCCore STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

if(MSVC)
    target_compile_options(MavlinkPOCCore PRIVATE /FS)
endif()

target_include_directories(MavlinkPOCCore PUBLIC
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/comms
    ${CMAKE_SOURCE_DIR}/comms/transport
    ${CMAKE_SOURCE_DIR}/comms/transport/tcp
    ${CMAKE_SOURCE_DIR}/comms/transport/udp
    ${CMAKE_SOURCE_DIR}/comms/transport/serial
    ${CMAKE_SOURCE_DIR}/comms/mavlink
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/gimbal
    ${CMAKE_SOURCE_DIR}/core/mission
    ${CMAKE_SOURCE_DIR}/core/map
    ${CMAKE_SOURCE_DIR}/core/storage
    ${CMAKE_SOURCE_DIR}/core/terrain
    ${CMAKE_SOURCE_DIR}/core/vehicle
    ${CMAKE_SOURCE_DIR}/core/video
    ${CMAKE_SOURCE_DIR}/integrations/gstreamer
    ${CMAKE_SOURCE_DIR}/integrations/Offline-3DMaps/src
    ${CMAKE_SOURCE_DIR}/integrations/map_provider
    ${CMAKE_SOURCE_DIR}/integrations/mavlink_provider/c_library_v2
    ${CMAKE_SOURCE_DIR}/integrations/SkyRouteX_New
    ${CMAKE_SOURCE_DIR}/integrations/SkyRouteX_New/include
    ${CMAKE_SOURCE_DIR}/media/icons
    ${CMAKE_SOURCE_DIR}/media/logo
)

target_link_libraries(MavlinkPOCCore PUBLIC
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::SerialPort
    Qt6::WebEngineCore
    Qt6::ZlibPrivate
)

if(GDAL_FOUND)
    target_compile_definitions(MavlinkPOCCore PUBLIC HAVE_GDAL)
else()
    message(WARNING "GDAL was not found. GeoTIFF raster map import will be disabled at runtime.")
endif()

if(BONV_HAS_GSTREAMER)
    target_compile_definitions(MavlinkPOCCore PUBLIC HAVE_GSTREAMER)
    target_link_libraries(MavlinkPOCCore PUBLIC PkgConfig::GSTREAMER PkgConfig::GSTREAMER_APP PkgConfig::GSTREAMER_VIDEO)
else()
    message(WARNING "GStreamer, gstreamer-app, or gstreamer-video was not found. Video::start() will report that GStreamer support is unavailable.")
endif()

if(TARGET PkgConfig::BONV_ZLIB)
    target_compile_definitions(MavlinkPOCCore PUBLIC BONV_HAS_ZLIB)
    target_link_libraries(MavlinkPOCCore PUBLIC PkgConfig::BONV_ZLIB)
else()
    message(WARNING "zlib was not found. Missing SRTM terrain tiles will not be downloaded automatically.")
endif()

# Executable
if(WIN32)
    set(BONV_APP_ICON_RESOURCE "${CMAKE_SOURCE_DIR}/appicon.rc")
endif()

# A fresh Windows installation cannot be assumed to have the Visual C++
# redistributable. Bundle the matching CRT beside the executable instead of
# relying on a developer-machine installation.
if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_bonv_msvc_redist_arch x64)
    else()
        set(_bonv_msvc_redist_arch x86)
    endif()

    set(BONV_MSVC_REDIST_DIR "")
    if(DEFINED ENV{VCToolsRedistDir}
            AND EXISTS "$ENV{VCToolsRedistDir}/${_bonv_msvc_redist_arch}/Microsoft.VC143.CRT/vcruntime140.dll")
        set(BONV_MSVC_REDIST_DIR "$ENV{VCToolsRedistDir}/${_bonv_msvc_redist_arch}/Microsoft.VC143.CRT")
    else()
        file(GLOB _bonv_msvc_redist_candidates
            "$ENV{ProgramFiles}/Microsoft Visual Studio/*/*/VC/Redist/MSVC/*/${_bonv_msvc_redist_arch}/Microsoft.VC143.CRT")
        list(SORT _bonv_msvc_redist_candidates COMPARE NATURAL ORDER DESCENDING)
        foreach(_bonv_msvc_redist_candidate IN LISTS _bonv_msvc_redist_candidates)
            if(EXISTS "${_bonv_msvc_redist_candidate}/vcruntime140.dll")
                set(BONV_MSVC_REDIST_DIR "${_bonv_msvc_redist_candidate}")
                break()
            endif()
        endforeach()
        unset(_bonv_msvc_redist_candidate)
        unset(_bonv_msvc_redist_candidates)
    endif()

    file(GLOB BONV_MSVC_RUNTIME_DLLS "${BONV_MSVC_REDIST_DIR}/*.dll")
    if(NOT BONV_MSVC_REDIST_DIR OR NOT BONV_MSVC_RUNTIME_DLLS)
        message(FATAL_ERROR "MSVC runtime DLLs were not found. Install the Visual Studio C++ x64 redistributable components before packaging.")
    endif()
    unset(_bonv_msvc_redist_arch)
endif()

qt_add_executable(BonVGroundStation
    app/main.cpp
    app/AppBackend.h
    app/AppBackend.cpp
    app/JoystickController.h
    app/JoystickController.cpp
    app/KeyManager.h
    app/KeyManager.cpp
    app/ScreenRecorder.h
    app/ScreenRecorder.cpp
    ${QML_FILES}
    ${BONV_APP_ICON_RESOURCE}
    resources.qrc
)

if(WIN32)
    set_target_properties(BonVGroundStation PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

if(MSVC)
    target_compile_options(BonVGroundStation PRIVATE /FS)
endif()

# Link Qt libraries
target_link_libraries(BonVGroundStation PRIVATE
    MavlinkPOCCore
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::QuickDialogs2
    Qt6::Network
    Qt6::SerialPort
    Qt6::Location
    Qt6::Positioning
    Qt6::Multimedia
    Qt6::WebEngineQuick
    Qt6::WebEngineCore
    Qt6::Svg
)

if(WIN32)
    target_link_libraries(BonVGroundStation PRIVATE winmm)
endif()

# The deployment may have been moved or removed since configuration.
# Create it before any of the post-build runtime copy commands.
add_custom_command(TARGET BonVGroundStation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BONV_DEPLOY_DIR}"
    COMMENT "Creating runtime deployment directory"
    VERBATIM
)

if(MSVC)
    add_custom_command(TARGET BonVGroundStation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${BONV_MSVC_RUNTIME_DLLS}
                "${BONV_DEPLOY_DIR}"
        COMMENT "Copying MSVC runtime next to BonVGroundStation"
    )
endif()

if(GDAL_FOUND)
    add_executable(RasterImportWorker
        tools/RasterImportWorker.cpp
        core/map/RasterOverlayStorage.cpp
    )
    target_include_directories(RasterImportWorker PRIVATE
        ${CMAKE_SOURCE_DIR}/core/map
    )
    target_link_libraries(RasterImportWorker PRIVATE
        Qt6::Core
        Qt6::Gui
        GDAL::GDAL
    )
    foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(TOLOWER "${_config}" _config_lower)
        set_target_properties(RasterImportWorker PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/${_config_lower}/raster_import_worker"
        )
    endforeach()
    add_dependencies(BonVGroundStation RasterImportWorker)
endif()

if(BONV_GSTREAMER_ROOT AND BONV_HAS_GSTREAMER)
    # set(_gst_runtime_output "$<TARGET_FILE_DIR:BonVGroundStation>")
    set(_gst_runtime_output
        "${BONV_DEPLOY_DIR}")
    set(_gst_runtime_commands
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${BONV_GSTREAMER_ROOT}/bin"
                "${_gst_runtime_output}"
    )

    if(EXISTS "${BONV_GSTREAMER_ROOT}/lib/gstreamer-1.0")
        list(APPEND _gst_runtime_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_gst_runtime_output}/lib/gstreamer-1.0"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${BONV_GSTREAMER_ROOT}/lib/gstreamer-1.0"
                    "${_gst_runtime_output}/lib/gstreamer-1.0"
        )
    endif()

    if(EXISTS "${BONV_GSTREAMER_ROOT}/libexec/gstreamer-1.0")
        list(APPEND _gst_runtime_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_gst_runtime_output}/libexec/gstreamer-1.0"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${BONV_GSTREAMER_ROOT}/libexec/gstreamer-1.0"
                    "${_gst_runtime_output}/libexec/gstreamer-1.0"
        )
    endif()

    # Keep the bundled GStreamer configuration with the executable as well.
    # In particular, this includes the CA bundle needed by TLS-capable
    # GStreamer sources on machines without a separately installed SDK.
    if(EXISTS "${BONV_GSTREAMER_ROOT}/etc")
        list(APPEND _gst_runtime_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${_gst_runtime_output}/etc"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${BONV_GSTREAMER_ROOT}/etc"
                    "${_gst_runtime_output}/etc"
        )
    endif()

    add_custom_command(TARGET BonVGroundStation POST_BUILD
        ${_gst_runtime_commands}
        COMMENT "Copying bundled GStreamer runtime next to BonVGroundStation"
    )

    unset(_gst_runtime_commands)
    unset(_gst_runtime_output)
endif()

# ------------------------------------------------------------------
# Copy application runtime resources
# ------------------------------------------------------------------

add_custom_command(TARGET BonVGroundStation POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/media"
        "${BONV_DEPLOY_DIR}/media"

    COMMENT "Copying media resources"

)

if(WIN32)
    # find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    get_target_property(QMAKE_EXECUTABLE
    Qt6::qmake
    IMPORTED_LOCATION)

    get_filename_component(QT_BIN_DIR
        "${QMAKE_EXECUTABLE}"
        DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE

        NAMES windeployqt

        HINTS
            "${QT_BIN_DIR}"

        REQUIRED
    )
    find_package(OpenSSL 3 REQUIRED COMPONENTS SSL Crypto)

    # FindOpenSSL exposes .lib import libraries through TARGET_FILE on Windows.
    # Qt's OpenSSL TLS backend must instead receive the runtime DLLs.
    # get_filename_component(OPENSSL_LIBRARY_DIR "${SSL_EAY_RELEASE}" DIRECTORY)
    # get_filename_component(OPENSSL_INSTALL_DIR "${OPENSSL_LIBRARY_DIR}" DIRECTORY)
    # file(GLOB OPENSSL_RUNTIME_DLLS
    #     "${OPENSSL_INSTALL_DIR}/bin/libssl-*.dll"
    #     "${OPENSSL_INSTALL_DIR}/bin/libcrypto-*.dll"
    # )
    set(OPENSSL_INSTALL_DIR "C:/Users/Olivia-BV0054/Desktop/New_GCS/BonV_GroundControlStation/build/Desktop_Qt_6_10_1_MSVC2022_64bit-Debug/vcpkg_installed/x64-windows")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OPENSSL_RUNTIME_DIR "${OPENSSL_INSTALL_DIR}/debug/bin")
    else()
        set(OPENSSL_RUNTIME_DIR "${OPENSSL_INSTALL_DIR}/bin")
    endif()

    file(GLOB OPENSSL_RUNTIME_DLLS
        "${OPENSSL_RUNTIME_DIR}/libssl-*.dll"
        "${OPENSSL_RUNTIME_DIR}/libcrypto-*.dll"
    )
    if(NOT OPENSSL_RUNTIME_DLLS)
        message(FATAL_ERROR "OpenSSL runtime DLLs were not found in: ${OPENSSL_INSTALL_DIR}/bin")
    endif()

    file(GLOB QT_TLS_BACKEND_PLUGINS "${QT_BIN_DIR}/../plugins/tls/q*backend.dll")
    if(NOT QT_TLS_BACKEND_PLUGINS)
        message(FATAL_ERROR "Qt TLS backend plugins were not found next to the selected Qt kit: ${QT_BIN_DIR}/../plugins/tls")
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET BonVGroundStation POST_BUILD
            # COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            #         --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
            #         "$<TARGET_FILE:BonVGroundStation>"
            # Copy exe first
            COMMAND ${CMAKE_COMMAND} -E copy
                    "$<TARGET_FILE:BonVGroundStation>"
                    "${BONV_DEPLOY_DIR}"

            # Deploy Qt runtime

            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "--$<IF:$<CONFIG:Debug>,debug,release>"
                    --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
                    "${BONV_DEPLOY_DIR}/BonVGroundStation.exe"

            # Map tiles are fetched through Qt Network, which needs a TLS backend
            # and its OpenSSL 3 runtime on machines without a development Qt install.
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${BONV_DEPLOY_DIR}/tls"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${QT_TLS_BACKEND_PLUGINS}
                    "${BONV_DEPLOY_DIR}/tls"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${OPENSSL_RUNTIME_DLLS}
                    "${BONV_DEPLOY_DIR}"
            COMMENT "Deploying Qt runtime next to BonVGroundStation"
        )
        if(TARGET RasterImportWorker)
            add_custom_command(TARGET RasterImportWorker POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                        "$<TARGET_FILE:RasterImportWorker>"
                COMMENT "Deploying Qt runtime next to RasterImportWorker"
            )
        endif()
    else()
        message(WARNING "windeployqt was not found. Run it manually before launching BonVGroundStation outside a Qt environment.")
    endif()
endif()
add_subdirectory(tests)
# add_subdirectory(tools/raster_prep)

