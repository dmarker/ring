/*
 * Copyright (c) 2025 David Marker <dave@freedave.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Yes this could have been done in the makefile by compiling with `-DR_SZ=16`
 * but I haven't figured out how to get the FreeBSD /usr/share/mk build system
 * to compile a file twice with different flags.
 *
 * If you know that's a pull request I'll definitely merge!
 *
 * This doesn't get installed anyway so its not a big deal.
 */

#ifdef R_SZ
#undef R_SZ
#endif
#define R_SZ	16

#include "ring32.c"
