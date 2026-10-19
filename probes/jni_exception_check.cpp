#include "core/middleware/jni_exceptions.hpp"
#include <stdexcept>
extern "C" JNIEXPORT void JNICALL Java_JniExceptionCheck_invoke(JNIEnv *env, jclass, jint mode) {
    mcvr::jni::invoke(env, "external-boundary-check", [&] {
        if (mode == 1) throw std::runtime_error("vkWaitForFences failed (VK_ERROR_DEVICE_LOST, -4)");
        if (mode == 2) throw 42;
        if (mode == 3) {
            jclass type = env->FindClass("java/lang/IllegalStateException");
            env->ThrowNew(type, "existing Java exception");
            env->DeleteLocalRef(type);
            throw std::runtime_error("secondary native exception");
        }
    });
}
