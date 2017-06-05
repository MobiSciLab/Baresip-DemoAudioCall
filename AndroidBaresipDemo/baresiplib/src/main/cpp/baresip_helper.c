//
// Created by CaoMinhVu on 3/22/17.
//

#define DEBUG_MODULE 1

#include <string.h>
#include <stdlib.h>
#include "baresip_helper.h"
#include "baresipwrapper.h"

static const char* AUDIO_CODECS = "opus/48000/2,PCMU/8000/1";
static const char* VIDEO_CODECS = "h264";
static const char* DEFAULT_TRANSPORT = "tcp";
static const char* DEFAULT_ENC = "zrtp";
static const int DEFAULT_LIFETIME = 7200;

void android_log_msg(uint32_t level, const char *msg)
{
    const char delims[] = "\n";
    char* cpy = strdup(msg);
    char *line = strtok(cpy, delims);
    while(line != NULL){
        if (level > 2) {
            LOGE("%s", line);
        } else if (level == 2) {
            LOGW("%s", line);
        } else if (level == 1) {
            LOGI("%s", line);
        } else if (level == 0) {
            LOGD("%s", line);
        }
        line = strtok(NULL, delims);
    }
    free(cpy);
}

struct log android_log = {
        .le = { NULL, NULL, NULL, NULL},
        .h = &android_log_msg
};

struct ua* userAgent;

static ua_event_h *event_listener(struct ua *ua, enum ua_event ev, struct call *call, const char *prm,
                           void *arg){
    switch (ev) {
        case UA_EVENT_CALL_RINGING:
            break;
        case UA_EVENT_CALL_INCOMING:
            break;
        case UA_EVENT_CALL_PROGRESS:
            break;
        case UA_EVENT_CALL_ESTABLISHED:
            break;
        case UA_EVENT_CALL_CLOSED:
            break;
        case UA_EVENT_REGISTERING:
            break;
        case UA_EVENT_REGISTER_OK:
            break;
        case UA_EVENT_REGISTER_FAIL:
            break;
        default:
            break;
    }
    notifyEvent(ev);
    return EXIT_SUCCESS;
}

void init(const char* path) {
    LOGI("--------init--------");
    LOGI("Config path: %s", path);

    log_register_handler(&android_log);
    log_enable_debug(true);
    log_enable_info(true);
    log_enable_stderr(true);


    libre_init();
    conf_path_set(path);

    struct config* config = conf_config();
    config->call.local_timeout = 2*60*60*1000;
    config->call.max_calls = 1;
    config->avt.rtp_timeout = 2*60*60*1000;

    baresip_init(config, false);
    mod_init();
    conf_configure();
    conf_modules();

    struct player* player;
    play_init(&player);
    play_set_path(player, path);

    strcpy(config->audio.audio_path, path);

    ua_init("PandaTalk2", true, true, false, false);

    uag_event_register((ua_event_h *) event_listener, NULL);
}

long create_agent(const char* username, const char* password, const char* sip_server, const char* ice_server) {
    LOGI("--------create_agent--------");
    char aor[1024];
    memset(aor, 0, sizeof(aor));
    snprintf(aor, sizeof(aor),
             "<sip:%s:%s@%s;transport=%s>;regint=%d;regq=0.5;answermode=manual;audio_codecs=%s;video_codecs=%s;mediaenc=%s;medianat=ice;stunserver=stun:@%s",
             username, password,
             sip_server, DEFAULT_TRANSPORT, DEFAULT_LIFETIME,
             AUDIO_CODECS, VIDEO_CODECS,
             DEFAULT_ENC, ice_server

        );
    LOGI("AGENT: %s", aor);

    int err = ua_alloc(&userAgent, aor);
    if(err) {
        LOGI("ERROR: create_agent error! %d", err);
        return EXIT_FAILURE;
    }
    return (long) &userAgent;
}

void answer() {
    ua_answer(userAgent, ua_call(userAgent));
}

void decline() {
    ua_hangup(userAgent, ua_call(userAgent), UA_EVENT_CALL_CLOSED, NULL);
}

void start_audio_call(const char* peer) {
    ua_connect(userAgent, NULL, NULL, peer, NULL, VIDMODE_OFF);
}

void start_video_call(const char* peer) {
    ua_connect(userAgent, NULL, NULL, peer, NULL, VIDMODE_ON);

}

bool is_video_call() {
   return call_has_video(ua_call(userAgent));
}

void mute_video(bool isMuted) {
    video_mute(call_video(ua_call(userAgent)), isMuted);
}

void mute_audio(bool isMuted) {
    audio_mute(call_audio(ua_call(userAgent)), isMuted);
}

extern
long get_call_duration() {
    return call_duration(ua_call(userAgent));
}

extern
char* get_peer_name() {
    char* peer_name = call_peername(ua_call(userAgent));
    if (peer_name == NULL) {
        peer_name = call_peeruri(ua_call(userAgent));
    }
    return peer_name;
}

void start_daemon() {
    re_main(NULL);
    baresip_close();
    start_daemon();
}

void stop_daemon() {
    ua_close();
    mod_close();

    /* close and check for memory leaks */
    libre_close();
    tmr_debug();
    mem_debug();
}
