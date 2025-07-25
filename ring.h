/*
 * Copyright (c) 2025 David Marker <dave@freedave.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * This provides a uint16_t and a uint32_t version. The uint16_t is just more
 * compact and has drastically fewer size options: [0,3]
 * While the uint32_t version has size option of [0,19]
 *
 * Two structures are provided but should be treated as opaque:
 *	struct ring16
 *	struct ring32
 *
 * The following static inline functions are provided for uint16_t:
 *	uint16_t	 ring16_count(struct ring16 *rb);
 *	uint16_t	 ring16_free(struct ring16 *rb);
 *	bool		 ring16_full(struct ring16 *rb);
 *	bool		 ring16_empty(struct ring16 *rb);
 *	void		*ring16_read_buffer(struct ring16 *rb, size_t *nbytes)
 *	void		*ring16_write_buffer(struct ring16 *rb, size_t *nbytes);
 *	ssize_t		 ring16_read_advance(struct ring16 *rb, ssize_t nread);
 *	ssize_t		 ring16_write_advance(struct ring16 *rb, ssize_t nwrit);
 * And the same set for uint32_t:
 *	uint32_t	 ring32_count(struct ring32 *rb);
 *	uint32_t	 ring32_free(struct ring32 *rb);
 *	bool		 ring32_full(struct ring32 *rb);
 *	bool		 ring32_empty(struct ring32 *rb);
 *	void		*ring32_read_buffer(struct ring32 *rb, size_t *nbytes)
 *	void		*ring32_write_buffer(struct ring32 *rb, size_t *nbytes);
 *	ssize_t		 ring32_read_advance(struct ring32 *rb, ssize_t nread);
 *	ssize_t		 ring32_write_advance(struct ring32 *rb, ssize_t nwrit);
 *
 * You also need the non-inline functions to create / destroy, either:
 *	int	ring16_init(struct ring16 *rb, uint8_t logpages);
 *	int	ring16_fini(struct ring16 *rb);
 * Or:
 *	int	ring32_init(struct ring32 *rb, uint8_t logpages);
 *	int	ring32_fini(struct ring32 *);
 * Or both.
 *
 * To get those you compile ring.c with R_SZ defined as either 16 or 32. If
 * you want both you have to compile the source twice.
 * 
 * Including this header gets you all the inlines and prototypes. So its only
 * the ring16_init and ring16_fini functions that you will be missing if you
 * don't build ring.c without R_SZ set to 16 (it defaults to 32).
 *
 * What is this? Its a ring buffer that maps the same memory twice in a row.
 * That makes for a ring buffer that you can always read as much space is
 * available in the buffer in a single contiguous read. Likewise you can
 * always write everything in the buffer as if it is one contiguous buffer.
 *
 * You do NOT need to use readv(2) or writev(2) ever. Nor do you need recvmsg(2)
 * or sendmsg(2) either. For a pretend 4 byte buffer mapped twice it would look
 * like this:
 *
 *	+---+---+---+---+---+---+---+---+
 *	| A | B | C | D | A | B | C | D |
 *	+---+---+---+---+---+---+---+---+
 *	  ^               ^
 *	  |               |
 *	 data           copy
 *
 * Even if you start at C and read 3 bytes its going to work out because of the
 * double mapping. Same thing happens when writing. You just have to start in
 * `data`. You can't index into `copy` and expect it to work.
 *
 * This means you have to have buffers that are multiples of the page size
 * (probably 4k) not just 4 bytes.
 *
 * These are only suitable for a single threaded (like kqueue(2) event loop)
 * application.
 */
#ifndef __FREEDAVE_NET_RING_H__
#define __FREEDAVE_NET_RING_H__

/*
 * If you only need one size (hint: its 32), just define before including
 * ring.h
 */
#define	__FREEDAVE_NET_RING_H_INCLUDED__
#ifdef	R_SZ
#	include <ring_impl.h>
#else
#	define	R_SZ	16
#	include <ring_impl.h>
#	undef	R_SZ
#	define	R_SZ	32
#	include <ring_impl.h>
#endif
#undef	__FREEDAVE_NET_RING_H_INCLUDED__

#endif /* __FREEDAVE_NET_RING_H__ */
