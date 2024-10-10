#include <iostream>
#include "main.hpp"
#include <vector>
#include <cassert>
#include <cstring>
#include <cinttypes>


#ifdef __linux__
#include <sys/mman.h>

void* getExecutableMemory(size_t size) {
	void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		return nullptr;
	}
	return ptr;
}

bool makeMemoryExecutable(void* ptr, size_t size) {
	if (mprotect(ptr, size, PROT_READ | PROT_EXEC) != 0) {
		std::cerr << "Failed to make memory executable\n";
		return false;
	}
	return true;
}

void freeExectuableMemory(void*& ptr, size_t size) {
	if (ptr) {
		munmap(ptr, sizeof(ptr));
	}
	ptr = nullptr;
}

bool jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code) {
	std::cin.clear();
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

	return false;
}

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void* getExecutableMemory(size_t size) {
	void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!ptr) {
		return nullptr;
	}
	return ptr;
}

bool makeMemoryExecutable(void* ptr, size_t size) {
	DWORD oldProtection;
	if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &oldProtection)) {
		std::cerr << "Failed to make memory executable\n";
		return false;
	}
	return true;
}

void freeExecutableMemory(void*& ptr) {
	if (ptr) {
		VirtualFree(ptr, 0, MEM_RELEASE);
	}
	ptr = nullptr;
}

bool jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code) {
	std::cin.clear();
	assert(code.empty());

	std::vector<Backpatch> backpatches;
	std::vector<size_t> addresses;
	addresses.reserve(tokens.size());	// exact amount needed
	backpatches.reserve(tokens.size()); // assuming every operation is jump
	code.reserve(tokens.size() * 4);	// NOT EVEN CLOSE

	
	uintptr_t address_GetStdHandle = reinterpret_cast<uintptr_t>(&GetStdHandle);
	std::cout << address_GetStdHandle;
	uintptr_t address_WriteConsoleA = reinterpret_cast<uintptr_t>(&WriteConsoleA);
	GetStdHandle(-11);
	//uintptr_t address_ReadConsoleA =
	//	reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "ReadConsoleA"));
	; // Replace with actual address

	auto append_cstr_to_vector = [](std::vector<uint8_t>& vec, const char* str, size_t size = 0) {
		if (size == 0)
			size = strlen(str);
		vec.insert(vec.end(), str, str + size);
	};

	for (const Token& tk : tokens) {
		addresses.push_back(code.size());

		switch (tk.operand) {
		case TokenType::ADD:
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x80\x01"); // add byte[rcx],
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::SUB:
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x80\x29"); // sub byte[rcx],
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			break;
		case TokenType::MOVE_RIGHT: {
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x48\x83\xc1"); // add rcx,
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			// code.insert(code.end(), &operand, &operand + sizeof(operand)); // when operand = 1, it inserts "1 0 85 208"
			break;
		}
		case TokenType::MOVE_LEFT: {
			assert(tk.size < 256 && "TODO: support bigger operands");
			append_cstr_to_vector(code, "\x48\x83\xe9"); // sub rcx,
			uint32_t operand = static_cast<uint32_t>(tk.size);
			code.push_back(static_cast<uint8_t>(tk.size & 0xFF));
			// code.insert(code.end(), &operand, &operand + sizeof(operand));
			break;
		}
		case TokenType::OUTPUT: {
			append_cstr_to_vector(code, "\x48\xc7\xc7\xf5\xff\xff\xff"); // mov rdi, -11 (STD_OUTPUT_HANDLE)
			append_cstr_to_vector(code, "\xff\x15");                     // call GetStdHandle
			code.insert(code.end(), reinterpret_cast<uint8_t*>(&address_GetStdHandle),
						reinterpret_cast<uint8_t*>(&address_GetStdHandle) + sizeof(uintptr_t));
			append_cstr_to_vector(code, "\x48\x89\x6");                 // mov rsi, rax (stdout handle)
			append_cstr_to_vector(code, "\x8a\x01");	                // mov al, byte [rcx]
			append_cstr_to_vector(code, "\x48\xc7\xc2\x01\x00\x00\x00", 7); // mov rdx, 1
			for (size_t i = 0; i < tk.size; ++i) {
				append_cstr_to_vector(code, "\xff\x15"); // call GetStdHandle
				code.insert(code.end(), reinterpret_cast<uint8_t*>(&address_WriteConsoleA),
							reinterpret_cast<uint8_t*>(&address_WriteConsoleA) + sizeof(uintptr_t));
			}
			break;
		}
		case TokenType::INPUT: {
			for (size_t i = 0; i < tk.size; ++i) {
				assert(0 && "NOT IMPLEMENTED");
			}
			break;
		}
		case TokenType::JUMP_FWD: {
			assert(tk.size <= tokens.size());
			append_cstr_to_vector(code, "\x8a\x01"); // mov al, byte [rcx]
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
			append_cstr_to_vector(code, "\x8a\x01"); // mov al, byte [rcx]
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

	return false;
}
#endif