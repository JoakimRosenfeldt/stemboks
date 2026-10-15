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
option(STEMBOKS_REQUIRE_ONNX "Fail configuration if ONNX Runtime separation is unavailable" OFF)
set(ONNXRUNTIME_DIR "$ENV{ONNXRUNTIME_DIR}" CACHE PATH "ONNX Runtime SDK directory")
set(STEMBOKS_HAVE_ONNX OFF)
if(STEMBOKS_ONNX)
  if(ONNXRUNTIME_DIR)
    unset(ONNX_INCLUDE CACHE)
    unset(ONNX_LIB CACHE)
    find_path(ONNX_INCLUDE onnxruntime_cxx_api.h
      PATHS "${ONNXRUNTIME_DIR}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    find_library(ONNX_LIB onnxruntime
      PATHS "${ONNXRUNTIME_DIR}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
  else()
    find_path(ONNX_INCLUDE onnxruntime_cxx_api.h
      PATHS /opt/homebrew/include/onnxruntime /usr/include/onnxruntime /usr/local/include/onnxruntime)
    find_library(ONNX_LIB onnxruntime
      PATHS /opt/homebrew/lib /usr/lib /usr/local/lib)
  endif()
  if(ONNX_INCLUDE AND ONNX_LIB)
    set(STEMBOKS_HAVE_ONNX ON)
  endif()
endif()
if(STEMBOKS_REQUIRE_ONNX AND NOT STEMBOKS_HAVE_ONNX)
  message(FATAL_ERROR "ONNX Runtime is required. Set ONNXRUNTIME_DIR to the CPU SDK directory.")
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
if(STEMBOKS_HAVE_ONNX)
  target_include_directories(stemboks PRIVATE ${ONNX_INCLUDE} ${ONNX_INCLUDE}/..)
  target_link_libraries(stemboks PRIVATE ${ONNX_LIB})
  target_compile_definitions(stemboks PRIVATE STEMBOKS_HAVE_ONNX=1)
  message(STATUS "ONNX Runtime separation: ${ONNX_LIB}")
  if(ONNXRUNTIME_DIR)
    if(APPLE)
      get_filename_component(ONNX_RUNTIME_LIBRARY "${ONNX_LIB}" REALPATH)
      set(ONNX_RUNTIME_FILES "${ONNX_RUNTIME_LIBRARY}")
      set(ONNX_RUNTIME_DEST "$<TARGET_BUNDLE_DIR:stemboks>/Contents/Frameworks")
      set(ONNX_INSTALL_DEST "stemboks.app/Contents/Frameworks")
      set(ONNX_LICENSE_DEST "$<TARGET_BUNDLE_DIR:stemboks>/Contents/Resources/licenses/onnxruntime")
      set(ONNX_LICENSE_INSTALL_DEST "stemboks.app/Contents/Resources/licenses/onnxruntime")
      set_property(TARGET stemboks APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
      set_property(TARGET stemboks APPEND PROPERTY INSTALL_RPATH "@executable_path/../Frameworks")
    elseif(WIN32)
      file(GLOB ONNX_RUNTIME_FILES "${ONNXRUNTIME_DIR}/lib/onnxruntime*.dll")
      set(ONNX_RUNTIME_DEST "$<TARGET_FILE_DIR:stemboks>")
      set(ONNX_INSTALL_DEST bin)
      set(ONNX_LICENSE_DEST "$<TARGET_FILE_DIR:stemboks>/licenses/onnxruntime")
      set(ONNX_LICENSE_INSTALL_DEST bin/licenses/onnxruntime)
    else()
      file(GLOB ONNX_RUNTIME_FILES
        "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so.*"
        "${ONNXRUNTIME_DIR}/lib/libonnxruntime_providers_shared.so")
      set(ONNX_RUNTIME_DEST "$<TARGET_FILE_DIR:stemboks>/lib")
      set(ONNX_INSTALL_DEST bin/lib)
      set(ONNX_LICENSE_DEST "$<TARGET_FILE_DIR:stemboks>/licenses/onnxruntime")
      set(ONNX_LICENSE_INSTALL_DEST bin/licenses/onnxruntime)
      set_property(TARGET stemboks APPEND PROPERTY BUILD_RPATH "$ORIGIN/lib")
      set_property(TARGET stemboks APPEND PROPERTY INSTALL_RPATH "$ORIGIN/lib")
    endif()
    if(NOT ONNX_RUNTIME_FILES)
      message(FATAL_ERROR "ONNX Runtime SDK has no shared runtime libraries: ${ONNXRUNTIME_DIR}")
    endif()
    add_custom_command(TARGET stemboks POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${ONNX_RUNTIME_DEST}" "${ONNX_LICENSE_DEST}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${ONNX_RUNTIME_FILES} "${ONNX_RUNTIME_DEST}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${ONNXRUNTIME_DIR}/LICENSE" "${ONNXRUNTIME_DIR}/ThirdPartyNotices.txt" "${ONNX_LICENSE_DEST}"
      VERBATIM)
    install(FILES ${ONNX_RUNTIME_FILES} DESTINATION "${ONNX_INSTALL_DEST}")
    install(FILES "${ONNXRUNTIME_DIR}/LICENSE" "${ONNXRUNTIME_DIR}/ThirdPartyNotices.txt"
      DESTINATION "${ONNX_LICENSE_INSTALL_DEST}")
  endif()
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
