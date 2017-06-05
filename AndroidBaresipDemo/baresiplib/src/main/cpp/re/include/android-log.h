//
// Created by Cao Minh Vu on 12/13/16.
//

#ifndef BARESIPWRAPPER_ANDROID_LOG_H
#define BARESIPWRAPPER_ANDROID_LOG_H

#include <android/log.h>

#define DEBUG TRUE
static const char* kTAG = "CMV";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))
#define LOGD(...) \
  ((void)__android_log_print(ANDROID_LOG_DEBUG, kTAG, __VA_ARGS__))

#endif //BARESIPWRAPPER_ANDROID_LOG_H
