cmake_minimum_required(VERSION 3.24)
project(stemboks VERSION 1.8.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(cmake/Mixxx.cmake)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg Network Concurrent Qml)

option(STEMBOKS_ONNX "Enable ONNX Runtime stem separation if found" ON)
if(STEMBOKS_ONNX)
  find_path(ONNX_INCLUDE onnxruntime_cxx_api.h
    PATHS /opt/homebrew/include/onnxruntime /usr/include/onnxruntime
          /usr/local/include/onnxruntime $ENV{ONNXRUNTIME_DIR}/include)
  find_library(ONNX_LIB onnxruntime
    PATHS /opt/homebrew/lib /usr/lib /usr/local/lib $ENV{ONNXRUNTIME_DIR}/lib)
endif()

set(SOURCES
  src/main.cpp
  src/control/ControlRegistry.cpp
  src/control/AppSettings.cpp
  src/engine/Decoder.cpp
  src/engine/Analysis.cpp
  src/engine/Effects.cpp
  src/engine/Deck.cpp
  src/engine/MixxxControlBridge.cpp
  src/engine/AudioEngine.cpp
  src/library/Library.cpp
  src/library/RekordboxUsb.cpp
  src/stems/StemManager.cpp
  src/stems/MixxxStemSource.cpp
  src/ui/MainWindow.cpp
  src/ui/DeckWidget.cpp
  src/ui/WaveformWidget.cpp
  src/ui/LibraryWidget.cpp
  src/ui/PrefsWidget.cpp
  src/ui/StemsManagerWidget.cpp
  src/ui/SecondScreen.cpp
  res/stemboks.qrc
  "${mixxx_SOURCE_DIR}/res/mixxx.qrc")

if(APPLE)
  add_executable(stemboks MACOSX_BUNDLE ${SOURCES})
  set_target_properties(stemboks PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME Stemboks
    MACOSX_BUNDLE_GUI_IDENTIFIER com.stemboks.stemboks
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
elseif(WIN32)
  add_executable(stemboks WIN32 ${SOURCES})
else()
  add_executable(stemboks ${SOURCES})
endif()

# Upstream headers precede ours on case-insensitive filesystems.
target_include_directories(stemboks PRIVATE
  "${mixxx_SOURCE_DIR}/src" "${mixxx_BINARY_DIR}/src" src ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(stemboks PRIVATE
  ${STEMBOKS_MIXXX_APP_LIBRARIES} Microsoft.GSL::GSL
  Qt6::Widgets Qt6::Sql Qt6::Svg Qt6::Network Qt6::Concurrent Qt6::Qml)
target_compile_definitions(stemboks PRIVATE
  MIXXX_USE_QOPENGL
  STEMBOKS_MIXXX_RESOURCE_DIR="${mixxx_SOURCE_DIR}/res")
if(ONNX_INCLUDE AND ONNX_LIB)
  target_include_directories(stemboks PRIVATE ${ONNX_INCLUDE} ${ONNX_INCLUDE}/..)
  target_link_libraries(stemboks PRIVATE ${ONNX_LIB})
  target_compile_definitions(stemboks PRIVATE STEMBOKS_HAVE_ONNX=1)
endif()

if(APPLE)
  set(STEMBOKS_RESOURCE_DEST "$<TARGET_BUNDLE_DIR:stemboks>/Contents/Resources/mixxx")
else()
  set(STEMBOKS_RESOURCE_DEST "$<TARGET_FILE_DIR:stemboks>/mixxx")
endif()
add_custom_command(TARGET stemboks POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${mixxx_SOURCE_DIR}/res" "${STEMBOKS_RESOURCE_DEST}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/res/controllers" "${STEMBOKS_RESOURCE_DEST}/controllers"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${mixxx_SOURCE_DIR}/LICENSE" "${STEMBOKS_RESOURCE_DEST}/LICENSE-Mixxx"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${STEMBOKS_RESOURCE_DEST}/LICENSE-Stemboks"
  VERBATIM)
install(TARGETS stemboks BUNDLE DESTINATION . RUNTIME DESTINATION bin)
install(DIRECTORY "${mixxx_SOURCE_DIR}/res/" DESTINATION share/stemboks/mixxx)
install(FILES "${mixxx_SOURCE_DIR}/LICENSE" DESTINATION share/stemboks RENAME LICENSE-Mixxx)

install(DIRECTORY res/controllers/ DESTINATION share/stemboks/mixxx/controllers)
