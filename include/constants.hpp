#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP
#include <cstdio>
#include <array>
#include "main.hpp"

#define CREATE_ARRAY(name, ...)                                                                                       \
	constexpr uint8_t name[] = {__VA_ARGS__};                                                                          \
	constexpr std::size_t name##_SIZE = sizeof(name);

#define CREATE_RUNTIME_ARRAY(name, func)                                                                              \
    const auto __##name = func();                                                                                            \
	const auto name = __##name.data();                                                                                       \
	const std::size_t name##_SIZE = __##name.size() / sizeof(name[0]); 

#define CREATE_NOTHING(name) \
	constexpr uint8_t* name = nullptr;                                                                                 \
	constexpr std::size_t name##_SIZE = 0;

static void convertToLittleEndian(uint8_t* dst, uintptr_t value) {
	dst[0] = static_cast<uint8_t>(value & 0xFF);
	dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	dst[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
	dst[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
	dst[4] = static_cast<uint8_t>((value >> 32) & 0xFF);
	dst[5] = static_cast<uint8_t>((value >> 40) & 0xFF);
	dst[6] = static_cast<uint8_t>((value >> 48) & 0xFF);
	dst[7] = static_cast<uint8_t>((value >> 56) & 0xFF);
}


CREATE_ARRAY(ADD_BYTES, 0x80, 0x07); // add byte[rdi],
CREATE_ARRAY(SUB_BYTES, 0x80, 0x2f); // sub byte[rdi],
CREATE_ARRAY(MOVE_RIGHT_BYTES, 0x48, 0x83, 0xc7); // add rdi,
CREATE_ARRAY(MOVE_LEFT_BYTES, 0x48, 0x83, 0xef); // sub rdi,

CREATE_ARRAY(JUMP_FWD_BYTES, 
	0x8a, 0x07, // mov al, byte [rdi]
	0x84, 0xc0, // test al, al
	0x0f, 0x84  // jz
);

CREATE_ARRAY(JUMP_BACK_BYTES,
	0x8a, 0x07, // mov al, byte [rdi]
	0x84, 0xc0,	// test al, al
	0x0f, 0x85  // jz
);


#if PLATFORM_LINUX
static const uintptr_t address_getchar = reinterpret_cast<uintptr_t>(&getchar);

CREATE_NOTHING(START_BYTES);
CREATE_ARRAY(OUTPUT_BYTES,
			 0x57,									   // push rdi
			 0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00, // mov rax, 1
			 0x48, 0x89, 0xfe,						   // mov rsi, rdi
			 0x48, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00, // mov rdi, 1
			 0x48, 0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, // mov rdx, 1
			 0x0f, 0x05,							   // syscall
			 0x5f									   // pop rdi
);

CREATE_ARRAY(INPUT_BYTES,
			 0x57,									   // push rdi
			 0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00, // mov rax, 1
			 0x48, 0x89, 0xfe,						   // mov rsi, rdi
			 0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00, // mov rdi, 0
			 0x48, 0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, // mov rdx, 1
			 0x0f, 0x05,							   // syscall
			 0x5f									   // pop rdi
);


#elif PLATFORM_WINDOWS
static const uintptr_t address_putchar = reinterpret_cast<uintptr_t>(&putchar);

const auto createOutputStart = []() {
	// Define a byte array of 12 bytes
	std::array<uint8_t, 22> byteArray = {
		0x8a, 0x0f,				// mov cl, byte [rdi]
		0x48, 0xba,				// mov rdx,
		0x00, 0x00, 0x00, 0x00, // Placeholder for address (first 4 bytes)
		0x00, 0x00, 0x00, 0x00,	// Placeholder for address (last 4 bytes)
		0x48, 0x83, 0xec, 0x28, // sub rsp, 40
		0xff, 0xd2,				// call rdx
		0x48, 0x83, 0xc4, 0x28	// add rsp, 40
	};
	convertToLittleEndian(byteArray.data() + 4, address_putchar);
	// Update the address in little-endian format

	return byteArray;
};

CREATE_ARRAY(START_BYTES, 0x48, 0x89, 0xcf, // mov rdi, rcx
			 0x48, 0x31, 0xc9				// xor rcx, rcx
);

CREATE_RUNTIME_ARRAY(OUTPUT_BYTES, createOutputStart);

// TODO: input for windows
CREATE_NOTHING(INPUT_BYTES);

#endif

#endif // CONSTANTS_HPP
