/*
 * Copyright (c) 2025 David Marker <dave@freedave.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>


#ifndef __FREEDAVE_NET_RING_H_INCLUDED__
#	error "do not include ring_impl.h directly, include ring.h"
#endif

/*
 * Make sure we have a sane R_SZ
 * For now that is 16 or 32 with it being hard to justify 16.
 */
#if R_SZ != 16
#	if R_SZ != 32
#		error R_SZ must be 16 or 32
#	endif
#endif

/*
 * We have 2 types that change bitsize their variants are:
 *	`uin16_t`, `uint32_t`
 *	`struct ring16`, `struct ring32`
 */
#define MKTYPE(macro, z)	macro(z)

#define	_RING_SZTYPE(z)	uint ##z ##_t
#define UINTSZ_T	MKTYPE(_RING_SZTYPE, R_SZ)

#define _RING_RTYPE(z)	struct ring ##z
#define RINGSZ		MKTYPE(_RING_RTYPE, R_SZ)

/*
 * You can see why uint16_t will only save you 8 bytes here.
 * sizeof(UINTSZ_T) * 4 is either 8 or 16.
 */
RINGSZ {
	const UINTSZ_T		capacity;
	const UINTSZ_T		mask;
	struct {
		UINTSZ_T	start;
		UINTSZ_T	end;
	}			index;	/* group indices */
	struct {
		uint8_t	* const	data;
		uint8_t	* const	copy;	/* mapped right after data */
	}			maps;
};

/* for each functdion we do the 2 level trick as well */

#define	MKFUNCTION(macro, z, suffix)	macro(z, suffix)
#define _RING_FUNC(z, suffix) ring ##z ##_ ##suffix

#define RING_COUNT		MKFUNCTION(_RING_FUNC, R_SZ, count)
#define RING_FREE		MKFUNCTION(_RING_FUNC, R_SZ, free)
#define RING_FULL		MKFUNCTION(_RING_FUNC, R_SZ, full)
#define RING_EMPTY		MKFUNCTION(_RING_FUNC, R_SZ, empty)
#define RING_READ_BUFFER	MKFUNCTION(_RING_FUNC, R_SZ, read_buffer)
#define RING_WRITE_BUFFER	MKFUNCTION(_RING_FUNC, R_SZ, write_buffer)
#define RING_READ_ADVANCE	MKFUNCTION(_RING_FUNC, R_SZ, read_advance)
#define RING_WRITE_ADVANCE	MKFUNCTION(_RING_FUNC, R_SZ, write_advance)
#define RING_INIT		MKFUNCTION(_RING_FUNC, R_SZ, init)
#define RING_FINI		MKFUNCTION(_RING_FUNC, R_SZ, fini)


#ifdef TEST
#define RING_PEEK		MKFUNCTION(_RING_FUNC, R_SZ, peek)
#define RING_POKE		MKFUNCTION(_RING_FUNC, R_SZ, poke)
#endif

/*
 * This checks validity by making sure `rb` isn't null and has a capacity > 0,
 * a mask > 0 and using them verifies it is a power of 2.
 *
 * This doesn't check that capacity is a multiple of page size though.
 */
#ifdef NDEBUG
#	define SANITY_CHECK(rb)
#else
#	define SANITY_CHECK(rb) {			\
		assert(rb != NULL);			\
		assert(rb->capacity != 0);		\
		assert(rb->mask != 0);			\
		assert((rb->capacity & rb->mask) == 0);	\
	}
#endif

/*
 * ring[16|32]_count is the consumed space available to write from
 * Most other functions are counting on this one to check `ring[16|32]` to be
 * valid.
 */
static __inline UINTSZ_T
RING_COUNT(RINGSZ *rb)
{
	SANITY_CHECK(rb);

	UINTSZ_T count = (rb->index.end - rb->index.start);
	assert(count <= rb->capacity);

	return (count);
}

/* ring[16|32]_free is the available space to read into */
static __inline UINTSZ_T
RING_FREE(RINGSZ *rb)
{
	UINTSZ_T count = RING_COUNT(rb);
	return (rb->capacity - count);
}

static __inline bool
RING_FULL(RINGSZ *rb)
{
	UINTSZ_T count = RING_COUNT(rb);
	return (rb->capacity == count);
}

static __inline bool
RING_EMPTY(RINGSZ *rb)
{
	SANITY_CHECK(rb);
	return (rb->index.start == rb->index.end);
}

static __inline void *
RING_READ_BUFFER(RINGSZ *rb, size_t *nbytes)
{
	void *result;
	UINTSZ_T avail = RING_FREE(rb);

	result = (avail > 0) ? &rb->maps.data[rb->index.end & rb->mask] : NULL;
	if (nbytes != NULL)
		*nbytes = avail;

	return (result);
}

static __inline void *
RING_WRITE_BUFFER(RINGSZ *rb, size_t *nbytes)
{
	void *result;
	UINTSZ_T count = RING_COUNT(rb);

	result = (count > 0) ? &rb->maps.data[rb->index.start & rb->mask] : NULL;
	if (nbytes != NULL)
		*nbytes = count;

	return (result);
}

static __inline ssize_t
RING_READ_ADVANCE(RINGSZ *rb, ssize_t nread)
{
	SANITY_CHECK(rb);

	/* on a failed read we don't advance */
	if (nread == -1)
		return (nread);

	assert(nread <= rb->capacity);
	rb->index.end += nread;

	return (nread);
}

static __inline ssize_t
RING_WRITE_ADVANCE(RINGSZ *rb, ssize_t nwrit)
{
	SANITY_CHECK(rb);

	/* on a failed write we don't advance */
	if (nwrit == -1)
		return (nwrit);

	assert(nwrit <= rb->capacity);
	rb->index.start += nwrit;

	return (nwrit);
}


/*
 * The only 2 functions that aren't inline.
 *
 * For ring[16|32]_init you have to pass a `struct ring[16|32]` that will be
 * filled out and have memory mapped in for you. Much like MAP_ALIGNED for mmap,
 * the second argument to ring32_init is a binary logarithm of the number of
 * pages you want mapped. For a 4k page and R_SZ=32, valid values are [0,19].
 * For 4k page and R_SZ=16, valid values are [0,3].
 *
 * These will return -1 on failure and set `errno`, they don't assert.
 */
int	RING_INIT(RINGSZ *, uint8_t);
int	RING_FINI(RINGSZ *);


#ifdef TEST
/*
 * These two functions are intentionally using different pointers to show that
 * they are mapped to the same memory. Homage to Beagle Bros.
 */

static __inline uint8_t
RING_PEEK(RINGSZ *rb, UINTSZ_T idx)
{
	assert((idx & rb->mask) == idx);
	return rb->maps.data[idx & rb->mask];
}

/* whole point is to show that our copy effects the main data */
static __inline void
RING_POKE(RINGSZ *rb, UINTSZ_T idx, uint8_t val)
{
	assert((idx & rb->mask) == idx);
	rb->maps.copy[idx & rb->mask] = val;
}
#endif
