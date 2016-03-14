Variable-Length Integers
========================

This repository contains code for exploring the performance of different
variable-length integer encoding schemes, specifically to determine the best
encoding to use in the WebAssembly binary encoding. See the related
[WebAssembly design issue](https://github.com/WebAssembly/design/issues/601).

There are three primary interesting performance factors for a variable-length
integer encoding:

1. Compression ratio for the relevant distribution of integers.
2. Decoding speed.
3. Encoding speed.

We don't measure encoding speed here since it is less relevant for WebAssembly
as long as it is reasonable.

Note that the C++ code in this repository is *not production quality*. Like
most C++ code, it can be tricked into executing _undefined behavior_ by
maliciously formed input.

LEB128
------
The [Little Endian Base-128](https://en.wikipedia.org/wiki/LEB128) encoding is
used in [DWARF](http://dwarfstd.org) debug information files and many other
places. It is a simple byte-oriented encoding where each byte contains 7 bits
of the integer being encoded, and the MSB is used as a _tag bit_ which
indicates if there are more bytes coming.

    0xxxxxxx                     7 bits in 1 byte
    1xxxxxxx 0xxxxxxx           14 bits in 2 bytes
    1xxxxxxx 1xxxxxxx 0xxxxxxx  21 bits in 3 bytes
    ...

PrefixVarint
------------
Brought up in WebAssembly/design#601, and probably invented independently many
times, this encoding is very similar to LEB128, but it moves all the tag bits
to the LSBs of the first byte:

    xxxxxxx1  7 bits in 1 byte
    xxxxxx10 14 bits in 2 bytes
    xxxxx100 21 bits in 3 bytes
    xxxx1000 28 bits in 4 bytes
    xxx10000 35 bits in 5 bytes
    xx100000 42 bits in 6 bytes
    x1000000 49 bits in 7 bytes
    10000000 56 bits in 8 bytes
    00000000 64 bits in 9 bytes

This has advantages on modern CPUs with fast unaligned loads and _count
trailing zeros_ instructions. The compression ratio is the same as for LEB128,
except for those 64-bit numbers that require 10 bytes to encode in LEB128.

Like [UTF-8](https://en.wikipedia.org/wiki/UTF-8), the length of a
PrefixVarint-encoded number can be determined from the first byte. (UTF-8 is
not considered here since it only encodes 6 bits per byte due to design
constraints that are not relevant to WebAssembly.)

SQLite
------
The [SQLite variable-length integer
encoding](https://sqlite.org/src4/doc/trunk/www/varint.wiki) is biased towards
integer distributions with more small numbers. It can encode the integers 0-240
in one byte. The first byte, `A[0]` determines the encoding:

    0-240   1 byte   A[0]
    241-248 2 bytes  240+256*(A[0]-241)+A[1]
    249     3 bytes  2288+256*A[1]+A[2]
    250     4 bytes  A[1..3]
    251     5 bytes  A[1..4]
    252     6 bytes  A[1..4]
    253     7 bytes  A[1..4]
    254     8 bytes  A[1..4]
    255     9 bytes  A[1..8]

This encoding is big-endian, and also has the property that lexicographical and
numerical orderings are the same. This is not important for WebAssembly. We
would probably modify it to be little-endian for improved decoding speed.
