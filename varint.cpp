#include <vector>
#include <random>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <iostream>
#include <cassert>

using namespace std;

// How many uint64_t numbers in the vector we're compressing.
const size_t N = 1000;

// Maximum significant bits in the log-uniform rendom numbers.
const unsigned random_bits = 64;

// Generate n log-uniform random numbers.
vector<uint64_t>
gen_log_uniform(size_t n) {
    default_random_engine gen;
    uniform_real_distribution<double> dist(0.0, random_bits*log(2));
    vector<uint64_t> vec;

    vec.reserve(n);
    while (vec.size() < n) {
	double x = exp(dist(gen));
	if (x < 0x1p64)
	    vec.push_back(x);
    }
    return vec;
}

vector<uint8_t>
leb128_encode(const vector<uint64_t> &in) {
    vector<uint8_t> out;
    for (auto x : in) {
	while (x > 127) {
	    out.push_back(x | 0x80);
	    x >>= 7;
	}
	out.push_back(x);
    }
    return out;
}

typedef void (*decoder_func)(const uint8_t *in, uint64_t *out, size_t count);

void
leb128_decode(const uint8_t *in, uint64_t *out, size_t count) {
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
varint_encode(const vector<uint64_t> &in) {
    vector<uint8_t> out;
    for (auto x : in) {
	unsigned bits = 64 - __builtin_clzll(x|1);
	unsigned bytes = 1 + (bits-1)/7;
        if (bits > 56) {
	    out.push_back(0);
	    bytes = 8;
	} else {
	    x = (2*x+1) << (bytes-1);
	}
	for (unsigned n = 0; n < bytes; n++) {
	    out.push_back(x & 0xff);
	    x >>= 8;
	}
    }
    return out;
}

uint64_t
uload(const uint8_t *p) {
    uint64_t x;
    memcpy(&x, p, 8);
    return x;
}

static inline
unsigned
varint_length(const uint8_t *p) {
    return 1 + __builtin_ctz(*p | 0x100);
}

static inline
uint64_t
varint_decode(const uint8_t *p, unsigned length) {
    if (length < 9) {
	uint64_t mask = (uint64_t(1) << 7*length) - 1;
	return (uload(p) >> length) & mask;
    } else {
	return uload(p+1);
    }
}

void
varint_decode(const uint8_t *in, uint64_t *out, size_t count) {
    while (count-- > 0) {
	unsigned length = varint_length(in);
	*out++ = varint_decode(in, length);
	in += length;
    }
}


double
time_decode(const uint8_t *in, uint64_t *out, decoder_func func, unsigned repetitions) {
    using namespace chrono;
    auto before = high_resolution_clock::now();
    for (unsigned n =0; n < repetitions; n++)
	func(in, out, N);
    auto after = high_resolution_clock::now();
    auto ns = duration_cast<nanoseconds>(after - before).count();
    cout << repetitions << " reps = " << ns << " ns" << endl;
    return double(ns) / 1e9;
}

double
measure(const uint8_t *in, uint64_t *out, decoder_func func) {
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
main() {
    auto numbers = gen_log_uniform(N);
    vector<uint64_t> buffer(N);
    auto encoded = leb128_encode(numbers);
    double leb128 = measure(encoded.data(), buffer.data(), leb128_decode);
    assert(buffer == numbers);

    cout << "LEB128 = " << leb128 << " secs / " << N << endl;

    encoded = varint_encode(numbers);
    double varint = measure(encoded.data(), buffer.data(), varint_decode);
    assert(buffer == numbers);

    cout << "VarInt = " << varint << " secs / " << N << endl;

    cout << "T(LEB128) / T(VarInt) = " << leb128/varint << endl;
}
