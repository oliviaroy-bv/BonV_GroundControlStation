function(addGcsTest target)
    qt_add_executable(${target}
        ${ARGN}
    )

    target_link_libraries(${target} PRIVATE
        MavlinkPOCCore
        Qt6::Test
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /FS)
    endif()

    add_test(NAME ${target} COMMAND ${target})

    if(WIN32)
        set(_test_path_modification "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
        if(BONV_GSTREAMER_ROOT)
            string(APPEND _test_path_modification ";PATH=path_list_prepend:${BONV_GSTREAMER_ROOT}/bin")
        endif()
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT_MODIFICATION "${_test_path_modification}"
        )
        unset(_test_path_modification)
    endif()
endfunction()

addGcsTest(VideoTest
    VideoTest.cpp
)

addGcsTest(MavlinkTest
    MavlinkTest.cpp
)

addGcsTest(MavlinkInspectorModelTest
    MavlinkInspectorModelTest.cpp
)

addGcsTest(MavlinkForwarderTest
    MavlinkForwarderTest.cpp
)

addGcsTest(TerrainProviderTest
    TerrainProviderTest.cpp
)

if(WIN32 AND TARGET TerrainProviderTest AND BONV_GSTREAMER_ROOT)
    add_custom_command(TARGET TerrainProviderTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${BONV_GSTREAMER_ROOT}/bin"
                "$<TARGET_FILE_DIR:TerrainProviderTest>"
        COMMENT "Copying terrain test runtime DLLs"
    )
endif()

addGcsTest(VehicleTest
    VehicleTest.cpp
)

addGcsTest(VehicleManagerTest
    VehicleManagerTest.cpp
)
