// Copyright 2016 Jakob Stoklund Olesen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This implements a second variant of the SQLite variable-length integer
// encoding.
//
// The first byte B0 determines the length:
//
// [0; cut1[    1 byte, value = B0.
// [cut1; cut2[ 2 bytes, first byte provides 6 high bits.
// [cut2; cut3[ 3 bytes, first byte provides 3 high bits.
// [cut3; 255]  4-9 bytes, first byte provides length.

#include "compiler.h"
#include "varint.h"

using namespace std;

const unsigned cut1 = 178;
const unsigned cut2 = 242;
const unsigned cut3 = 250; // total = B0 - cut3 + 4 bytes.

// Bijective encoding of 1, 2, 3 bytes. Not bijective with 4+ bytes.
const uint64_t offset1 = cut1;
const uint64_t limit1 = offset1 + (1 << 14);

const uint64_t offset2  = limit1;
const uint64_t limit2  = offset2 + (1 << 19);

static_assert(cut2 - cut1 == 64, "B0 provides 6 bits");
static_assert(cut3 - cut2 == 8, "B0 provides 3 bits");

vector<uint8_t> lesqlite2_encode(const vector<uint64_t> &in) {
  vector<uint8_t> out;
  for (auto x : in) {
    if (x < cut1) {
      // 1 byte.
      out.push_back(x);
    } else if (x < limit1) {
      // 2 bytes encode 14 bits.
      x -= offset1;
      out.push_back(cut1 + (x >> 8));
      out.push_back(x & 0xff);
    } else if (x < limit2) {
      // 3 bytes encode 19 bits.
      x -= offset2;
      out.push_back(cut2 + (x >> 16));
      // Next 2 bytes in little-endian.
      out.push_back(x & 0xff);
      x >>= 8;
      out.push_back(x & 0xff);
    } else {
      // 4-9 bytes, no offset.
      unsigned bits = 64 - count_leading_zeros_64(x);
      unsigned bytes = (bits + 7) / 8;
      out.push_back(cut3 + (bytes - 3));
      for (unsigned n = 0; n < bytes; n++) {
        out.push_back(x & 0xff);
        x >>= 8;
      }
    }
  }
  return out;
}

void lesqlite2_decode(const uint8_t *in, uint64_t *out, size_t count) {
  while (count-- > 0) {
    uint8_t b0 = *in++;
    if (LIKELY(b0 < cut1)) {
      *out++ = b0;
    } else if (b0 < cut2) {
      b0 -= cut1;
      uint8_t b1 = *in++;
      *out++ = offset1 + b1 + (b0 << 8);
    } else if (b0 < cut3) {
      b0 -= cut2;
      *out++ = offset2 + unaligned_load_u16(in) + (b0 << 16);
      in += 2;
    } else {
      b0 -= cut3;
      *out++ = unaligned_load_u64(in) & ((uint64_t(1) << 8 * b0 << 24) - 1);
      in += 3 + b0;
    }
  }
}

const codec_descriptor lesqlite2_codec = {
    .name = "leSQLite2", .encoder = lesqlite2_encode, .decoder = lesqlite2_decode,
};
