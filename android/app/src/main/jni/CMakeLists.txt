cmake_minimum_required(VERSION 3.13)

# Define the library name here.
project(appmodules)

# This file includes all the necessary to let you build your React Native application
include(${REACT_ANDROID_DIR}/cmake-utils/ReactNative-application.cmake)

# Define where the additional source code lives. We need to crawl back the jni, main, src, app, android folders
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ../../../../../shared/NativeSampleModule.cpp)

# Search for Android system libs
find_library(log-lib log)
find_library(android-lib android)

# Add Skia library and include paths
set(SKIA_ROOT ${CMAKE_SOURCE_DIR}/../../../../../third_party/skia)
set(SKIA_INCLUDE_DIR
  ${SKIA_ROOT}
  ${SKIA_ROOT}/include
)

# ABI에 따라 올바른 libskia.a 경로 선택
if(ANDROID_ABI STREQUAL "armeabi-v7a")
  set(SKIA_LIB ${SKIA_ROOT}/android/armeabi-v7a/libskia.a)
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
  set(SKIA_LIB ${SKIA_ROOT}/android/arm64-v8a/libskia.a)
elseif(ANDROID_ABI STREQUAL "x86")
  set(SKIA_LIB ${SKIA_ROOT}/android/x86/libskia.a)
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(SKIA_LIB ${SKIA_ROOT}/android/x86_64/libskia.a)
else()
  message(FATAL_ERROR "not supported ABI: ${ANDROID_ABI}")
endif()

# Define where CMake can find the additional header files. We need to crawl back the jni, main, src, app, android folders
# Include Skia headers
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  ../../../../../shared
  ${SKIA_INCLUDE_DIR}
)

# Link static libs
target_link_libraries(${CMAKE_PROJECT_NAME}
  ${SKIA_LIB}
  ${log-lib}
  ${android-lib}
)
