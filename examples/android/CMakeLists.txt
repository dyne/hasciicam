cmake_minimum_required(VERSION 3.12)
project(hasciicam_android_sample LANGUAGES C CXX)

add_library(hasciicam_android_jni SHARED hasciicam_jni.cpp)
target_include_directories(hasciicam_android_jni PRIVATE ../../include)
target_link_libraries(hasciicam_android_jni PRIVATE hasciicam_core)
