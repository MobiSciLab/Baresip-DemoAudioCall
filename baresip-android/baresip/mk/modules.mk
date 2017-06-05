#
# modules.mk
#
# Copyright (C) 2010 - 2017 Creytiv.com
#
# External libraries:
#
#   USE_ALSA          ALSA audio driver
#   USE_AMR           Adaptive Multi-Rate (AMR) audio codec
#   USE_AUDIOUNIT     AudioUnit audio driver for OSX/iOS
#   USE_AVCAPTURE     AVFoundation video capture for OSX/iOS
#   USE_AVCODEC       avcodec video codec module
#   USE_AVFORMAT      avformat video source module
#   USE_BV32          BroadVoice32 Wideband Audio codec
#   USE_CAIRO         Cairo module
#   USE_CONS          Console input driver
#   USE_COREAUDIO     MacOSX Coreaudio audio driver
#   USE_EVDEV         Event Device module
#   USE_G711          G.711 audio codec
#   USE_G722          G.722 audio codec
#   USE_G722_1        G.722.1 audio codec
#   USE_G726          G.726 audio codec
#   USE_GSM           GSM audio codec
#   USE_GST           Gstreamer 0.10 audio module
#   USE_GST1          Gstreamer 1.0 audio module
#   USE_GST_VIDEO     Gstreamer 0.10 video module
#   USE_GST_VIDEO1    Gstreamer 1.0 video module
#   USE_GTK           GTK+ user interface
#   USE_H265          H.265 video codec
#   USE_ILBC          iLBC audio codec
#   USE_ISAC          iSAC audio codec
#   USE_JACK          JACK Audio Connection Kit audio driver
#   USE_L16           L16 audio codec
#   USE_MPA           MPA audo codec
#   USE_MPG123        Use mpg123
#   USE_OMX_RPI       RaspberryPi VideoCore display driver
#   USE_OMX_BELLAGIO  libomxil-bellagio xvideosink driver
#   USE_OPUS          Opus audio codec
#   USE_OSS           OSS audio driver
#   USE_PLC           Packet Loss Concealment
#   USE_PORTAUDIO     Portaudio audio driver
#   USE_PULSE         Pulseaudio audio driver
#   USE_SDL           libSDL video output
#   USE_SILK          SILK (Skype) audio codec
#   USE_SNDFILE       sndfile wav dumper
#   USE_SPEEX         Speex audio codec
#   USE_SPEEX_AEC     Speex Acoustic Echo Canceller
#   USE_SPEEX_PP      Speex preprocessor
#   USE_SRTP          Secure RTP module using libre
#   USE_STDIO         stdio input driver
#   USE_SYSLOG        Syslog module
#   USE_V4L           Video4Linux module
#   USE_V4L2          Video4Linux2 module
#   USE_WINWAVE       Windows audio driver
#   USE_X11           X11 video output
#


# Default is enabled
MOD_AUTODETECT := 1

ifneq ($(MOD_AUTODETECT),)

USE_CONS  := 1
USE_G711  := 1
USE_L16   := 1

ifneq ($(OS),win32)

USE_ALSA  := $(shell [ -f $(SYSROOT)/include/alsa/asoundlib.h ] || \
	[ -f $(SYSROOT_ALT)/include/alsa/asoundlib.h ] && echo "yes")
USE_AMR   := $(shell [ -d $(SYSROOT)/include/opencore-amrnb ] || \
	[ -d $(SYSROOT_ALT)/include/opencore-amrnb ] || \
	[ -d $(SYSROOT)/local/include/amrnb ] || \
	[ -d $(SYSROOT)/include/amrnb ] && echo "yes")
USE_AVCODEC := $(shell [ -f $(SYSROOT)/include/libavcodec/avcodec.h ] || \
	[ -f $(SYSROOT)/local/include/libavcodec/avcodec.h ] || \
	[ -f $(SYSROOT)/include/$(MACHINE)/libavcodec/avcodec.h ] || \
	[ -f $(SYSROOT_ALT)/include/libavcodec/avcodec.h ] && echo "yes")
USE_AVFORMAT := $(shell [ -f $(SYSROOT)/include/libavformat/avformat.h ] || \
	[ -f $(SYSROOT)/local/include/libavformat/avformat.h ] || \
	[ -f $(SYSROOT)/include/$(MACHINE)/libavformat/avformat.h ] || \
	[ -f $(SYSROOT_ALT)/include/libavformat/avformat.h ] && echo "yes")
USE_BV32  := $(shell [ -f $(SYSROOT)/include/bv32/bv32.h ] || \
	[ -f $(SYSROOT)/local/include/bv32/bv32.h ] && echo "yes")
USE_CAIRO  := $(shell [ -f $(SYSROOT)/include/cairo/cairo.h ] || \
	[ -f $(SYSROOT)/local/include/cairo/cairo.h ] || \
	[ -f $(SYSROOT_ALT)/include/cairo/cairo.h ] && echo "yes")
USE_DTLS := $(shell [ -f $(SYSROOT)/include/openssl/dtls1.h ] || \
	[ -f $(SYSROOT)/local/include/openssl/dtls1.h ] || \
	[ -f $(SYSROOT_ALT)/include/openssl/dtls1.h ] && echo "yes")
USE_DTLS_SRTP := $(shell [ -f $(SYSROOT)/include/openssl/srtp.h ] || \
	[ -f $(SYSROOT)/local/include/openssl/srtp.h ] || \
	[ -f $(SYSROOT_ALT)/include/openssl/srtp.h ] && echo "yes")
USE_G722 := $(shell [ -f $(SYSROOT)/include/spandsp/g722.h ] || \
	[ -f $(SYSROOT_ALT)/include/spandsp/g722.h ] || \
	[ -f $(SYSROOT)/local/include/spandsp/g722.h ] && echo "yes")
USE_G722_1 := $(shell [ -f $(SYSROOT)/include/g722_1.h ] || \
	[ -f $(SYSROOT_ALT)/include/g722_1.h ] || \
	[ -f $(SYSROOT)/local/include/g722_1.h ] && echo "yes")
USE_G726 := $(shell [ -f $(SYSROOT)/include/spandsp/g726.h ] || \
	[ -f $(SYSROOT_ALT)/include/spandsp/g726.h ] || \
	[ -f $(SYSROOT)/local/include/spandsp/g726.h ] && echo "yes")
USE_GSM := $(shell [ -f $(SYSROOT)/include/gsm.h ] || \
	[ -f $(SYSROOT_ALT)/include/gsm.h ] || \
	[ -f $(SYSROOT)/include/gsm/gsm.h ] || \
	[ -f $(SYSROOT)/local/include/gsm.h ] || \
	[ -f $(SYSROOT)/local/include/gsm/gsm.h ] && echo "yes")
USE_GST := $(shell pkg-config --exists gstreamer-0.10 && echo "yes")
USE_GST1 := $(shell pkg-config --exists gstreamer-1.0 && echo "yes")
USE_GST_VIDEO := \
	$(shell pkg-config --exists gstreamer-0.10 gstreamer-app-0.10 \
		&& echo "yes")
USE_GST_VIDEO1 := $(shell pkg-config --exists gstreamer-1.0 gstreamer-app-1.0 \
		&& echo "yes")
USE_GTK := $(shell pkg-config 'gtk+-2.0 >= 2.22' && \
		   pkg-config 'glib-2.0 >= 2.32' && echo "yes")
ifneq ($(USE_AVCODEC),)
USE_H265  := $(shell [ -f $(SYSROOT)/include/x265.h ] || \
	[ -f $(SYSROOT)/local/include/x265.h ] || \
	[ -f $(SYSROOT_ALT)/include/x265.h ] && echo "yes")
endif
USE_ILBC := $(shell [ -f $(SYSROOT)/include/iLBC_define.h ] || \
	[ -f $(SYSROOT)/local/include/iLBC_define.h ] && echo "yes")
USE_ISAC := $(shell [ -f $(SYSROOT)/include/isac.h ] || \
	[ -f $(SYSROOT)/local/include/isac.h ] && echo "yes")
USE_JACK := $(shell [ -f $(SYSROOT)/include/jack/jack.h ] || \
	[ -f $(SYSROOT)/local/include/jack/jack.h ] && echo "yes")
USE_MPG123  := $(shell [ -f $(SYSROOT)/include/mpg123.h ] || \
	[ -f $(SYSROOT)/local/include/mpg123.h ] || \
	[ -f $(SYSROOT_ALT)/include/mpg123.h ] && echo "yes")
USE_OPUS := $(shell [ -f $(SYSROOT)/include/opus/opus.h ] || \
	[ -f $(SYSROOT_ALT)/include/opus/opus.h ] || \
	[ -f $(SYSROOT)/local/include/opus/opus.h ] && echo "yes")
USE_OSS := $(shell [ -f $(SYSROOT)/include/soundcard.h ] || \
	[ -f $(SYSROOT)/include/linux/soundcard.h ] || \
	[ -f $(SYSROOT)/include/sys/soundcard.h ] && echo "yes")
USE_PLC := $(shell [ -f $(SYSROOT)/include/spandsp/plc.h ] || \
	[ -f $(SYSROOT_ALT)/include/spandsp/plc.h ] || \
	[ -f $(SYSROOT)/local/include/spandsp/plc.h ] && echo "yes")
USE_PORTAUDIO := $(shell [ -f $(SYSROOT)/local/include/portaudio.h ] || \
		[ -f $(SYSROOT)/include/portaudio.h ] || \
		[ -f $(SYSROOT_ALT)/include/portaudio.h ] && echo "yes")
USE_PULSE := $(shell pkg-config --exists libpulse && echo "yes")
USE_SDL  := $(shell [ -f $(SYSROOT)/include/SDL/SDL.h ] || \
	[ -f $(SYSROOT)/local/include/SDL/SDL.h ] || \
	[ -f $(SYSROOT_ALT)/include/SDL/SDl.h ] && echo "yes")
USE_SDL2  := $(shell [ -f $(SYSROOT)/include/SDL2/SDL.h ] || \
	[ -f $(SYSROOT)/local/include/SDL2/SDL.h ] || \
	[ -f $(SYSROOT_ALT)/include/SDL2/SDl.h ] && echo "yes")
USE_SILK := $(shell [ -f $(SYSROOT)/include/silk/SKP_Silk_SDK_API.h ] || \
	[ -f $(SYSROOT_ALT)/include/silk/SKP_Silk_SDK_API.h ] || \
	[ -f $(SYSROOT)/local/include/silk/SKP_Silk_SDK_API.h ] && echo "yes")
USE_SNDFILE := $(shell [ -f $(SYSROOT)/include/sndfile.h ] || \
	[ -f $(SYSROOT_ALT)/include/sndfile.h ] || \
	[ -f $(SYSROOT_ALT)/usr/local/include/sndfile.h ] && echo "yes")
USE_STDIO := $(shell [ -f $(SYSROOT)/include/termios.h ] && echo "yes")
HAVE_SPEEXDSP := $(shell \
	[ -f $(SYSROOT)/local/lib/libspeexdsp$(LIB_SUFFIX) ] || \
	[ -f $(SYSROOT)/lib/libspeexdsp$(LIB_SUFFIX) ] || \
	[ -f $(SYSROOT_ALT)/lib/libspeexdsp$(LIB_SUFFIX) ] && echo "yes")
ifeq ($(HAVE_SPEEXDSP),)
HAVE_SPEEXDSP := \
	$(shell find $(SYSROOT)/lib -name libspeexdsp$(LIB_SUFFIX) 2>/dev/null)
endif
ifneq ($(USE_MPG123),)
ifneq ($(HAVE_SPEEXDSP),)
USE_MPA  := $(shell [ -f $(SYSROOT)/include/twolame.h ] || \
	[ -f $(SYSROOT)/local/include/twolame.h ] || \
	[ -f $(SYSROOT_ALT)/include/twolame.h ] && echo "yes")
endif
endif
USE_SPEEX := $(shell [ -f $(SYSROOT)/include/speex.h ] || \
	[ -f $(SYSROOT)/include/speex/speex.h ] || \
	[ -f $(SYSROOT)/local/include/speex.h ] || \
	[ -f $(SYSROOT)/local/include/speex/speex.h ] || \
	[ -f $(SYSROOT_ALT)/include/speex/speex.h ] && echo "yes")
USE_SPEEX_AEC := $(shell [ -f $(SYSROOT)/include/speex/speex_echo.h ] || \
	[ -f $(SYSROOT)/local/include/speex/speex_echo.h ] || \
	[ -f $(SYSROOT_ALT)/include/speex/speex_echo.h ] && echo "yes")
USE_SPEEX_PP := $(shell [ -f $(SYSROOT)/include/speex_preprocess.h ] || \
	[ -f $(SYSROOT)/local/include/speex_preprocess.h ] || \
	[ -f $(SYSROOT)/local/include/speex/speex_preprocess.h ] || \
	[ -f $(SYSROOT_ALT)/include/speex/speex_preprocess.h ] || \
	[ -f $(SYSROOT)/include/speex/speex_preprocess.h ] && echo "yes")
USE_SYSLOG := $(shell [ -f $(SYSROOT)/include/syslog.h ] || \
	[ -f $(SYSROOT_ALT)/include/syslog.h ] || \
	[ -f $(SYSROOT)/local/include/syslog.h ] && echo "yes")
USE_V4L  := $(shell [ -f $(SYSROOT)/include/libv4l1.h ] || \
	[ -f $(SYSROOT)/local/include/libv4l1.h ] \
	&& echo "yes")
HAVE_LIBV4L2 := $(shell [ -f $(SYSROOT)/include/libv4l2.h ] || \
	[ -f $(SYSROOT)/local/include/libv4l2.h ] \
	&& echo "yes")
USE_V4L2 := $(shell [ -f $(SYSROOT)/include/linux/videodev2.h ] || \
	[ -f $(SYSROOT)/local/include/linux/videodev2.h ] || \
	[ -f $(SYSROOT)/include/sys/videoio.h ] \
	&& echo "yes")
USE_X11 := $(shell [ -f $(SYSROOT)/include/X11/Xlib.h ] || \
	[ -f $(SYSROOT)/local/include/X11/Xlib.h ] || \
	[ -f $(SYSROOT_ALT)/include/X11/Xlib.h ] && echo "yes")
USE_ZRTP := $(shell [ -f $(SYSROOT)/include/libzrtp/zrtp.h ] || \
	[ -f $(SYSROOT)/local/include/libzrtp/zrtp.h ] || \
	[ -f $(SYSROOT_ALT)/include/libzrtp/zrtp.h ] && echo "yes")
USE_VPX  := $(shell [ -f $(SYSROOT)/include/vpx/vp8.h ] \
	|| [ -f $(SYSROOT)/local/include/vpx/vp8.h ] \
	|| [ -f $(SYSROOT_ALT)/include/vpx/vp8.h ] \
	&& echo "yes")
USE_OMX_RPI := $(shell [ -f /opt/vc/include/bcm_host.h ] || \
	[ -f $(SYSROOT)/include/bcm_host.h ] \
	|| [ -f $(SYSROOT_ALT)/include/bcm_host.h ] \
	&& echo "yes")
USE_OMX_BELLAGIO := $(shell [ -f /usr/include/OMX_Core.h ] \
	|| [ -f $(SYSROOT)/include/OMX_Core.h ] \
	|| [ -f $(SYSROOT_ALT)/include/OMX_Core.h ] \
	&& echo "yes")
else
# Windows.
# Accounts for mingw with Windows SDK (formerly known as Platform SDK)
# mounted at /winsdk
USE_DSHOW := $(shell [ -f /winsdk/Include/um/dshow.h ] && echo "yes")
endif

# Platform specific modules
ifeq ($(OS),darwin)
USE_COREAUDIO := yes
USE_OPENGL    := yes

USE_AVFOUNDATION := \
	$(shell [ -d /System/Library/Frameworks/AVFoundation.framework ] \
		&& echo "yes")

USE_AUDIOUNIT := \
	$(shell [ -d /System/Library/Frameworks/AudioUnit.framework ] \
		&& echo "yes")

ifneq ($(USE_AVFOUNDATION),)
USE_AVCAPTURE := yes
else
USE_QTCAPTURE := yes
endif


endif
ifeq ($(OS),linux)
USE_EVDEV := $(shell [ -f $(SYSROOT)/include/linux/input.h ] && echo "yes")
MODULES   += dtmfio
endif
ifeq ($(OS),win32)
USE_WINWAVE := yes
MODULES   += wincons
endif
ifeq ($(OS),openbsd)
MODULES   += sndio
endif
ifeq ($(OS),freebsd)
MODULES   += dtmfio
endif

ifneq ($(USE_GTK),)
USE_LIBNOTIFY := $(shell pkg-config 'libnotify glib-2.0 < 2.40' && echo "yes")
endif

endif

# ------------------------------------------------------------------------- #

MODULES   += $(EXTRA_MODULES)
MODULES   += stun turn ice natbd auloop presence
MODULES   += menu contact vumeter mwi account natpmp httpd
MODULES   += srtp
MODULES   += uuid
MODULES   += debug_cmd

ifneq ($(HAVE_PTHREAD),)
MODULES   += aubridge aufile
endif
ifneq ($(USE_VIDEO),)
MODULES   += vidloop selfview vidbridge
ifneq ($(HAVE_PTHREAD),)
MODULES   += fakevideo
endif
endif


ifneq ($(USE_ALSA),)
MODULES   += alsa
endif
ifneq ($(USE_AMR),)
MODULES   += amr
endif
ifneq ($(USE_AUDIOUNIT),)
MODULES   += audiounit
endif
ifneq ($(USE_AVCAPTURE),)
MODULES   += avcapture
endif
ifneq ($(USE_AVCODEC),)
MODULES   += avcodec
ifneq ($(USE_AVFORMAT),)
MODULES   += avformat
endif
endif
ifneq ($(USE_BV32),)
MODULES   += bv32
endif
ifneq ($(USE_CAIRO),)
MODULES   += cairo
MODULES   += vidinfo
ifneq ($(USE_MPG123),)
MODULES   += rst
endif
endif
ifneq ($(USE_CONS),)
MODULES   += cons
endif
ifneq ($(USE_COREAUDIO),)
MODULES   += coreaudio
endif
ifneq ($(USE_DTLS_SRTP),)
MODULES   += dtls_srtp
endif
ifneq ($(USE_QTCAPTURE),)
MODULES   += qtcapture
CFLAGS    += -DQTCAPTURE_RUNLOOP
endif
ifneq ($(USE_EVDEV),)
MODULES   += evdev
endif
ifneq ($(USE_G711),)
MODULES   += g711
endif
ifneq ($(USE_G722),)
MODULES   += g722
endif
ifneq ($(USE_G722_1),)
MODULES   += g7221
endif
ifneq ($(USE_G726),)
MODULES   += g726
endif
ifneq ($(USE_GSM),)
MODULES   += gsm
endif
ifneq ($(USE_GST),)
MODULES   += gst
endif
ifneq ($(USE_GST1),)
MODULES   += gst1
endif
ifneq ($(USE_GST_VIDEO),)
MODULES   += gst_video
endif
ifneq ($(USE_GST_VIDEO1),)
MODULES   += gst_video1
endif
ifneq ($(USE_GTK),)
MODULES   += gtk
endif
ifneq ($(USE_H265),)
MODULES   += h265
endif
ifneq ($(USE_ILBC),)
MODULES   += ilbc
endif
ifneq ($(USE_ISAC),)
MODULES   += isac
endif
ifneq ($(USE_JACK),)
MODULES   += jack
endif
ifneq ($(USE_L16),)
MODULES   += l16
endif
ifneq ($(USE_OPENGL),)
MODULES   += opengl
endif
ifneq ($(USE_OPENGLES),)
MODULES   += opengles
endif
ifneq ($(USE_OPUS),)
MODULES   += opus
endif
ifneq ($(USE_MPA),)
MODULES   += mpa
endif
ifneq ($(USE_OSS),)
MODULES   += oss
endif
ifneq ($(USE_PLC),)
MODULES   += plc
endif
ifneq ($(USE_PORTAUDIO),)
MODULES   += portaudio
endif
ifneq ($(USE_PULSE),)
MODULES   += pulse
endif
ifneq ($(USE_SDL),)
MODULES   += sdl
endif
ifneq ($(USE_SDL2),)
MODULES   += sdl2
endif
ifneq ($(USE_SILK),)
MODULES   += silk
endif
ifneq ($(USE_SNDFILE),)
MODULES   += sndfile
endif
ifneq ($(USE_SPEEX),)
MODULES   += speex
endif
ifneq ($(USE_SPEEX_AEC),)
MODULES   += speex_aec
endif
ifneq ($(USE_SPEEX_PP),)
MODULES   += speex_pp
endif
ifneq ($(USE_STDIO),)
MODULES   += stdio
endif
ifneq ($(USE_SYSLOG),)
MODULES   += syslog
endif
ifneq ($(USE_V4L),)
MODULES   += v4l
endif
ifneq ($(USE_V4L2),)
MODULES   += v4l2 v4l2_codec
endif
ifneq ($(USE_OMX_RPI),)
MODULES   += omx
endif
ifneq ($(USE_OMX_BELLAGIO),)
MODULES   += omx
endif
ifneq ($(USE_VPX),)
MODULES   += vp8
MODULES   += $(shell pkg-config 'vpx >= 1.3.0' && echo "vp9")
endif
ifneq ($(USE_WINWAVE),)
MODULES   += winwave
endif
ifneq ($(USE_X11),)
MODULES   += x11 x11grab
endif
ifneq ($(USE_ZRTP),)
MODULES   += zrtp
endif
ifneq ($(USE_DSHOW),)
MODULES   += dshow
endif
