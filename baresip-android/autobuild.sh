#!/usr/bin/env bash

#NOTE: Modified baresip/modules/avcodec/module.mk:
#$(MOD)_LFLAGS	+= -lvpx -lx264 -lavfilter -lswscale -lswresample -lavdevice -lavformat -lavcodec -lavutil


make clean
make zrtp

#make openssl
#make opus
make libbaresip.a

rm ../AndroidBaresipDemo/baresiplib/src/main/cpp/zrtp/lib/libzrtp.a
rm ../AndroidBaresipDemo/baresiplib/src/main/cpp/re/lib/libre.a
rm ../AndroidBaresipDemo/baresiplib/src/main/cpp/rem/lib/librem.a
rm ../AndroidBaresipDemo/baresiplib/src/main/cpp/baresip/lib/libbaresip.a


cp zrtp/third_party/bnlib/libbn.a ../AndroidBaresipDemo/baresiplib/src/main/cpp/zrtp/lib/libbn.a
cp zrtp/libzrtp.a ../AndroidBaresipDemo/baresiplib/src/main/cpp/zrtp/lib/libzrtp.a

cp baresip/libbaresip.a ../AndroidBaresipDemo/baresiplib/src/main/cpp/baresip/lib/libbaresip.a
cp re/libre.a ../AndroidBaresipDemo/baresiplib/src/main/cpp/re/lib/libre.a
cp rem/librem.a ../AndroidBaresipDemo/baresiplib/src/main/cpp/rem/lib/librem.a