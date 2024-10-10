#pragma once

#define JIT_MEMORY_CAP 512

enum class BfTokenType {
	ADD = '+',
	SUB = '-',
	MOVE_RIGHT = '>',
	MOVE_LEFT = '<',
	OUTPUT = '.',
	INPUT = ',',
	JUMP_FWD = '[',
	JUMP_BACK = ']'
};

struct Token {
	BfTokenType operand;
	size_t size;

	Token(BfTokenType _kind, size_t _size) : operand(_kind), size(_size) {
	}

	Token(size_t _size, BfTokenType _kind) : operand(_kind), size(_size) {
	}
};

struct Backpatch {
	size_t operand_byte_addr;
	size_t src_byte_addr;
	size_t dst_op_index;
};

typedef void (*RunFunc)(void* memory);