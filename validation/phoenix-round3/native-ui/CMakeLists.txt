cmake_minimum_required(VERSION 3.25)
project(PhoenixRound3Diagnostics LANGUAGES C CXX)
include(CTest)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
get_filename_component(SPEKTRALAB_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(SPEKTRALAB_BUILD_NATIVE_RAW ON CACHE BOOL "" FORCE)
set(SPEKTRALAB_BUILD_DESKTOP ON CACHE BOOL "" FORCE)
add_subdirectory("${SPEKTRALAB_SOURCE_ROOT}/engine" engine)
add_subdirectory("${SPEKTRALAB_SOURCE_ROOT}/windows_UI" ui)
set(CMAKE_AUTOMOC ON)
qt_add_executable(phoenix_round3_ui candidate-ui.cpp
 "${SPEKTRALAB_SOURCE_ROOT}/windows_UI/src/PreviewController.hpp"
 "${SPEKTRALAB_SOURCE_ROOT}/windows_UI/src/PreviewController.cpp"
 "${SPEKTRALAB_SOURCE_ROOT}/windows_UI/src/FrameOutput.cpp"
 "${SPEKTRALAB_SOURCE_ROOT}/windows_UI/src/EditStore.cpp")
target_link_libraries(phoenix_round3_ui PRIVATE spk_material_library Qt6::Gui)
