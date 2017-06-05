# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)


#-fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# build application's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

set(distribution_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)

link_directories(${distribution_DIR}/opus/lib)

message(distribution_DIR)

add_definitions("-DDEBUG_LEVEL=1")
add_definitions("-DARCH=\"arm\"")
add_definitions("-DHAVE_NEON=1")
add_definitions("-DANDROID=1")
add_definitions("-DHAVE_INTTYPES_H")
add_definitions("-fPIC")
add_definitions("-mfpu=neon")

add_definitions("-DHAVE_POLL")
add_definitions("-DHAVE_SELECT")
add_definitions("-DHAVE_SELECT_H")
add_definitions("-DHAVE_PTHREAD")
add_definitions("-DHAVE_STRERROR_R")
add_definitions("-DHAVE_INTTYPES_H")
add_definitions("-DHAVE_NET_ROUTE_H")
add_definitions("-DHAVE_INET6")
add_definitions("-DHAVE_SYSLOG")
add_definitions("-DHAVE_FORK")
add_definitions("-DHAVE_INET_NTOP")
add_definitions("-DHAVE_PWD_H")
add_definitions("-DHAVE_POLL")
add_definitions("-DHAVE_SELECT")
add_definitions("-DHAVE_SELECT_H")
add_definitions("-DHAVE_SETRLIMIT")
add_definitions("-DHAVE_SIGNAL")
add_definitions("-DHAVE_SYS_TIME_H")
add_definitions("-DHAVE_EPOLL")
add_definitions("-DHAVE_UNISTD_H")
add_definitions("-DHAVE_UNAME")
add_definitions("-DHAVE_STRINGS_H")
add_definitions("-DHAVE_GAI_STRERROR")
add_definitions("-DHAVE_UNAME")
add_definitions("-DHAVE_SPEEXDSP")

add_definitions("-DUSE_OPENSSL_DTLS")
add_definitions("-DUSE_DTLS")
add_definitions("-DUSE_OPENSSL_SRTP")
add_definitions("-DUSE_DTLS_SRTP")
add_definitions("-DUSE_ZLIB")
add_definitions("-DUSE_X264")
add_definitions("-DUSE_G711")
add_definitions("-DUSE_AVFORMAT")
add_definitions("-DUSE_SPEEX_AEC")
add_definitions("-DUSE_KISS_FFT")
add_definitions("-DUSE_ZRTP")

add_definitions("-DZRTP_DEBUG")
add_definitions("-DZRTP_DEBUG_SRTP_KEYS")


add_library(libssl STATIC IMPORTED)
set_target_properties(libssl PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/openssl/lib/libssl.a)

add_library(libcrypto STATIC IMPORTED)
set_target_properties(libcrypto PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/openssl/lib/libcrypto.a)

add_library(libre STATIC IMPORTED)
set_target_properties(libre PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/re/lib/libre.a)

add_library(librem STATIC IMPORTED)
set_target_properties(librem PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/rem/lib/librem.a)

add_library(libbn STATIC IMPORTED)
set_target_properties(libbn PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/zrtp/lib/libbn.a)

add_library(libzrtp STATIC IMPORTED)
set_target_properties(libzrtp PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/zrtp/lib/libzrtp.a)

add_library(libavcodec STATIC IMPORTED)
set_target_properties(libavcodec PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libavcodec.a)

add_library(libavdevice STATIC IMPORTED)
set_target_properties(libavdevice PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libavdevice.a)

add_library(libavfilter STATIC IMPORTED)
set_target_properties(libavfilter PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libavfilter.a)

add_library(libavformat STATIC IMPORTED)
set_target_properties(libavformat PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libavformat.a)

add_library(libavutil STATIC IMPORTED)
set_target_properties(libavutil PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libavutil.a)

add_library(libswresample STATIC IMPORTED)
set_target_properties(libswresample PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libswresample.a)

add_library(libswscale STATIC IMPORTED)
set_target_properties(libswscale PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/ffmpeg/lib/libswscale.a)

add_library(libx264 STATIC IMPORTED)
set_target_properties(libx264 PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/x264/lib/libx264.a)

add_library(libscalerarm STATIC IMPORTED)
set_target_properties(libscalerarm PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/scalerarm/lib/libscalerarm.a)

add_library(libspeexdsp STATIC IMPORTED)
set_target_properties(libspeexdsp PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/speexdsp/lib/libspeexdsp.a)

add_library(libbaresip STATIC IMPORTED)
set_target_properties(libbaresip PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/baresip/lib/libbaresip.a)


add_library( baresipwrapper
             SHARED
             src/main/cpp/baresip_helper.c
             src/main/cpp/baresipwrapper.c
              )

target_include_directories(baresipwrapper PRIVATE
                           ${distribution_DIR}/re/include
                           ${distribution_DIR}/rem/include
                           ${distribution_DIR}/opus/include
                           ${distribution_DIR}/zrtp/third_party/bgaes
                           ${distribution_DIR}/zrtp/third_party/bnlib
                           ${distribution_DIR}/zrtp/include
                           ${distribution_DIR}/ffmpeg/include
                           ${distribution_DIR}/x264/include
                           ${distribution_DIR}/speexdsp/include
                           ${distribution_DIR}/baresip/include
                           )

target_link_libraries( baresipwrapper

                       libbaresip
                       libzrtp
                       libbn
                       libre
                       librem

                       opus
                       libspeexdsp
                       libx264
                       libavformat
                       libavdevice
                       libavfilter
                       libswscale
                       libavcodec
                       libavutil
                       libswresample

                       libssl
                       libcrypto

                       libscalerarm

                       log
                       dl
                       android
                       OpenSLES
                       EGL
                       GLESv1_CM
                       GLESv2
                       z
                      )