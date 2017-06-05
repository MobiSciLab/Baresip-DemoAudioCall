//
// Created by CaoMinhVu on 3/20/17.
//

#ifndef ANDROIDBARESIP_BARESIPWRAPPER_H
#define ANDROIDBARESIP_BARESIPWRAPPER_H


#include <string.h>
#include <pthread.h>
#include <android/log.h>
#include <android/../jni.h>
#include <assert.h>
#include "android-log.h"

typedef struct Baresip_context {
    JavaVM  *javaVM;
    jclass   TelephonyServiceClz;
    jobject  TelephonyServiceObj;
    jmethodID  onMessageCallback;
    pthread_mutex_t  lock;
    int      done;
} Baresip_Context;
Baresip_Context g_ctx;

extern int notifyEvent(int event);
extern int startCamera();
extern int stopCamera();

#endif //ANDROIDBARESIP_BARESIPWRAPPER_H
