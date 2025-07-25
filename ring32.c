/*
 * Copyright (c) 2025 David Marker <dave@freedave.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

/* This is the file that actually implements everything. ring16.c just includes
 * this file. By default with no R_SZ specified you get 32 (which is probably
 * what you want anyway).
 */
#ifndef R_SZ
#	define R_SZ	32
#endif

#define __FREEDAVE_NET_RING_H_INCLUDED__
#include "ring_impl.h"

/*
 * For a `struct ring16` and `uint16_t` we can't accurately represent a capacity
 * of the max size. Because its 2^16 - 1. That breaks our rules that we must be
 * a power of 2 as well as a multiple of page size (itself a power of 2).
 *
 * That means `lgpages` can only be in range [0,3] a small enoug set to say for
 * 4k pages what the outcomes of valid `lgpage` are:
 *	lgpages = 0 -> 1 * 4k =  4k
 *	lgpages = 1 -> 2 * 4k =  8k
 *	lgpages = 2 -> 4 * 4k = 16k
 *	lgpages = 3 -> 8 * 4k = 32k
 *
 * Similar story for `struct ring32` and `uint32_t` but your range is better, its
 * now [0,19]. And 19 gives you 2gb for a buffer size which is already excessive
 * for a ring buffer.
 *
 * The formula for your buffer size 2^(lgpagesz + lgpages). Usually you have
 * 4k pages which is 2^12. So your size will typically be 2^(12 + lgpages) no
 * matter if its uint16_t or uint32_t. The more important value to know is the
 * page size.
 *
 * Finally, in theory, for `struct ring64` and `uint64_t`, for a range of [0,51]
 * which would allow you a max of 2tb ... which would be insane! I have not and
 * have no intention of testing this size. ring_impl.h arbitrarily doesn't allow
 * it. Its justs theoretically possible to use a uint64_t to index into a byte
 * array. But something may be wrong if you need ring buffers greater than 2gb.
 *
 * uint8_t would not work, it can't even index a full page.
 *
 * A more interesting enhancement would be to switch to shm_create_largepage if
 * the size worked out to be some multiple of a larger page. That still wouldn't
 * require uint64_t.
 *
 * Who knows, maybe that eventually makes sense.
 */
int
RING_INIT(RINGSZ *rb, uint8_t lgpages)
{
	int shm;
	UINTSZ_T pagesz = (UINTSZ_T) getpagesize();
	int lgpagesz = ffsl(pagesz) - 1; /* for MAP_ALIGNED */

	UINTSZ_T capacity;
	uint8_t	*data, *copy;


	if (rb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Make sure we can actually handle the size.
	 * This may seem overly complicated but it works for all possible R_SZ
	 * without having to change anything.
	 */
	if ((lgpagesz + lgpages) > (8 * sizeof(rb->capacity) - 1)) {
		errno = EDOM;
		return (-1);
	}

	/* the test above guaranteed this would fit */
	capacity = 1 << (lgpages + lgpagesz);

	shm = shm_open(SHM_ANON, O_RDWR | O_EXCL | O_CREAT, 0600);
	if (shm == -1)
		return (-1);	/* shm_open set errno */

	/* we only need ring->capacity as we map that same region twice */
	if (ftruncate(shm, (off_t)capacity) == -1) {
		/* shouldn't happen per man page but in case that changes */
		return (-1);
	}

	data = mmap(
		0, capacity,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ALIGNED(lgpagesz),
		shm, (off_t)0
	);
	if (data == MAP_FAILED) {
		int save_err = errno; /* mmap set */
		(void) close(shm);
		errno = save_err;
		return (-1);
	}

	/*
	 * NOTE: it doesn't appear, even though we use shm_open(2), that we are
	 *       required to go touch all the pages to force allocations. But if
	 *       we did, this is where we would. (By writing a '0' to one byte
	 *       of each page of `data`).
	 */

	/* force the next address but map same data */
	copy = mmap(
		data + capacity, capacity,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_FIXED,
		shm, (off_t)0
	);
	if (copy == MAP_FAILED) {
		int save_err = errno;
		(void) munmap(data, capacity);
		(void) close(shm);
		errno = save_err;
		return (-1);
	}
	close(shm); /* no longer needed */

	/*
	 * kind of annoying way to initialize rb by memcpy, all part of the
	 * `const compromise` for structure members.
	 */
	RINGSZ initializer = {
		.capacity = capacity,
		.mask = capacity - 1,
		.index = { .start = 0, .end = 0 },
		.maps = { .data = data, .copy = copy },
	};
	memcpy(rb, &initializer, sizeof(*rb));

	return (0);
}


int
RING_FINI(RINGSZ *rb)
{

	if (rb == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (rb->capacity == 0) {
		errno = ENXIO;
		return (-1);
	}
	(void) munmap(rb->maps.copy, rb->capacity);
	(void) munmap(rb->maps.data, rb->capacity);
	bzero(rb, sizeof(*rb));

	return (0);
}
