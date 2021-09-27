#include <iostream>
#include <fstream>
#include <bitset>
#include <limits>
#include <string_view>
#include <cstdint>
#include <array>
#include <random>
#include <chrono>

#define ANKERL_NANOBENCH_IMPLEMENT
#include "nanobench.h"


// UTF-8 crash course
/*
    A UTF-8 encoded unicode codepoint can contain anywhere from 1-4 bytes.
    The first byte encodes the size and the remaining bytes in the codepoint
    are known as "continuation bytes".
    The first byte can take any of the following forms:
        0b0xxxxxxx = 1 byte
        0b11xxxxxx = 2 bytes
        0b111xxxxx = 3 bytes
        0b1111xxxx = 4 bytes
    Note: Single byte UTF-8 encoded codepoints are 100% ASCII compatible.
    And continuation bytes take the form:
        0b10xxxxxx
    Some examples of UTF-8 encoded codepoints:
        "g" = 01100111
        "ȥ" = 11001000 10100101
        "⛸" = 11100010 10011011 10111000
        "𝓕" = 11110000 10011101 10010011 10010101
    The decoded form of each of the above is:
        "g" = 0x67
        "ȥ" = 0x225
        "⛸" = 0x26F8
        "𝓕" = 0x1D4D5
    Taking the 4 byte codepoint example from above, let's break it down:
        "𝓕" =
            11110000 10011101 10010011 10010101
            xxxx0000 xx011101 xx010011 xx010101
                        11101   010011   010101
                        = 11101010011010101
                        = 0x1D4D5
*/



// START OF BENCHING DRIVER CODE

std::string read_file(const std::string& str) {
	std::ifstream ifs(str);
	return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}


// bytes needed to encode codepoint
constexpr uint32_t bytes_needed(uint32_t c) {
	const bool vals[] = {
		c < 128,
		c >= 128 && c < 2048,
		c >= 2048 && c < 65536,
		c >= 65536 && c < 1114112,
	};

	uint32_t out = 0;

	for (uint8_t i = 0; i < 4; ++i)
		if (vals[i]) out = i;

	return out + 1;
}


// encode int as utf-8 codepoint.
constexpr auto encode(uint32_t c) {
	constexpr uint8_t first_byte_mask[] = {
		0b00000000,
		0b11000000,
		0b11100000,
		0b11110000,
	};

	const auto bn = bytes_needed(c);

	constexpr uint8_t continuation         = 0b10'000000;
	constexpr uint8_t trailing_byte_mask   = 0b00'111111;

	constexpr uint8_t max_codepoint_length = 4u;
	const uint8_t index = bn - 1;

	std::array<char, 5> buffer = {{ 0 }};

	buffer[0] = first_byte_mask[bn - 1] + (c >> (index * 6u));

	for (uint32_t i = 1; i != bn; i++) {
		buffer[i] = continuation + ((c >> ((index - i) * 6u)) & trailing_byte_mask);
	}

	return buffer;
}


// random utf-8 encoded string.
inline std::string random_string(size_t n) {
	std::random_device dev;
	std::mt19937 rng(0);
	std::uniform_int_distribution<std::mt19937::result_type> dist(1, 983040);

	std::string out;

	while (n--) {
		out += std::string(encode(dist(rng)).data());
	}

	return out;
}

// END OF BENCH DRIVER CODE
constexpr uint8_t codepoint_length(const char* const ptr) {
	uint32_t u = ~(((uint32_t)ptr[0]) << 24);

	constexpr uint32_t out[] = {
		1, 1, 2, 3, 4
	};

	return out[__builtin_clz(u)];
}


constexpr uint32_t zext(char c) {
    return (unsigned char)c;
}


// jack2
template <typename T> constexpr auto condition(bool cond, T a, T b) {
	return (cond * a) + (!cond * b);
}

template <typename T> constexpr auto min(T a, T b) {
	return condition(a < b, a, b);
}

constexpr uint32_t decode_loop(const char* const ptr, const size_t sz) {
	constexpr uint8_t max_codepoint_sz = 4u;
	constexpr uint8_t cm = 0b00111111;  // inverted codepoint mask

	// Calculate row in lookup table based on the number
	// of bytes in the codepoint.
	const auto row = ((max_codepoint_sz * 2u) * (sz - 1u));

	constexpr uint8_t masks[] = {
	//  v-------masks--------v          v----scalars----v
		0b11111111, 0u, 0u, 0u, /* | */  0u,  0u, 0u, 0u, // 1 byte(s)
		0b00011111, cm, 0u, 0u, /* | */  6u,  0u, 0u, 0u, // 2 byte(s)
		0b00001111, cm, cm, 0u, /* | */ 12u,  6u, 0u, 0u, // 3 byte(s)
		0b00000111, cm, cm, cm, /* | */ 18u, 12u, 6u, 0u, // 4 byte(s)
	};

	uint32_t out = 0;

	// Loop through the
	for (uint32_t i = 0; i != max_codepoint_sz; i++)
		out |= (ptr[min<uint8_t>(i, sz)] & masks[row + i]) << masks[row + 4u + i];

	return out;
}


constexpr uint32_t decode_hybrid(const char* const ptr, const size_t sz) {
	constexpr auto loop = [] (const char* const ptr, const size_t sz) {
		constexpr uint8_t masks[] = {
		//  v-------------------masks--------------------v     |    v----shifts----v
			0b11111111, 0b00000000, 0b00000000, 0b00000000, /* | */  0u,  0u, 0u, 0u, // 1 byte(s)
			0b00011111, 0b00111111, 0b00000000, 0b00000000, /* | */  6u,  0u, 0u, 0u, // 2 byte(s)
			0b00001111, 0b00111111, 0b00111111, 0b00000000, /* | */ 12u,  6u, 0u, 0u, // 3 byte(s)
			0b00000111, 0b00111111, 0b00111111, 0b00111111, /* | */ 18u, 12u, 6u, 0u, // 4 byte(s)
		};

		constexpr uint8_t max_codepoint_sz = 4u;
		const auto row = ((max_codepoint_sz * 2u) * (sz - 1u));

		uint32_t out = 0;

		for (uint32_t i = 0; i != sz; i++)
			out |= (ptr[i] & masks[row + i]) << masks[row + 4u + i];

		return out;
	};

	switch (sz) {
		[[unlikely]] case 2: return loop(ptr, 2);
		[[unlikely]] case 3: return loop(ptr, 3);
		[[unlikely]] case 4: return loop(ptr, 4);
	}

	return loop(ptr, 1);
}


constexpr uint8_t MASKS[] = {
	0b00000000, 0b10000000, 0b10000000, 0b10000000,
	0b11000000, 0b10000000, 0b10000000, 0b10000000,
	0b11100000, 0b10000000, 0b10000000, 0b10000000,
	0b11110000, 0b10000000, 0b10000000, 0b10000000,
};

constexpr uint8_t SHIFTS[] = {
	0u,  0u, 0u, 0u,
	6u,  0u, 0u, 0u,
	12u,  6u, 0u, 0u,
	18u, 12u, 6u, 0u,
};

constexpr uint8_t MAX_CODEPOINT_SZ = 4u;

__attribute__((always_inline)) constexpr auto decode_xor_inner(const char* const ptr, const size_t sz) {
	const auto row = ((MAX_CODEPOINT_SZ) * (sz - 1u));

	uint32_t out = 0;
	for (uint32_t i = 0; i != sz; i++)
		out ^= zext(ptr[i]) << SHIFTS[row + i];

	uint32_t x = 0;
	for (uint32_t i = 0; i != sz; i++)
		x |= MASKS[row + i] << SHIFTS[row + i];

	return out ^ x;
};


constexpr uint32_t decode_stupid(const char* const ptr, const size_t sz) {
	const uint32_t res[] = {
		decode_xor_inner(ptr, min<size_t>(1, sz)),
		decode_xor_inner(ptr, min<size_t>(2, sz)),
		decode_xor_inner(ptr, min<size_t>(3, sz)),
		decode_xor_inner(ptr, min<size_t>(4, sz)),
	};

	return res[sz - 1u];
}


// the defacto naive utf-8 decoder
constexpr uint32_t decode_branch(const char* const ptr, const size_t sz) {
	switch (sz) {
		[[unlikely]] case 2: return (((uint32_t)ptr[0] & 31u) << 6u)  | (((uint32_t)ptr[1] & 63u) << 0u);
		[[unlikely]] case 3: return (((uint32_t)ptr[0] & 15u) << 12u) | (((uint32_t)ptr[1] & 63u) << 6u)  | (((uint32_t)ptr[2] & 63u) << 0u);
		[[unlikely]] case 4: return (((uint32_t)ptr[0] & 7u)  << 18u) | (((uint32_t)ptr[1] & 63u) << 12u) | (((uint32_t)ptr[2] & 63u) << 6u) | (((uint32_t)ptr[3] & 63u) << 0u);
	}

	return (((uint32_t)ptr[0]));
}



constexpr uint32_t decode_xor(const char* ptr, const size_t sz) {
	switch (sz) {
		[[unlikely]] case 4: return decode_xor_inner(ptr, 4);
		[[unlikely]] case 3: return decode_xor_inner(ptr, 3);
		[[unlikely]] case 2: return decode_xor_inner(ptr, 2);
	}

	return decode_xor_inner(ptr, 1);
}

constexpr uint32_t countl_one(uint32_t x) {
	return __builtin_clz(~x);
}


constexpr int codepoint_length_bitmagic(const char* const ptr) {
	unsigned u = (((unsigned)((ptr[0] | 0b10000000) & ~0b01000000 ) | ((ptr[0] & 0b10000000) >> 1)) << 24);
	return countl_one(u);
}



int main() {
	auto eng = read_file("files/english.txt");
	auto rus = read_file("files/russian.txt");
	auto chi = read_file("files/chinese.txt");

	const auto run_bench = [&] (const std::string& name, auto f) {
		using namespace std::chrono_literals;
		ankerl::nanobench::Bench()
			.warmup(3)
			.minEpochIterations(25)
			.timeUnit(1us, "µs")
			.run(name, f);
	};

	constexpr auto generate_bench = [&] (const std::string& str, const auto& decoder, const auto& cp_length) {
		return [&] () {
			const char *ptr = str.data();
			const char* const end = str.data() + str.size();

			uint32_t cp;
			while (ptr != end) {
				const auto sz = cp_length(ptr);
				uint32_t cp = decoder(ptr, sz);
				ptr += sz;
				ankerl::nanobench::doNotOptimizeAway(cp);
			}
		};
	};

	run_bench("chi naive", generate_bench(eng, decode_branch, codepoint_length_bitmagic));
	run_bench("eng naive", generate_bench(rus, decode_branch, codepoint_length_bitmagic));
	run_bench("rus naive", generate_bench(chi, decode_branch, codepoint_length_bitmagic));

	run_bench("chi loop", generate_bench(eng, decode_loop, codepoint_length_bitmagic));
	run_bench("eng loop", generate_bench(rus, decode_loop, codepoint_length_bitmagic));
	run_bench("rus loop", generate_bench(chi, decode_loop, codepoint_length_bitmagic));

	run_bench("chi hybrid", generate_bench(eng, decode_hybrid, codepoint_length_bitmagic));
	run_bench("eng hybrid", generate_bench(rus, decode_hybrid, codepoint_length_bitmagic));
	run_bench("rus hybrid", generate_bench(chi, decode_hybrid, codepoint_length_bitmagic));

	run_bench("chi stupid", generate_bench(eng, decode_stupid, codepoint_length_bitmagic));
	run_bench("eng stupid", generate_bench(rus, decode_stupid, codepoint_length_bitmagic));
	run_bench("rus stupid", generate_bench(chi, decode_stupid, codepoint_length_bitmagic));

	run_bench("chi xor", generate_bench(eng, decode_xor, codepoint_length_bitmagic));
	run_bench("eng xor", generate_bench(rus, decode_xor, codepoint_length_bitmagic));
	run_bench("rus xor", generate_bench(chi, decode_xor, codepoint_length_bitmagic));
}
