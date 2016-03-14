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

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

#include "compiler.h"
#include "varint.h"

using namespace std;

// How many uint64_t numbers in the vector we're compressing.
const size_t N = 100000;

// Maximum significant bits in the log-uniform random numbers.
const unsigned random_bits = 64;

// Generate n log-uniform random numbers.
vector<uint64_t> gen_log_uniform(size_t n) {
  default_random_engine gen;
  uniform_real_distribution<double> dist(0.0, random_bits * log(2));
  vector<uint64_t> vec;

  vec.reserve(n);
  while (vec.size() < n)
    vec.push_back(exp(dist(gen)));
  return vec;
}

double time_decode(const uint8_t *in, uint64_t *out, decoder_func func,
                   unsigned repetitions) {
  using namespace chrono;
  auto before = high_resolution_clock::now();
  for (unsigned n = 0; n < repetitions; n++)
    func(in, out, N);
  auto after = high_resolution_clock::now();
  auto ns = duration_cast<nanoseconds>(after - before).count();
  return double(ns) / 1e9;
}

double measure(const uint8_t *in, uint64_t *out, decoder_func func) {
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

double do_codec(const codec_descriptor &codec,
                const vector<uint64_t> &numbers) {
  vector<uint64_t> buffer(numbers.size());
  auto encoded = codec.encoder(numbers);

  printf("%s: %.3f bytes/integer.\n", codec.name,
         double(encoded.size()) / numbers.size());
  fflush(stdout);

  double dtime = measure(encoded.data(), buffer.data(), codec.decoder);
  assert(buffer == numbers);

  printf("%s: %.3e secs.\n", codec.name, dtime);

  return dtime;
}

int main() {
  auto numbers = gen_log_uniform(N);

  double leb128 = do_codec(leb128_codec, numbers);
  double prefix = do_codec(prefix_codec, numbers);

  printf("T(LEB128) / T(PrefixVarint) = %.3f.\n", leb128 / prefix);
}
