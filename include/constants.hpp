#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <array>
#include "main.hpp"

#define CREATE_ARRAY(name, ...)                                                                                       \
	constexpr uint8_t name[] = {__VA_ARGS__};                                                                          \
	constexpr std::size_t name##_SIZE = sizeof(name) / sizeof(name[0]);


#if PLATFORM_LINUX
CREATE_ARRAY(START_BYTES);
#elif PLATFORM_WINDOWS

CREATE_ARRAY(START_BYTES, 
	0x48, 0x89 ,0xcf, // mov rdi, rcx
	0x48, 0x31, 0xc9 // xor rcx, rcx
);
#endif

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

#endif // CONSTANTS_HPP
