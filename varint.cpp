#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

// How many uint64_t numbers in the vector we're compressing.
const size_t N = 100000;

// Maximum significant bits in the log-uniform random numbers.
const unsigned random_bits = 64;

typedef void (*decoder_func)(const uint8_t* in, uint64_t* out, size_t count);

// Generate n log-uniform random numbers.
vector<uint64_t>
gen_log_uniform(size_t n)
{
  default_random_engine gen;
  uniform_real_distribution<double> dist(0.0, random_bits * log(2));
  vector<uint64_t> vec;

  vec.reserve(n);
  while (vec.size() < n)
    vec.push_back(exp(dist(gen)));
  return vec;
}

static inline unsigned
count_leading_zeros_64(uint64_t x)
{
  return __builtin_clzll(x);
}

static inline unsigned
count_trailing_zeros_32(uint32_t x)
{
  return __builtin_ctz(x);
}

vector<uint8_t>
leb128_encode(const vector<uint64_t>& in)
{
  vector<uint8_t> out;
  for (auto x : in) {
    while (x > 127) {
      out.push_back(x | 0x80);
      x >>= 7;
    }
    out.push_back(x);
  }
  cout << "LEB128: " << out.size() << " bytes." << endl;
  return out;
}

void
leb128_decode(const uint8_t* in, uint64_t* out, size_t count)
{
  while (count-- > 0) {
    uint64_t value = 0;
    unsigned shift = 0;
    uint8_t byte;
    do {
      byte = *in++;
      value |= uint64_t(byte & 0x7f) << shift;
      shift += 7;
    } while (byte >= 128);
    *out++ = value;
  }
}

vector<uint8_t>
varint_encode(const vector<uint64_t>& in)
{
  vector<uint8_t> out;
  for (auto x : in) {
    unsigned bits = 64 - count_leading_zeros_64(x | 1);
    unsigned bytes = 1 + (bits - 1) / 7;
    if (bits > 56) {
      out.push_back(0);
      bytes = 8;
    } else {
      x = (2 * x + 1) << (bytes - 1);
    }
    for (unsigned n = 0; n < bytes; n++) {
      out.push_back(x & 0xff);
      x >>= 8;
    }
  }
  cout << "VarInt: " << out.size() << " bytes." << endl;
  return out;
}

uint64_t
uload(const uint8_t* p)
{
  uint64_t x;
  memcpy(&x, p, 8);
  return x;
}

static inline size_t
varint_length(const uint8_t* p)
{
  return 1 + count_trailing_zeros_32(*p | 0x100);
}

static inline uint64_t
varint_decode(const uint8_t* p, size_t length)
{
  if (length < 9) {
    size_t unused = 64 - 8 * length;
    return uload(p) << unused >> (unused + length);
  } else {
    return uload(p + 1);
  }
}

void
varint_decode(const uint8_t* in, uint64_t* out, size_t count)
{
  while (count-- > 0) {
    size_t length = varint_length(in);
    *out++ = varint_decode(in, length);
    in += length;
  }
}

double
time_decode(const uint8_t* in, uint64_t* out, decoder_func func,
            unsigned repetitions)
{
  using namespace chrono;
  auto before = high_resolution_clock::now();
  for (unsigned n = 0; n < repetitions; n++)
    func(in, out, N);
  auto after = high_resolution_clock::now();
  auto ns = duration_cast<nanoseconds>(after - before).count();
  return double(ns) / 1e9;
}

double
measure(const uint8_t* in, uint64_t* out, decoder_func func)
{
  unsigned repetitions = 1;
  double runtime = time_decode(in, out, func, repetitions);
  while (runtime < 1) {
    repetitions *= 10;
    runtime = time_decode(in, out, func, repetitions);
  }

  repetitions = round(repetitions / runtime);
  runtime = time_decode(in, out, func, repetitions);
  return runtime / repetitions;
}

int
main()
{
  auto numbers = gen_log_uniform(N);
  vector<uint64_t> buffer(N);

  cout << "Encoding " << N << " random log-uniform " << random_bits
       << "-bit numbers." << endl;

  auto encoded = leb128_encode(numbers);
  double leb128 = measure(encoded.data(), buffer.data(), leb128_decode);
  assert(buffer == numbers);

  cout << "LEB128: " << leb128 << " secs / " << N << " decoded numbers."
       << endl;

  encoded = varint_encode(numbers);
  // Add some padding so the varint_decode function can safely read ahead.
  encoded.resize(encoded.size() + 8);
  double varint = measure(encoded.data(), buffer.data(), varint_decode);
  assert(buffer == numbers);

  cout << "VarInt: " << varint << " secs / " << N << " decoded numbers."
       << endl;

  cout << "T(LEB128) / T(VarInt) = " << leb128 / varint << endl;
}
