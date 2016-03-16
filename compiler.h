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

// Utility functions for making the compiler do what we want.

#include <cstdint>
#include <cstring>

// Count the number of contiguous zero bits starting from the MSB.
// It is undefined behavior if x is 0.
inline unsigned
count_leading_zeros_64(uint64_t x)
{
  return __builtin_clzll(x);
}

// Count the number of contiguous zero bits starting from the LSB.
// It is undefined behavior if x is 0.
inline unsigned
count_trailing_zeros_32(uint32_t x)
{
  return __builtin_ctz(x);
}

// Load an unsigned 64-bit number from an unaligned address.
// This will usually be translated into one instruction.
inline uint64_t
unaligned_load_u64(const uint8_t* p)
{
  uint64_t x;
  std::memcpy(&x, p, 8);
  return x;
}

// Load an unsigned 16-bit number from an unaligned address.
// This will usually be translated into one instruction.
inline uint16_t
unaligned_load_u16(const uint8_t* p)
{
  uint16_t x;
  std::memcpy(&x, p, 8);
  return x;
}

// Hint to the compiler that X is probably true.
#define LIKELY(X) __builtin_expect(!!(X), 1)
