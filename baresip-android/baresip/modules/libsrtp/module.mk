#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= libsrtp
$(MOD)_SRCS	+= srtp.c sdes.c
$(MOD)_LFLAGS	+= -lsrtp

include mk/mod.mk
