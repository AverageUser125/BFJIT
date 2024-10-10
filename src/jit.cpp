#include "jit.hpp"
#include "main.hpp"
#include "executableMemory.hpp"
#include <cassert>
#include <cstring>

#if PLATFORM_WINDOWS
static const uintptr_t address_putchar = reinterpret_cast<uintptr_t>(&putchar);
#endif

void jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code) {
	assert(code.empty());

	std::vector<Backpatch> backpatches;
	std::vector<size_t> addresses;
	addresses.reserve(tokens.size());	// exact amount needed
	backpatches.reserve(tokens.size()); // assuming every operation is jump
	code.reserve(tokens.size() * 4);	// NOT EVEN CLOSE


	auto append_cstr_to_vector = [](std::vector<uint8_t>& vec, const char* str, size_t size = 0) {
		if (size == 0)
			size = strlen(str);
		vec.insert(vec.end(), str, str + size);
	};

#if PLATFORM_WINDOWS
	auto push_uintptr = [](std::vector<uint8_t>& vec, uintptr_t value) {
		// little endian storing
		for (size_t i = 0; i < sizeof(uintptr_t); ++i) {
			vec.push_back(static_cast<uint8_t>(value & 0xFF));
			value >>= 8; // Shift right by 8 bits to get the next byte
		}
	};
	// calling convention stuff
	append_cstr_to_vector(code, "\x48\x89\xcf"); // mov rdi, rcx
	append_cstr_to_vector(code, "\x48\x31\xc9"); // xor rcx, rcx (calling convention);
#endif

	for (const Token& tk : tokens) {
		addresses.push_back(code.size());

		switch (tk.operand) {
		case TokenType::ADD:
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x80\x07"); // add byte[rdi],
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::SUB:
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x80\x2f"); // sub byte[rdi],
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::MOVE_RIGHT: {
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x48\x83\xc7"); // add rdi,
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			// code.insert(code.end(), &operand, &operand + sizeof(operand)); // when operand = 1, it inserts "1 0 85 208"
			break;
		}
		case TokenType::MOVE_LEFT: {
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x48\x83\xef"); // sub rdi,
			uint32_t operand = static_cast<uint32_t>(tk.size);
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			// code.insert(code.end(), &operand, &operand + sizeof(operand));
			break;
		}
#if PLATFORM_WINDOWS
		case TokenType::OUTPUT: {
			// append_cstr_to_vector(code, "\x48\x31\xc0"); // xor rax, rax
			append_cstr_to_vector(code, "\x8a\x0f"); // mov cl, byte [rdi]
			append_cstr_to_vector(code, "\x48\xba"); // mov rdx,
			push_uintptr(code, address_putchar);	 // mov 8 bytes
			for (size_t i = 0; i < tk.size; ++i) {

				append_cstr_to_vector(code, "\x48\x83\xec\x28"); // sub rsp, 40
				append_cstr_to_vector(code, "\xff\xd2");		 // call rdx
				append_cstr_to_vector(code, "\x48\x83\xc4\x28"); // add rsp, 40
			}
			break;
		}
		case TokenType::INPUT: {
			for (size_t i = 0; i < tk.size; ++i) {
				assert(0 && "NOT IMPLEMENTED");
			}
			break;
		}
#elif PLATFORM_LINUX
		case TokenType::OUTPUT:
			for (size_t i = 0; i < tk.size; ++i) {
				append_cstr_to_vector(code, "\x57");							// push rdi
				append_cstr_to_vector(code, "\x48\xc7\xc0\x01\x00\x00\x00", 7); // mov rax, 1
				append_cstr_to_vector(code, "\x48\x89\xfe");					// mov rsi, rdi
				append_cstr_to_vector(code, "\x48\xc7\xc7\x01\x00\x00\x00", 7); // mov rdi, 1
				append_cstr_to_vector(code, "\x48\xc7\xc2\x01\x00\x00\x00", 7); // mov rdx, 1
				append_cstr_to_vector(code, "\x0f\x05");						// syscall
				append_cstr_to_vector(code, "\x5f");							// pop rdi
			}
			break;
		case TokenType::INPUT:
			assert(0 && "NOT WORKING");
			for (size_t i = 0; i < tk.size; ++i) {
				append_cstr_to_vector(code, "\x57");							// push rdi
				append_cstr_to_vector(code, "\x48\xc7\xc0\x01\x00\x00\x00", 7); // mov rax, 1
				append_cstr_to_vector(code, "\x48\x89\xfe");					// mov rsi, rdi
				append_cstr_to_vector(code, "\x48\xc7\xc7\x00\x00\x00\x00", 7); // mov rdi, 0
				append_cstr_to_vector(code, "\x48\xc7\xc2\x01\x00\x00\x00", 7); // mov rdx, 1
				append_cstr_to_vector(code, "\x0f\x05");						// syscall
				append_cstr_to_vector(code, "\x5f");							// pop rdi
			}
			break;
#endif
		case TokenType::JUMP_FWD: {
			assert(tk.size <= tokens.size());
			append_cstr_to_vector(code, "\x8a\x07"); // mov al, byte [rdi]
			append_cstr_to_vector(code, "\x84\xc0"); // test al, al
			append_cstr_to_vector(code, "\x0f\x84"); // jz
			size_t operand_byte_addr = code.size();
			append_cstr_to_vector(code, "\x00\x00\x00\x00", 4);
			size_t src_byte_addr = code.size();
			Backpatch bp{};
			bp.operand_byte_addr = operand_byte_addr;
			bp.src_byte_addr = src_byte_addr;
			bp.dst_op_index = tk.size;
			backpatches.push_back(bp);
			break;
		}
		case TokenType::JUMP_BACK: {
			append_cstr_to_vector(code, "\x8a\x07"); // mov al, byte [rdi]
			append_cstr_to_vector(code, "\x84\xc0"); // test al, al
			append_cstr_to_vector(code, "\x0f\x85"); // jnz
			size_t operand_byte_addr = code.size();
			append_cstr_to_vector(code, "\x00\x00\x00\x00", 4);
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
	append_cstr_to_vector(code, "\xC3"); // ret
}

void jit_run(const std::vector<uint8_t> code) {

	void* executableCode = getExecutableMemory(code.size());

	memcpy(executableCode, code.data(), code.size());
	makeMemoryExecutable(executableCode, code.size());
	uint8_t programMemory[JIT_MEMORY_CAP] = {0};

	((RunFunc)executableCode)(programMemory);
}
