#include <cstdint>
#include <cstddef>
#include <bit>
#include <iostream>
#include <type_traits>
#include <bitset>

constexpr uint32_t zext(char c) {
    return (unsigned char)c;
}

uint8_t codepoint_length(const char* const ptr) {
	const uint32_t u = ~(((uint32_t)ptr[0]) << 24);

	constexpr uint32_t out[] = {
		1, 1, 2, 3, 4
	};

	return out[__builtin_clz(u)];
}

uint32_t ground(const char* const ptr, const size_t sz) {
	uint32_t out = 0;

	switch (sz) {
		case 1: return (((uint32_t)ptr[0]));
		case 2: return (((uint32_t)ptr[0] & 31u) << 6u)  | (((uint32_t)ptr[1] & 63u) << 0u);
		case 3: return (((uint32_t)ptr[0] & 15u) << 12u) | (((uint32_t)ptr[1] & 63u) << 6u)  | (((uint32_t)ptr[2] & 63u) << 0u);
		case 4: return (((uint32_t)ptr[0] & 7u)  << 18u) | (((uint32_t)ptr[1] & 63u) << 12u) | (((uint32_t)ptr[2] & 63u) << 6u) | (((uint32_t)ptr[3] & 63u) << 0u);
	}

	__builtin_unreachable();
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

__attribute__((always_inline)) constexpr auto loop(const char* const ptr, const size_t sz) {
	const auto row = ((MAX_CODEPOINT_SZ) * (sz - 1u));

	uint32_t out = 0;
	for (uint32_t i = 0; i != sz; i++)
		out ^= zext(ptr[i]) << SHIFTS[row + i];

	uint32_t x = 0;
	for (uint32_t i = 0; i != sz; i++)
		x |= MASKS[row + i] << SHIFTS[row + i];

	return out ^ x;
};

uint32_t decode(const char* const ptr, const size_t sz) {
	switch (sz) {
		[[unlikely]] case 4: return loop(ptr, 4);
		[[unlikely]] case 3: return loop(ptr, 3);
		[[unlikely]] case 2: return loop(ptr, 2);
	}

    return loop(ptr, 1);
}

int main() {
	const auto one   = "g";
	const auto two   = "ȥ";
	const auto three = "⛸";
	const auto four  = "𝓕";

	auto dec_one   = decode(one,   codepoint_length(one));
	auto dec_two   = decode(two,   codepoint_length(two));
	auto dec_three = decode(three, codepoint_length(three));
	auto dec_four  = decode(four,  codepoint_length(four));

	auto gro_one   = ground(one,   codepoint_length(one));
	auto gro_two   = ground(two,   codepoint_length(two));
	auto gro_three = ground(three, codepoint_length(three));
	auto gro_four  = ground(four,  codepoint_length(four));

	std::cout << std::bitset<24>(dec_three) << '\n';
	std::cout << std::bitset<24>(gro_three) << "\n\n";

	std::cout << std::bitset<32>(dec_four) << '\n';
	std::cout << std::bitset<32>(gro_four) << "\n\n";

	std::cout << (dec_one   == 0x67)    << " " << (gro_one   == 0x67)    << '\n';
	std::cout << (dec_two   == 0x225)   << " " << (gro_two   == 0x225)   << '\n';
	std::cout << (dec_three == 0x26F8)  << " " << (gro_three == 0x26F8)  << '\n';
	std::cout << (dec_four  == 0x1D4D5) << " " << (gro_four  == 0x1D4D5) << '\n';
}
