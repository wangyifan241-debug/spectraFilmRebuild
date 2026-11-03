cmake_minimum_required(VERSION 3.22)

# Also usable as add_subdirectory(windows_UI) after the engine defines
# spk_desktop_host. The standalone mode never modifies the engine checkout.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SpektraLabWindowsUI LANGUAGES C CXX)
    include(CTest)
    set(SPEKTRALAB_SOURCE_ROOT "" CACHE PATH "Source tree supplying engine/")
    if(NOT EXISTS "${SPEKTRALAB_SOURCE_ROOT}/engine/CMakeLists.txt")
        message(FATAL_ERROR "Set SPEKTRALAB_SOURCE_ROOT to the SpektraLab source tree")
    endif()
    set(SPEKTRALAB_BUILD_NATIVE_RAW ON CACHE BOOL "" FORCE)
    set(SPEKTRALAB_BUILD_DESKTOP ON CACHE BOOL "" FORCE)
    add_subdirectory("${SPEKTRALAB_SOURCE_ROOT}/engine" engine)
endif()

if(NOT TARGET spk_desktop_host)
    message(FATAL_ERROR "Build the engine's spk_desktop_host before windows_UI")
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

add_library(spk_material_library STATIC src/MaterialLibrary.hpp src/MaterialLibrary.cpp)
target_compile_features(spk_material_library PUBLIC cxx_std_20)
target_include_directories(spk_material_library PUBLIC src)
target_link_libraries(spk_material_library PUBLIC spk_desktop_host Qt6::Core)

# Offline measurement research only. Not linked into the interactive renderer
# and does not emit/install Profile files or depend on Python/scipy.
add_library(spk_scan_curve_fit STATIC material_fit/CurveFit.hpp material_fit/CurveFit.cpp
    material_fit/SpectralLift.hpp material_fit/SpectralLift.cpp)
target_compile_features(spk_scan_curve_fit PUBLIC cxx_std_20)
target_include_directories(spk_scan_curve_fit PUBLIC material_fit)
target_link_libraries(spk_scan_curve_fit PUBLIC Qt6::Core)
qt_add_executable(spk_material_fit material_fit/main.cpp)
target_link_libraries(spk_material_fit PRIVATE spk_scan_curve_fit)
set_target_properties(spk_material_fit PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")

qt_add_executable(SpektraLabQt WIN32
    src/main.cpp src/PreviewController.hpp src/PreviewController.cpp
    src/FrameOutput.hpp src/FrameOutput.cpp
    src/EditStore.hpp src/EditStore.cpp
    src/FrameCanvas.hpp src/FrameCanvas.cpp)
target_compile_features(SpektraLabQt PRIVATE cxx_std_20)
target_link_libraries(SpektraLabQt PRIVATE
    spk_desktop_host Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(WIN32)
    target_sources(SpektraLabQt PRIVATE assets/application.rc)
    target_include_directories(SpektraLabQt PRIVATE assets)
    target_link_libraries(SpektraLabQt PRIVATE gdi32 dwmapi)
endif()
qt_add_resources(SpektraLabQt "application_icon" PREFIX "/" FILES assets/film.ico)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(SpektraLabQt URI SpektraLab VERSION 1.0
    QML_FILES qml/Main.qml qml/Theme.qml qml/Section.qml qml/ActionButton.qml qml/ToneCurve.qml qml/SceneLatitude.qml qml/FilmFrame.qml qml/MaterialManager.qml
        qml/EditorSlider.qml qml/EditorComboBox.qml qml/EditorCheckBox.qml qml/EditorScrollBar.qml qml/StockBadge.qml
    NO_CACHEGEN)
set_target_properties(SpektraLabQt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")

# The one resource directory is explicitly adjacent to the executable. No
# lookup through another checkout and no Python in the application.
add_custom_target(spektralab_qt_resources ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<TARGET_FILE_DIR:spk_raw_render>/resources" "$<TARGET_FILE_DIR:SpektraLabQt>/resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<TARGET_FILE_DIR:spk_raw_render>/licenses" "$<TARGET_FILE_DIR:SpektraLabQt>/licenses"
    VERBATIM)
add_dependencies(spektralab_qt_resources spektralab_vulkan_resources)
add_dependencies(SpektraLabQt spektralab_qt_resources)
add_dependencies(SpektraLabQt spk_material_fit)

if(BUILD_TESTING)
    qt_add_executable(spk_highlight_reference_test tests/highlight_reference.cpp)
    target_link_libraries(spk_highlight_reference_test PRIVATE spk_desktop_host Qt6::Core)
    add_dependencies(spk_highlight_reference_test spektralab_qt_resources)
    add_test(NAME spk_highlight_reference COMMAND spk_highlight_reference_test "${CMAKE_BINARY_DIR}/app/resources")
    set_tests_properties(spk_highlight_reference PROPERTIES TIMEOUT 90)
    qt_add_executable(spk_qt_raster_input_test tests/raster_input.cpp src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_link_libraries(spk_qt_raster_input_test PRIVATE spk_material_library Qt6::Gui)
    add_dependencies(spk_qt_raster_input_test spektralab_qt_resources)
    add_test(NAME spk_qt_raster_input COMMAND spk_qt_raster_input_test "${CMAKE_BINARY_DIR}/app/resources" "${CMAKE_BINARY_DIR}/raster-input-evidence")
    set_tests_properties(spk_qt_raster_input PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        find_program(SPEKTRALAB_PWSH pwsh REQUIRED)
        add_test(NAME spk_payload_inventory COMMAND "${SPEKTRALAB_PWSH}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/payload_inventory.ps1" -BuildDirectory "${CMAKE_BINARY_DIR}")
    endif()
    qt_add_executable(spk_qt_review_memory_probe tests/review_memory.cpp src/FrameOutput.cpp)
    target_include_directories(spk_qt_review_memory_probe PRIVATE src)
    target_link_libraries(spk_qt_review_memory_probe PRIVATE spk_desktop_host Qt6::Gui)
    if(WIN32)
        target_link_libraries(spk_qt_review_memory_probe PRIVATE psapi)
    endif()
    qt_add_executable(spk_qt_review_controller_test tests/review_controller.cpp src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_link_libraries(spk_qt_review_controller_test PRIVATE spk_material_library Qt6::Gui)
    add_dependencies(spk_qt_review_controller_test spektralab_qt_resources)
    add_test(NAME spk_qt_review_controller COMMAND spk_qt_review_controller_test "${CMAKE_BINARY_DIR}/app/resources")
    set_tests_properties(spk_qt_review_controller PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(spk_qt_review_colour_test tests/review_colour.cpp src/FrameOutput.cpp
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/output.cpp"
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/image_io.cpp"
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/stb_impl.cpp"
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/platform.cpp"
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/white_balance.cpp"
        "${SPEKTRALAB_SOURCE_ROOT}/engine/host/tiff_ifd.cpp")
    target_include_directories(spk_qt_review_colour_test PRIVATE src "${SPEKTRALAB_SOURCE_ROOT}/engine" "${SPEKTRALAB_SOURCE_ROOT}/engine/third_party/stb")
    target_link_libraries(spk_qt_review_colour_test PRIVATE spk_desktop_host spk_libraw Qt6::Gui)
    add_test(NAME spk_qt_review_colour COMMAND spk_qt_review_colour_test "${CMAKE_BINARY_DIR}/app/resources")
    qt_add_executable(spk_qt_icon_test tests/application_icon.cpp)
    target_compile_features(spk_qt_icon_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_icon_test PRIVATE Qt6::Gui)
    add_dependencies(spk_qt_icon_test SpektraLabQt)
    add_test(NAME spk_qt_application_icon COMMAND spk_qt_icon_test "$<TARGET_FILE:SpektraLabQt>" "${CMAKE_CURRENT_SOURCE_DIR}/assets/film.ico")
    set_tests_properties(spk_qt_application_icon PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(spk_qt_responsiveness_test tests/responsiveness.cpp src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_link_libraries(spk_qt_responsiveness_test PRIVATE spk_material_library Qt6::Gui)
    add_dependencies(spk_qt_responsiveness_test spektralab_qt_resources)
    add_test(NAME spk_qt_responsiveness COMMAND spk_qt_responsiveness_test "${CMAKE_BINARY_DIR}/app/resources")
    set_tests_properties(spk_qt_responsiveness PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(spk_spectral_lift_test material_fit/spectral_tests.cpp)
    target_link_libraries(spk_spectral_lift_test PRIVATE spk_scan_curve_fit spk_material_library)
    add_dependencies(spk_spectral_lift_test spk_material_fit)
    add_test(NAME spk_spectral_lift COMMAND spk_spectral_lift_test "$<TARGET_FILE:spk_material_fit>")
    set_tests_properties(spk_spectral_lift PROPERTIES TIMEOUT 90)
    qt_add_executable(spk_material_fit_test material_fit/tests.cpp)
    target_link_libraries(spk_material_fit_test PRIVATE spk_scan_curve_fit spk_material_library)
    add_dependencies(spk_material_fit_test spk_material_fit)
    add_test(NAME spk_scan_curve_fit COMMAND spk_material_fit_test "${SPEKTRALAB_SOURCE_ROOT}/engine/resources_research/scan/ProImage100_datasheet_curves.json" "$<TARGET_FILE:spk_material_fit>" "${CMAKE_CURRENT_SOURCE_DIR}/material_fit/fixtures/proimage100-statusm-fit-v1.json")
    set_tests_properties(spk_scan_curve_fit PROPERTIES TIMEOUT 90)
    qt_add_executable(spk_qt_qml_workflow_test tests/qml_workflow.cpp)
    target_compile_features(spk_qt_qml_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_qml_workflow_test PRIVATE Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_qml_workflow_test SpektraLabQt)
    add_test(NAME spk_qt_qml_workflow COMMAND spk_qt_qml_workflow_test "$<TARGET_FILE:SpektraLabQt>")
    set_tests_properties(spk_qt_qml_workflow PROPERTIES TIMEOUT 200)
    qt_add_executable(spk_qt_stock_policy_test tests/stock_policy.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_stock_policy_test PRIVATE src)
    target_compile_features(spk_qt_stock_policy_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_stock_policy_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_stock_policy_test spektralab_qt_resources)
    add_test(NAME spk_qt_stock_policy COMMAND spk_qt_stock_policy_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    qt_add_executable(spk_qt_frame_output_test tests/frame_output.cpp src/FrameOutput.cpp)
    target_include_directories(spk_qt_frame_output_test PRIVATE src)
    target_compile_features(spk_qt_frame_output_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_frame_output_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_test(NAME spk_qt_frame_output COMMAND spk_qt_frame_output_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")

    qt_add_executable(spk_qt_library_workflow_test tests/library_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_library_workflow_test PRIVATE src)
    target_compile_features(spk_qt_library_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_library_workflow_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    qt_add_executable(spk_qt_edit_store_test tests/edit_store.cpp src/EditStore.cpp src/FrameOutput.cpp)
    target_include_directories(spk_qt_edit_store_test PRIVATE src)
    target_compile_features(spk_qt_edit_store_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_edit_store_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_test(NAME spk_qt_edit_store COMMAND spk_qt_edit_store_test)
    qt_add_executable(spk_qt_scene_placement_test tests/scene_placement.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_scene_placement_test PRIVATE src)
    target_compile_features(spk_qt_scene_placement_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_scene_placement_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_scene_placement_test spektralab_qt_resources)
    add_test(NAME spk_qt_scene_placement COMMAND spk_qt_scene_placement_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_scene_placement PROPERTIES TIMEOUT 120)
    qt_add_executable(spk_qt_film_frame_test tests/film_frame.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_film_frame_test PRIVATE src)
    target_compile_features(spk_qt_film_frame_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_film_frame_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_film_frame_test spektralab_qt_resources)
    add_test(NAME spk_qt_film_frame COMMAND spk_qt_film_frame_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_film_frame PROPERTIES TIMEOUT 120)
    qt_add_executable(spk_qt_half_pair_test tests/half_pair.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_half_pair_test PRIVATE src)
    target_compile_features(spk_qt_half_pair_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_half_pair_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_half_pair_test spektralab_qt_resources)
    add_test(NAME spk_qt_half_pair COMMAND spk_qt_half_pair_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_half_pair PROPERTIES TIMEOUT 180)
    qt_add_executable(spk_qt_open_material_test tests/open_material.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_open_material_test PRIVATE src)
    target_compile_features(spk_qt_open_material_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_open_material_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_dependencies(spk_qt_open_material_test spektralab_qt_resources)
    add_test(NAME spk_qt_open_material COMMAND spk_qt_open_material_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_open_material PROPERTIES TIMEOUT 120)
    qt_add_executable(spk_qt_material_library_test tests/material_library.cpp)
    target_link_libraries(spk_qt_material_library_test PRIVATE spk_material_library)
    add_dependencies(spk_qt_material_library_test spektralab_qt_resources)
    add_test(NAME spk_qt_material_library COMMAND spk_qt_material_library_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_material_library PROPERTIES TIMEOUT 120)
    qt_add_executable(spk_qt_material_workflow_test tests/material_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_link_libraries(spk_qt_material_workflow_test PRIVATE spk_material_library Qt6::Gui)
    add_dependencies(spk_qt_material_workflow_test spektralab_qt_resources)
    add_test(NAME spk_qt_material_workflow COMMAND spk_qt_material_workflow_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
    set_tests_properties(spk_qt_material_workflow PROPERTIES TIMEOUT 150)
    if(WIN32)
        # Specify each test's DLL search path: CTest on this Windows host can
        # restore the pre-launch PATH between tests. Ambient PATH alone makes
        # the second Qt test fail to load even when it passes in isolation.
        set(qt_test_runtime_path "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
        if(MINGW)
            get_filename_component(qt_test_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
            list(APPEND qt_test_runtime_path "PATH=path_list_prepend:${qt_test_compiler_directory}")
        endif()
        set_tests_properties(spk_highlight_reference spk_qt_raster_input spk_qt_review_controller spk_qt_review_colour spk_qt_application_icon spk_qt_responsiveness spk_spectral_lift spk_scan_curve_fit spk_qt_frame_output spk_qt_edit_store spk_qt_stock_policy spk_qt_qml_workflow spk_qt_scene_placement spk_qt_film_frame spk_qt_half_pair spk_qt_open_material spk_qt_material_library spk_qt_material_workflow PROPERTIES
            ENVIRONMENT_MODIFICATION "${qt_test_runtime_path}")
    endif()
    qt_add_executable(spk_qt_editing_workflow_test tests/editing_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_editing_workflow_test PRIVATE src)
    target_compile_features(spk_qt_editing_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_editing_workflow_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    # Full-RAW fixtures and a fresh output directory are explicit test arguments;
    # no absent-fixture skip can masquerade as a green CTest run.
endif()

# Controller translation units are shared by the app and isolated workflow
# executables. Link one implementation of material storage into each consumer.
foreach(consumer SpektraLabQt spk_qt_stock_policy_test spk_qt_library_workflow_test
        spk_qt_scene_placement_test spk_qt_film_frame_test spk_qt_half_pair_test
        spk_qt_open_material_test spk_qt_editing_workflow_test)
    if(TARGET ${consumer})
        target_link_libraries(${consumer} PRIVATE spk_material_library)
    endif()
endforeach()

if(BUILD_TESTING)
    add_executable(spk_display_capability_test tests/display_capability.cpp)
    target_compile_features(spk_display_capability_test PRIVATE cxx_std_20)
    if(WIN32)
        target_link_libraries(spk_display_capability_test PRIVATE user32)
    endif()
    add_test(NAME spk_display_capability COMMAND spk_display_capability_test)
endif()

if(BUILD_TESTING)
    add_executable(spk_float_transport_test tests/float_transport.cpp)
    target_compile_features(spk_float_transport_test PRIVATE cxx_std_20)
    target_include_directories(spk_float_transport_test PRIVATE "${SPEKTRALAB_SOURCE_ROOT}/engine/include")
    target_link_libraries(spk_float_transport_test PRIVATE spektralab_engine)
    add_dependencies(spk_float_transport_test spektralab_qt_resources)
    add_test(NAME spk_float_transport COMMAND spk_float_transport_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")
endif()
