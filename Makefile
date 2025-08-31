#
# Copyright (c) 2025 David Marker <dave@freedave.net>
#
# SPDX-License-Identifier: BSD-2-Clause
#
#

LOCALBASE?=/usr/local
LIBDIR= ${LOCALBASE}/lib
INCSDIR= ${LOCALBASE}/include
SHAREDIR=${LOCALBASE}/share

PACKAGE=lib${LIB}
LIB=            ring

SHLIB_MAJOR=0
WITHOUT_PROFILE=1
MK_DEBUG_FILES= no

SRCS=	ring.c
INCS=	ring.h

DIRS+=	MAN3
MAN3=	${MANDIR}3

MAN=	ring.3

# TODO: make tests

.include <bsd.lib.mk>
