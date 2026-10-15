include(FetchContent)
find_package(Git REQUIRED)

set(GSL_TEST OFF CACHE BOOL "" FORCE)
set(GSL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ms_gsl
  URL https://codeload.github.com/microsoft/GSL/tar.gz/refs/tags/v4.2.0
  URL_HASH SHA256=2c717545a073649126cb99ebd493fa2ae23120077968795d2c69cbab821e4ac6)
FetchContent_MakeAvailable(ms_gsl)


# Keep the full upstream engine, readers, analysis, effects and controller stack.
FetchContent_Declare(mixxx
  URL https://codeload.github.com/mixxxdj/mixxx/tar.gz/527211c16adede822c262998dcb81089f81c87bf
  URL_HASH SHA256=2328b9fef4a237850c03013d276761eec668d183064d50b58728cae3efefcca3
  PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-space-change
    "${CMAKE_CURRENT_LIST_DIR}/mixxx-embedded.patch"
  SOURCE_SUBDIR stemboks-no-auto-configure)
FetchContent_MakeAvailable(mixxx)

set(QT6 ON CACHE BOOL "" FORCE)
set(QML OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(FFMPEG ON CACHE BOOL "" FORCE)
set(STEM ON CACHE BOOL "" FORCE)
set(PORTMIDI ON CACHE BOOL "" FORCE)
set(HID ON CACHE BOOL "" FORCE)
set(RUBBERBAND ON CACHE BOOL "" FORCE)
# These optional upstream integrations are not used by the Stembox interface.
foreach(feature ENGINEPRIME QTKEYCHAIN BROADCAST LILV VINYLCONTROL HSS1394 BATTERY MACOS_BUNDLE)
  set(${feature} OFF CACHE BOOL "" FORCE)
endforeach()
set(OPTIMIZE portable CACHE STRING "" FORCE)
if(APPLE AND NOT MIXXX_VCPKG_ROOT AND NOT BUILDENV_URL AND NOT DEFINED ENV{BUILDENV_URL})
  set(MIXXX_USE_SYSTEM_DEPS ON CACHE BOOL "" FORCE)
endif()

# Upstream still uses top-level roots for generated headers and version info.
# Limit those values to its directory so our application keeps its own identity.
function(stemboks_add_mixxx)
  set(CMAKE_REQUIRED_INCLUDES "${ms_gsl_SOURCE_DIR}/include")
  if(APPLE AND MIXXX_USE_SYSTEM_DEPS AND NOT QT_TRANSLATION_FILE)
    find_file(QT_TRANSLATION_FILE qt_de.qm
      PATHS /opt/homebrew/share/qt/translations /usr/local/share/qt/translations REQUIRED)
  endif()
  set(CMAKE_SOURCE_DIR "${mixxx_SOURCE_DIR}")
  set(CMAKE_BINARY_DIR "${mixxx_BINARY_DIR}")
  set(GIT_DESCRIBE "2.6.0-beta-527211c")
  set(GIT_COMMIT_DATE "2026-10-14T18:46:54+02:00")
  set(CMAKE_PROJECT_NAME mixxx)
  set(CMAKE_PROJECT_VERSION 2.6.0)
  set(CMAKE_PROJECT_VERSION_MAJOR 2)
  set(CMAKE_PROJECT_VERSION_MINOR 6)
  set(CMAKE_PROJECT_VERSION_PATCH 0)
  add_subdirectory("${mixxx_SOURCE_DIR}" "${mixxx_BINARY_DIR}" EXCLUDE_FROM_ALL)
  get_target_property(mixxx_app_libraries mixxx LINK_LIBRARIES)
  set(STEMBOKS_MIXXX_APP_LIBRARIES "${mixxx_app_libraries}" PARENT_SCOPE)
endfunction()
stemboks_add_mixxx()
set_property(TARGET mixxx-lib PROPERTY INTERFACE_PRECOMPILE_HEADERS "")

target_link_libraries(rendergraph_gl PUBLIC Microsoft.GSL::GSL)

target_include_directories(mixxx-gitinfostore PUBLIC "${mixxx_BINARY_DIR}/src")
