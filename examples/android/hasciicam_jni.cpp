#include <jni.h>

#include <hasciicam/hasciicam.h>

#include <string>

extern "C" JNIEXPORT jlong JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeCreate(JNIEnv *, jobject) {
    hasciicam_instance *instance = hasciicam_create();
    return reinterpret_cast<jlong>(instance);
}

extern "C" JNIEXPORT void JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeDestroy(JNIEnv *, jobject, jlong handle) {
    hasciicam_instance *instance = reinterpret_cast<hasciicam_instance *>(handle);
    if (instance != nullptr) {
        hasciicam_destroy(instance);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeStart(JNIEnv *, jobject,
                                                     jlong handle,
                                                     jint cameraWidth,
                                                     jint cameraHeight,
                                                     jint asciiWidth,
                                                     jint asciiHeight) {
    hasciicam_instance *instance = reinterpret_cast<hasciicam_instance *>(handle);
    if (instance == nullptr) {
        return JNI_FALSE;
    }
    return hasciicam_start_external(instance, cameraWidth, cameraHeight, asciiWidth, asciiHeight) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeSubmitFrame(JNIEnv *env,
                                                           jobject,
                                                           jlong handle,
                                                           jbyteArray bytes,
                                                           jint width,
                                                           jint height,
                                                           jint stride) {
    hasciicam_instance *instance = reinterpret_cast<hasciicam_instance *>(handle);
    jbyte *raw;
    jsize size;
    if (instance == nullptr || bytes == nullptr) {
        return JNI_FALSE;
    }
    size = env->GetArrayLength(bytes);
    raw = env->GetByteArrayElements(bytes, nullptr);
    if (raw == nullptr) {
        return JNI_FALSE;
    }
    {
        int ok = hasciicam_submit_frame(instance,
                                        reinterpret_cast<unsigned char *>(raw),
                                        static_cast<size_t>(size),
                                        width,
                                        height,
                                        stride,
                                        HASCIICAM_PIXFMT_BGRA32);
        env->ReleaseByteArrayElements(bytes, raw, JNI_ABORT);
        return ok ? JNI_TRUE : JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeRenderASCII(JNIEnv *env,
                                                           jobject,
                                                           jlong handle) {
    hasciicam_instance *instance = reinterpret_cast<hasciicam_instance *>(handle);
    const char *text = nullptr;
    int width = 0;
    int height = 0;
    if (instance == nullptr) {
        return nullptr;
    }
    if (!hasciicam_render_frame(instance)) {
        return nullptr;
    }
    if (!hasciicam_get_ascii_frame(instance, &text, nullptr, &width, &height) || text == nullptr) {
        return nullptr;
    }
    {
        std::string out(text, static_cast<size_t>(width * height));
        return env->NewStringUTF(out.c_str());
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_dyne_hasciicam_HasciiCamBridge_nativeStop(JNIEnv *, jobject, jlong handle) {
    hasciicam_instance *instance = reinterpret_cast<hasciicam_instance *>(handle);
    if (instance != nullptr) {
        hasciicam_stop(instance);
    }
}
