#
# Copyright (c) 2025 David Marker <dave@freedave.net>
#
# SPDX-License-Identifier: BSD-2-Clause
#
#

LIBDIR= /usr/local/lib
INCSDIR= /usr/local/include

PACKAGE=lib${LIB}
LIB=            ring

SHLIB_MAJOR=0
WITHOUT_PROFILE=1
MK_DEBUG_FILES= no

SRCS=	ring16.c ring32.c
INCS=	ring.h ring_impl.h

# TODO: make tests
# TODO: make man page

.include <bsd.lib.mk>
