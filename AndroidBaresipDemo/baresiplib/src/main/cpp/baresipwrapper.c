//
// Created by CaoMinhVu on 3/20/17.
//

#include <stdlib.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "baresipwrapper.h"
#include "baresip_helper.h"


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = vm;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }
    g_ctx.done = 0;
    return  JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_init(JNIEnv *env, jobject  instance, jstring jstrPath) {
    const char *path = (*env)->GetStringUTFChars(env, jstrPath, 0);

    init(path);

    jclass clz = (*env)->GetObjectClass(env, instance);
    g_ctx.TelephonyServiceClz = (*env)->NewGlobalRef(env, clz);
    g_ctx.TelephonyServiceObj = (*env)->NewGlobalRef(env, instance);
    g_ctx.onMessageCallback = (*env)->GetMethodID(env, g_ctx.TelephonyServiceClz,
                                            "onEvent", "(I)V");

    (*env)->ReleaseStringUTFChars(env, jstrPath, path);

}

JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_startDaemon(JNIEnv *env, jobject instance) {
    start_daemon();
}

JNIEXPORT jlong JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_createAgent(JNIEnv *env, jobject instance,
                                                                 jstring username_,
                                                                 jstring password_,
                                                                 jstring sipServer_,
                                                                 jstring iceServer_) {
    const char *username = (*env)->GetStringUTFChars(env, username_, 0);
    const char *password = (*env)->GetStringUTFChars(env, password_, 0);
    const char *sipServer = (*env)->GetStringUTFChars(env, sipServer_, 0);
    const char *iceServer = (*env)->GetStringUTFChars(env, iceServer_, 0);

    long agent = create_agent(username, password, sipServer, iceServer);

    (*env)->ReleaseStringUTFChars(env, username_, username);
    (*env)->ReleaseStringUTFChars(env, password_, password);
    (*env)->ReleaseStringUTFChars(env, sipServer_, sipServer);
    (*env)->ReleaseStringUTFChars(env, iceServer_, iceServer);

    return agent;
}



extern
int notifyEvent(int event) {
    JavaVM *javaVM = g_ctx.javaVM;
    JNIEnv *env;

    jint res = (*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);
        if (JNI_OK != res) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            return EXIT_FAILURE;
        }
    }

    (*env)->CallVoidMethod(env, g_ctx.TelephonyServiceObj, g_ctx.onMessageCallback, event);

//    (*javaVM)->DetachCurrentThread(javaVM);
}

JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_answer(JNIEnv *env, jobject instance, jlong userAgent_) {

    struct ua * user_agent = (struct ua*) userAgent_;
    answer();

}

JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_decline(JNIEnv *env, jobject instance, jlong userAgent_) {

    struct ua * user_agent = (struct ua*) userAgent_;
    decline();
}

JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_start_1audio_1call(JNIEnv *env,
                                                                        jobject instance,
                                                                        jstring to_) {
    const char *to = (*env)->GetStringUTFChars(env, to_, 0);

    start_audio_call(to);

    (*env)->ReleaseStringUTFChars(env, to_, to);
}

JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_start_1video_1call(JNIEnv *env,
                                                                        jobject instance,
                                                                        jstring to_) {
    const char *to = (*env)->GetStringUTFChars(env, to_, 0);

    start_video_call(to);

    (*env)->ReleaseStringUTFChars(env, to_, to);
}

JNIEXPORT jboolean JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_is_1video_1call(JNIEnv *env,
                                                                     jobject instance) {
    return is_video_call();
}






JNIEXPORT jlong JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_get_1call_1duration(JNIEnv *env,
                                                                         jobject instance) {

    return get_call_duration();

}









JNIEXPORT void JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_mute_1audio(JNIEnv *env, jobject instance,
                                                             jboolean isMuted) {

    mute_audio(isMuted);

}

JNIEXPORT jstring JNICALL
Java_net_edge_1works_baresiplib_TelephonyService_get_1peer_1name(JNIEnv *env, jobject instance) {

    return (*env)->NewStringUTF(env, get_peer_name());
}