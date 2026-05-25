# Android host sample

This folder contains a minimal Android NDK host-shell scaffold.

## Purpose

Use the same shared core API while keeping Camera2/Android lifecycle logic in
the Android application layer.

## Files

- `hasciicam_jni.cpp`: JNI bridge between Java/Kotlin and HasciiCam core.
- `CMakeLists.txt`: native target wiring for Android builds.

## Build intent

This sample is designed for integration into an Android app module using
Gradle + CMake.

## Frame format

The JNI bridge currently accepts RGBA-style frame bytes and submits as:

- `HASCIICAM_PIXFMT_BGRA32`

The Java/Kotlin host can adapt this to NV12/NV21 later for direct Camera2
pipeline integration.
