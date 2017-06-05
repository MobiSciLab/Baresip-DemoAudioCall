//
// Created by CaoMinhVu on 3/22/17.
//

#ifndef ANDROIDBARESIPDEMO_BARESIP_HELPER_H
#define ANDROIDBARESIPDEMO_BARESIP_HELPER_H

#include <re.h>
#include <rem.h>
#include <re_dbg.h>
#include <baresip.h>
#include <re.h>
#include <pthread.h>

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "android-log.h"

extern void init(const char* path);
extern void start_daemon();
extern long create_agent(const char* username, const char* password, const char* sip_server, const char* ice_server);
extern void start_audio_call(const char* peer);
extern void start_video_call(const char* peer);
extern bool is_video_call();
extern long get_call_duration();
extern void answer();
extern void decline();
extern void mute_audio(bool isMuted);
extern void mute_video(bool isMuted);
extern char* get_peer_name();

#endif //ANDROIDBARESIPDEMO_BARESIP_HELPER_H
