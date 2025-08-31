/*
 * Copyright (c) 2025 David Marker <dave@freedave.net>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __FREEDAVE_NET_RING_H__
#define __FREEDAVE_NET_RING_H__

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>


/*
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
 * (probably 4k) not just 4 bytes. Additionally they must be powers of 2.
 *
 * These are only suitable for a single threaded (like kqueue(2) event loop)
 * application. See buf_ring(9) for multi producer/consumer scenarios.
 */

struct ring {
	const uint32_t		capacity;
	const uint32_t		mask;
	struct {
		uint32_t	start;
		uint32_t	end;
	}			index;	/* group indices */
	struct {
		uint8_t	* const	data;
		uint8_t	* const	copy;	/* mapped right after data */
	}			maps;
};

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
 * ring_count is the consumed space available to write from. Most other
 * functions are counting on this one to check `ring` to be
 * valid.
 */
static __inline uint32_t
ring_count(struct ring *rb)
{
	SANITY_CHECK(rb);

	uint32_t count = (rb->index.end - rb->index.start);
	assert(count <= rb->capacity);

	return (count);
}

/* ring_free is the available space to read into */
static __inline uint32_t
ring_free(struct ring *rb)
{
	uint32_t count = ring_count(rb);
	return (rb->capacity - count);
}

static __inline bool
ring_full(struct ring *rb)
{
	uint32_t count = ring_count(rb);
	return (rb->capacity == count);
}

static __inline bool
ring_empty(struct ring *rb)
{
	SANITY_CHECK(rb);
	return (rb->index.start == rb->index.end);
}

static __inline void *
ring_read_buffer(struct ring *rb, size_t *nbytes)
{
	void *result;
	uint32_t avail = ring_free(rb);

	result = (avail > 0) ? &rb->maps.data[rb->index.end & rb->mask] : NULL;
	if (nbytes != NULL)
		*nbytes = avail;

	return (result);
}

static __inline void *
ring_write_buffer(struct ring *rb, size_t *nbytes)
{
	void *result;
	uint32_t count = ring_count(rb);

	result = (count > 0) ? &rb->maps.data[rb->index.start & rb->mask] : NULL;
	if (nbytes != NULL)
		*nbytes = count;

	return (result);
}

static __inline ssize_t
ring_read_advance(struct ring *rb, ssize_t nread)
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
ring_write_advance(struct ring *rb, ssize_t nwrit)
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
 * For ring_init you have to pass a `struct ring` that will be filled out and
 * have memory mapped in for you. Much like MAP_ALIGNED for mmap, the second
 * argument to ring_init is a binary logarithm of the number of pages you want
 * mapped. For a 4k page, valid values are [0,19].
 *
 * These will return -1 on failure and set `errno`, they don't assert.
 */
int	ring_init(struct ring *, uint8_t);
int	ring_fini(struct ring *);


#ifdef TEST
/*
 * These two functions are intentionally using different pointers to show that
 * they are mapped to the same memory. Homage to Beagle Bros.
 */

static __inline uint8_t
ring_peek(struct ring *rb, uint32_t idx)
{
	assert((idx & rb->mask) == idx);
	return rb->maps.data[idx & rb->mask];
}

/* whole point is to show that our copy effects the main data */
static __inline void
ring_poke(struct ring *rb, uint32_t idx, uint8_t val)
{
	assert((idx & rb->mask) == idx);
	rb->maps.copy[idx & rb->mask] = val;
}
#endif /* TEST */
#endif /* __FREEDAVE_NET_RING_H__ */
