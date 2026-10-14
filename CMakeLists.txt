cmake_minimum_required(VERSION 3.21)
project(stemboks VERSION 1.8.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(STEMBOKS_ONNX "Enable ONNX Runtime stem separation if found" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg Network Concurrent)
find_package(PkgConfig QUIET)

# FFmpeg (libav*)
set(FFMPEG_LIBS "")
foreach(lib avformat avcodec avutil swresample)
  if(PkgConfig_FOUND)
    pkg_check_modules(FF_${lib} lib${lib})
  endif()
  if(FF_${lib}_FOUND)
    list(APPEND FFMPEG_LIBS ${FF_${lib}_LIBRARIES})
    include_directories(${FF_${lib}_INCLUDE_DIRS})
    link_directories(${FF_${lib}_LIBRARY_DIRS})
  else()
    find_library(FFLIB_${lib} ${lib} REQUIRED)
    list(APPEND FFMPEG_LIBS ${FFLIB_${lib}})
    find_path(FFINC_${lib} lib${lib} PATH_SUFFIXES include)
    if(FFINC_${lib})
      include_directories(${FFINC_${lib}})
    endif()
  endif()
endforeach()

# RtMidi + hidapi via FetchContent (small, builds everywhere incl. Linux ARM)
include(FetchContent)
FetchContent_Declare(rtmidi
  GIT_REPOSITORY https://github.com/thestk/rtmidi.git
  GIT_TAG 6.0.0)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rtmidi)

find_package(hidapi QUIET)
if(NOT hidapi_FOUND)
  FetchContent_Declare(hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG hidapi-0.14.0)
  if(UNIX AND NOT APPLE)
    set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
  endif()
  FetchContent_MakeAvailable(hidapi)
endif()

# ONNX Runtime (optional: stem separation)
set(HAVE_ONNX OFF)
if(STEMBOKS_ONNX)
  find_path(ONNX_INCLUDE onnxruntime_cxx_api.h
    PATHS /opt/homebrew/include/onnxruntime /usr/include/onnxruntime
          /usr/local/include/onnxruntime $ENV{ONNXRUNTIME_DIR}/include)
  find_library(ONNX_LIB onnxruntime
    PATHS /opt/homebrew/lib /usr/lib /usr/local/lib $ENV{ONNXRUNTIME_DIR}/lib)
  if(ONNX_INCLUDE AND ONNX_LIB)
    set(HAVE_ONNX ON)
    message(STATUS "ONNX Runtime found: ${ONNX_LIB}")
  endif()
endif()

set(SOURCES
  src/main.cpp
  src/control/ControlRegistry.cpp
  src/control/AppSettings.cpp
  src/engine/Decoder.cpp
  src/engine/Analysis.cpp
  src/engine/TimeStretch.cpp
  src/engine/Effects.cpp
  src/engine/Deck.cpp
  src/engine/AudioEngine.cpp
  src/library/Library.cpp
  src/library/RekordboxUsb.cpp
  src/stems/StemManager.cpp
  src/controllers/ControllerManager.cpp
  src/ui/MainWindow.cpp
  src/ui/DeckWidget.cpp
  src/ui/WaveformWidget.cpp
  src/ui/LibraryWidget.cpp
  src/ui/PrefsWidget.cpp
  src/ui/StemsManagerWidget.cpp
  src/ui/SecondScreen.cpp
  res/stemboks.qrc
)

if(APPLE)
  add_executable(stemboks MACOSX_BUNDLE ${SOURCES})
  set_target_properties(stemboks PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Stemboks"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.stemboks.stemboks"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
elseif(WIN32)
  add_executable(stemboks WIN32 ${SOURCES})
else()
  add_executable(stemboks ${SOURCES})
endif()

target_include_directories(stemboks PRIVATE src third_party)
target_link_libraries(stemboks PRIVATE
  Qt6::Widgets Qt6::Sql Qt6::Svg Qt6::Network Qt6::Concurrent
  ${FFMPEG_LIBS} rtmidi)
if(TARGET hidapi::hidapi)
  target_link_libraries(stemboks PRIVATE hidapi::hidapi)
elseif(TARGET hidapi)
  target_link_libraries(stemboks PRIVATE hidapi)
else()
  message(FATAL_ERROR "hidapi did not provide a supported target")
endif()
if(HAVE_ONNX)
  target_include_directories(stemboks PRIVATE ${ONNX_INCLUDE} ${ONNX_INCLUDE}/..)
  target_link_libraries(stemboks PRIVATE ${ONNX_LIB})
  target_compile_definitions(stemboks PRIVATE STEMBOKS_HAVE_ONNX=1)
endif()

if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(stemboks PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
if(APPLE)
  target_link_libraries(stemboks PRIVATE
    "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
endif()

install(TARGETS stemboks BUNDLE DESTINATION . RUNTIME DESTINATION bin)
install(DIRECTORY res/mappings/ DESTINATION share/stemboks/mappings FILES_MATCHING PATTERN "*.mapping")
