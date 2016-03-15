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

// This implements a variant of the SQLite variable-length integer encoding.
//
// The first byte B0 determines the length:
//
// 0-184: 1 byte, value = B0.
// 185-248: 2 bytes, value = 185 + B1 + 256*(B0-185).
// 249-255: 3-9 bytes, B0-249+2 little-endian encoded bytes following B0.

#include "compiler.h"
#include "varint.h"

using namespace std;

const unsigned cut1 = 185;
const unsigned cut2 = 249;

vector<uint8_t> lesqlite_encode(const vector<uint64_t> &in) {
  vector<uint8_t> out;
  for (auto x : in) {
    if (x < cut1) {
      // 1 byte.
      out.push_back(x);
    } else if (x <= cut1 + 255 + 256 * (cut2 - 1 - cut1)) {
      // 2 bytes.
      x -= cut1;
      out.push_back(cut1 + (x >> 8));
      out.push_back(x & 0xff);
    } else {
      // 3-9 bytes.
      unsigned bits = 64 - count_leading_zeros_64(x);
      unsigned bytes = (bits + 7) / 8;
      out.push_back(cut2 + (bytes - 2));
      for (unsigned n = 0; n < bytes; n++) {
        out.push_back(x & 0xff);
        x >>= 8;
      }
    }
  }
  return out;
}

void lesqlite_decode(const uint8_t *in, uint64_t *out, size_t count) {
  while (count-- > 0) {
    uint8_t b0 = *in++;
    if (LIKELY(b0 < cut1)) {
      *out++ = b0;
    } else if (b0 < cut2) {
      uint8_t b1 = *in++;
      *out++ = cut1 + b1 + ((b0 - cut1) << 8);
    } else {
      size_t sh = b0 - cut2;
      *out++ = unaligned_load_u64(in) & ((uint64_t(1) << 8 * sh << 16) - 1);
      in += 2 + sh;
    }
  }
}

const codec_descriptor lesqlite_codec = {
    .name = "leSQLite", .encoder = lesqlite_encode, .decoder = lesqlite_decode,
};
