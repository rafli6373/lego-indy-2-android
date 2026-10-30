cmake_minimum_required(VERSION 3.22.1)

project("legoindy2_native")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ANDROID_NDK)
    if(DEFINED CMAKE_ANDROID_NDK)
        set(ANDROID_NDK ${CMAKE_ANDROID_NDK})
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
    elseif(DEFINED ENV{ANDROID_NDK_ROOT})
        set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT})
    endif()
endif()

# Include Android native app glue
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)

add_library(
    legoindy2_native
    SHARED
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    native_main.cpp
    psp_renderer_gl.cpp
    psp_hle.cpp
    touch_controls.cpp
)

target_include_directories(
    legoindy2_native
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANDROID_NDK}/sources/android/native_app_glue
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles3-lib GLESv3)

target_link_libraries(
    legoindy2_native
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles3-lib}
)
