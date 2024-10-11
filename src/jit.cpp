#include "jit.hpp"
#include "main.hpp"
#include "executableMemory.hpp"
#include <cassert>
#include <cstring>
#include "constants.hpp"

void jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code) {
	assert(code.empty());

	std::vector<Backpatch> backpatches;
	std::vector<size_t> addresses;
	addresses.reserve(tokens.size());	// exact amount needed
	backpatches.reserve(tokens.size()); // overestimate, assuming every operation is jump
	code.reserve(tokens.size() * 4);	// rough estimate

	auto append_to_vector = [](std::vector<uint8_t>& vec, const uint8_t* data, const size_t size) {
		if (size == 0)
			return;
		vec.insert(vec.end(), data, data + size);
	};

	append_to_vector(code, START_BYTES, START_BYTES_SIZE);
	for (const Token& tk : tokens) {
		addresses.push_back(code.size());

		switch (tk.operand) {
		case TokenType::ADD:
			append_to_vector(code, ADD_BYTES, ADD_BYTES_SIZE);
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::SUB:
			append_to_vector(code, SUB_BYTES, SUB_BYTES_SIZE);
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::MOVE_RIGHT: {
			int size = tk.size;
			while (size > 0) {
				append_to_vector(code, MOVE_RIGHT_BYTES, MOVE_RIGHT_BYTES_SIZE);
				code.push_back(static_cast<uint8_t>(size & 0xFF));
				size -= 256;
			}
			break;
		}
		case TokenType::MOVE_LEFT: {
			int size = tk.size;
			while (size > 0) {
				append_to_vector(code, MOVE_LEFT_BYTES, MOVE_LEFT_BYTES_SIZE);
				code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
				size -= 256;
			}
			break;
		}
		case TokenType::OUTPUT:
			append_to_vector(code, OUTPUT_BYTES_START, OUTPUT_BYTES_START_SIZE);
			for (size_t i = 0; i < tk.size; ++i) {
				append_to_vector(code, OUTPUT_BYTES_REPEAT, OUTPUT_BYTES_REPEAT_SIZE);
			}
			break;
		case TokenType::INPUT:
			// assert(0 && "NOT WORKING");
			append_to_vector(code, INPUT_BYTES_START, INPUT_BYTES_START_SIZE);
			for (size_t i = 0; i < tk.size; ++i) {
				append_to_vector(code, INPUT_BYTES_REPEAT, INPUT_BYTES_REPEAT_SIZE);
			}
			break;
		case TokenType::JUMP_FWD: {
			assert(tk.size <= tokens.size());
			append_to_vector(code, JUMP_FWD_BYTES, JUMP_FWD_BYTES_SIZE);
			size_t operand_byte_addr = code.size();
			// reserve space for the jump address (relative address)
			const std::array<uint8_t, 4> data = {0, 0, 0, 0};
			append_to_vector(code, data.data(), data.size());

			size_t src_byte_addr = code.size();
			Backpatch bp{};
			bp.operand_byte_addr = operand_byte_addr;
			bp.src_byte_addr = src_byte_addr;
			bp.dst_op_index = tk.size;
			backpatches.push_back(bp);
			break;
		}
		case TokenType::JUMP_BACK: {
			append_to_vector(code, JUMP_BACK_BYTES, JUMP_BACK_BYTES_SIZE);
			size_t operand_byte_addr = code.size();
			// reserve space for the jump address (relative address)
			const std::array<uint8_t, 4> data = {0, 0, 0, 0};
			append_to_vector(code, data.data(), data.size());

			size_t src_byte_addr = code.size();
			Backpatch bp{};
			bp.operand_byte_addr = operand_byte_addr;
			bp.src_byte_addr = src_byte_addr;
			bp.dst_op_index = tk.size;
			backpatches.push_back(bp);
			break;
		}
		default:
			assert(0 && "Invalid token type recieved");
		}
	}
	addresses.push_back(code.size());
	//printHexArray(code);
	for (const Backpatch& bp : backpatches) {
		assert(tokens[bp.dst_op_index - 1].operand == TokenType::JUMP_FWD ||
			   tokens[bp.dst_op_index - 1].operand == TokenType::JUMP_BACK);

		int32_t src_addr = static_cast<int32_t>(bp.src_byte_addr);
		int32_t dst_addr = static_cast<int32_t>(addresses[bp.dst_op_index]);
		int32_t operand = dst_addr - src_addr;
		memcpy(&code[bp.operand_byte_addr], &operand, sizeof(operand));
	}
	code.push_back(0xc3); // ret
}

void jit_run(const std::vector<uint8_t> code) {

	void* executableCode = getRawMemory(code.size());

	memcpy(executableCode, code.data(), code.size());
	makeMemoryExecutable(executableCode, code.size());
	uint8_t programMemory[JIT_MEMORY_CAP] = {0};

	((RunFunc)executableCode)(programMemory);
}
