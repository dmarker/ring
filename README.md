[1]: https://www.freebsd.org/
[10]: https://github.com/dmarker/bong-utils
[11]: https://www.youtube.com/watch?v=H8THRznXxpQ
[13]: https://en.wikipedia.org/w/index.php?title=Circular_buffer&oldid=600431497#Optimized_POSIX_implementation
[14]: https://en.wikipedia.org/wiki/Backronym
[15]: https://go.dev
[16]: https://github.com/illumos/illumos-gate/blob/master/usr/src/tools/scripts/cstyle.pl

[20]: https://man.freebsd.org/cgi/man.cgi?query=shm_open&sektion=2
[21]: https://man.freebsd.org/cgi/man.cgi?query=read&sektion=2
[22]: https://man.freebsd.org/cgi/man.cgi?query=write&sektion=2
[23]: https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2
[24]: https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2
[25]: https://man.freebsd.org/cgi/man.cgi?query=readv&sektion=2
[25]: https://man.freebsd.org/cgi/man.cgi?query=writev&sektion=2
[26]: https://man.freebsd.org/cgi/man.cgi?query=recvmsg&sektion=2
[27]: https://man.freebsd.org/cgi/man.cgi?query=sendmsg&sektion=2
[28]: https://man.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2

# ring
This is a ring buffer library using the shared memory trick to map the same
buffer twice. The code is highly [FreeBSD][1] specific. I don't test it
elsewhere. It is likely to get more FreeBSD specific if I get around to using
[shm_create_largepage(2)][20] instead of (or with) [shm_open(2)][20].

I have used and written myself many ring buffers going back to university. And
yet I had none saved or on github, so I was dreading all the edge cases to deal
with `struct iovec`, but I need a buffer for one of my [bong-utils][10]. It just
doesn't seem like it should be as hard as it always ends up being.

I had not long ago watched an excellent video by Casey Muratori,
[Powerful Page Mapping Techniques][11] and it was somewhere in the back of my
mind. It won't take much googling before you find lots of blog posts and github
repos doing something similar. This will just be one more to get lost among
them probably. There was a wikipedia entry everybody seems to reference,
[Optimized_POSIX_implementation][13] but it was since removed.

That was helpful insofar as it gave me the man pages I needed to pore over, but
ultimately I chose an interface that lends itself to an event loop and is
friendly with both [read(2)][21]/[write(2)][22] (for files) as well as
[recv(2)][23]/[send(2)][24] (for sockets). And because I was having far too much
fun I made it parameterizable (yeah C macros) for the storage type of the index.
But I have since decided that is of no utility and only the one version is
easier to work with.

You should _not_ require [readv(2)][25]/[writev(2)][26] _nor_
[recvmsg(2)][27]/[sendmsg(2)][28]. That is the whole motivation.

In addition to using this new (to me) technique of mapping the same memory
back to back I continued to use the trick of AND for fast modulus of a power
of 2 and letting the `start` and `end` indexes just "wrap" using unsigned rules.
This requires the masking for the `ring_read_buffer` and
`ring_write_buffer` functions since the values can have gone way past
the size before wrapping. But the power of 2 mask (modulus) always keeps it
starting somewhere you can index off the first buffer. Which means I never
require more than 2 maps to the same memory.

I think it worked out pretty well even though I was overzealous in making it
work with multiple sizes when practically speaking uint32_t is all I need.
And I'll probably be within rounding error of 100% of consumers of this
library :)

Seriously, that's why I'm not trying to think of anything clever for a
project name. If I could [backronym][14] something to oring (one ring to
rule them all and in the darkness bind them!) I'd take it. I mean I'd rather
it was one given to the miners and craftsmen of the mountain halls, but that
would be too many letters.

## example

Just to explain what I mean by friendly to the system functions. Which after
coding it up, I realize I am talking about function chaining, nevertheless here
is a contrived example that does nothing more than read stdin and write it back
to stdout. Its not even using kevent, because that isn't needed to show what I
mean. It may seem excessive, but it is a working example of using the ring
buffer in under 100 lines.

```C
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include "ring.h"

static struct ring OneRing;		/* to rule them all */

static void
set_nonblocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1) err(
		EX_OSERR, "fcntl: can't retrieve flags"
	);

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) err(
		EX_OSERR, "fcntl: can't set flags"
	);
}

static void
err_cleanup(int _)
{
	ring_fini(&OneRing);
}

int
main()
{
	bool eof = false;
	ssize_t rc;
	size_t spc;

	/* just one page */
	if (ring_init(&OneRing, 0) == -1) err(
		EX_OSERR, "ring_init failed"
	); else {
		/* blindly calls fini so set up after successful init */
		err_set_exit(err_cleanup);
	}
	set_nonblocking(STDIN_FILENO);
	set_nonblocking(STDOUT_FILENO);

	do {
		/* This is what I mean by friendly to the existing API */
		rc = ring_read_advance(
			&OneRing,	/* to find them */
			read(
				STDIN_FILENO,
				ring_read_buffer(&OneRing, &spc),
				spc
			)
		);
		if (rc == 0 && spc != 0) {
			/*
			 * only if we could have read something does it matter
			 * when we received nothing. That is EOF.
			 */
			eof = true;
		} else if (rc == -1 && errno != EAGAIN) err(
			EX_OSERR, "failed to read into buffer"
		);

		rc = ring_write_advance(
			&OneRing,	/* to bring them all, */
			write(
				STDOUT_FILENO,
				ring_write_buffer(&OneRing, &spc),
				spc
			)
		);
		if (rc == -1 && errno != EAGAIN) err(
			EX_OSERR, "failed to write from buffer"
		);
	} while (
					/* and in the darkness bind them! */
		!(eof && ring_empty(&OneRing))
	);

	ring_fini(&OneRing);
	return (0);
}
```
Yeah I did a lot of [golang][15] and it definitely influenced my C style.
I think for the better. I used to automatically splat [cstyle][16] compliant
code (that's what being a Solaris gatekeeper does to a guy) but now I splat
this golang influenced variant and I prefer it.

That just needs to have files directed to it to test:
```
# ./a.out < README.md > check
# diff README.md check
```
[bong-utils][10] will have an example using [kqueue(2)][28], but I think that
small snip of code shows why I chose the APIs I did. It doesn't even appear to
be using a ring buffer as it never needs `struct iovec`. Maybe its just me, and
maybe I suck, but I remember wasting ages on edge cases when you go the route of
`struct iovec`. There is nothing to get wrong here.

## editorial
I spent a lot of time failing to get [shm_create_largepage(2)][20] to work until
I dug through the kernel code to see that it will always return `EINVAL` if you
use `psind=0` (which is what the default page size is). That is not in the man
page.

This seems like a real shame as there are some other properties of that function
that make it seem like the better choice for [FreeBSD][1], specifically this bullet
from the man page:
```
	•	Memory for a largepage object is allocated when the object is
		extended using the ftruncate(2) system call, whereas memory for
		shared memory objects is allocated lazily and may be
		paged out to a swap device when not in use.
```
I don't see why it was disallowed. I need to investigate more, but if possible I
would advocate to remove this restriction that `psind` can't be 0 (the default
system page size).

And when you are trying a new system call with many parameters you are unsure
of, `EINVAL` isn't really clear enough. But I am glad those man pages are
together or I wouldn't have learned about [shm_create_largepage(2)][20] or the
`SHM_ANON` flag and so on...

And even though I'm complaining, I have not experienced any issues using
[shm_open(2)][20].

I know I'm super late to this party but I marvel at how much simpler this made
using a ring buffer. And apart from needing to read/experiment a lot to get the
incantations just right for initialization (creating the mapping) it was fun
code to write.
