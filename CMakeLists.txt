cmake_minimum_required(VERSION 3.21)

project(doludock
    VERSION 0.1.0
    DESCRIPTION "Hold WIN+Space to peek at a folder's contents over any window"
    HOMEPAGE_URL "https://github.com/DoluTattoo/doludock"
    LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "doludock targets Windows only (Win32 + Direct2D).")
endif()

# Default to Release for single-config generators (NMake, Ninja, Makefiles).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(DOLUDOCK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_executable(doludock WIN32)

target_sources(doludock PRIVATE
    src/main.cpp
    src/App.cpp
    src/FolderModel.cpp
    src/FolderWatcher.cpp
    src/KeyboardHook.cpp
    src/OverlayWindow.cpp
    src/Settings.cpp
    src/SettingsWindow.cpp
    src/app.rc
    src/app.manifest)

target_include_directories(doludock PRIVATE src)

target_compile_features(doludock PRIVATE cxx_std_17)

target_compile_definitions(doludock PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if(MSVC)
    target_compile_options(doludock PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(DOLUDOCK_WARNINGS_AS_ERRORS)
        target_compile_options(doludock PRIVATE /WX)
    endif()
endif()

# Windowing, Direct2D / DirectWrite / WIC rendering, and Shell integration.
target_link_libraries(doludock PRIVATE
    d2d1
    dwrite
    windowscodecs
    shell32
    shlwapi
    ole32
    user32
    gdi32
    dwmapi
    uxtheme
    comctl32
    shcore)
