#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <stack>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cinttypes>
#include <jitSpecific.hpp>
#include <iomanip> // For std::hex and std::setfill

void printHexArray(const std::vector<uint8_t>& data) {
	std::cout << "{ ";
	for (size_t i = 0; i < data.size(); ++i) {
		std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
		if (i != data.size() - 1) {
			std::cout << ", ";
		}
	}
	std::cout << " }" << std::endl;
}

// #define JIT_MEMORY_CAP (10 * 1000 * 1000)

#define JIT_MEMORY_CAP 512

enum class TokenType {
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
	TokenType operand;
	size_t size;

	Token(TokenType _kind, size_t _size): operand(_kind), size(_size) {
	}

	Token(size_t _size, TokenType _kind) : operand(_kind), size(_size) {
	}
};

struct Backpatch {
	size_t operand_byte_addr;
	size_t src_byte_addr;
	size_t dst_op_index;
};

typedef void (*RunFunc)(void* memory);

std::string fileToString(const std::string& filename) {
	std::ifstream f(filename);
	if (!f) {
		assert(0);
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

bool tokenizer(const std::string& code, std::vector<Token>& tokens) {
	std::stack<size_t, std::vector<size_t>> jumpAddresses;

    for (size_t i = 0; i < code.length(); ++i) {
		switch (code[i]) {
		case '+':
		case '-':
		case '>':
		case '<':
		case '.':
		case ',': {
			// compress adjacent same operations
			size_t count = 1;
			while (i + 1 < code.length() && code[i + 1] == code[i]) {
				++count;
				++i;
			}
			tokens.push_back({static_cast<TokenType>(code[i]), count});
			break;
		}
		case '[': {
			size_t addr = tokens.size();
			tokens.push_back({TokenType::JUMP_FWD, 0}); // Placeholder for jump target
			jumpAddresses.push(addr); // store current place for now
			break;
		}
		case ']': {
			if (jumpAddresses.empty()) {
				std::cerr << "STACK UNDERFLOW\n";
				return false;
			}
			assert(!jumpAddresses.empty());
			size_t jumpDestination = jumpAddresses.top(); // get the stored location
			jumpAddresses.pop();	
			tokens.push_back({TokenType::JUMP_BACK, jumpDestination + 1}); // Jump back to the corresponding '['
			assert(tokens[jumpDestination].operand == TokenType::JUMP_FWD); // making sure there ain't any error
			tokens[jumpDestination].size = tokens.size();					// set the jump to to current position
			break;
		}
		default:
			// Ignore any characters that are not Brainf**k commands
			break;
		}
	}


	if (!jumpAddresses.empty()) {
		std::cerr << "Error: Unmatched opening bracket '['\n";
		return false;
	}

	return true;
}

void interpretor(const std::vector<Token>& tokens) {

	std::vector<uint8_t> buffer;
	buffer.resize(512);
	int idx = 0;
	size_t tokenIdx = 0;

	while (tokenIdx < tokens.size()) {
		const Token& tk = tokens[tokenIdx];
		switch (tk.operand) {
		case TokenType::ADD:
			buffer[idx] += tk.size;
			tokenIdx++;
			break;
		case TokenType::SUB:
			buffer[idx] -= tk.size;
			tokenIdx++;
			break;
		case TokenType::MOVE_RIGHT:
			idx += tk.size;
			if (idx >= buffer.size()) {
				// Resize the buffer if we move beyond its current size
				buffer.resize(idx + tk.size + 1); // Increase buffer size to fit the new index
			}
			tokenIdx++;
			break;
		case TokenType::MOVE_LEFT:
			idx -= tk.size;
			tokenIdx++;
			break;
		case TokenType::OUTPUT:
			std::cout << std::string(tk.size, buffer[idx]);
			tokenIdx++;
			break;
		case TokenType::INPUT:
			buffer[idx] = std::cin.get();
			tokenIdx++;
			break;
		case TokenType::JUMP_FWD:
			if (buffer[idx] == 0) {
				tokenIdx = tokens[tokenIdx].size;
			} else {
				tokenIdx++;
			}
			break;
		case TokenType::JUMP_BACK:
			if (buffer[idx] != 0) {
				tokenIdx = tokens[tokenIdx].size;
			} else {		
				tokenIdx++;
			}
			break;

		default:
			assert(0 && "Invalid token type recieved");
		}
	}

}

bool jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code) {
	std::cin.clear();
	assert(code.empty());

	std::vector<Backpatch> backpatches;
	std::vector<size_t> addresses;
	addresses.reserve(tokens.size()); // exact amount needed
	backpatches.reserve(tokens.size()); // assuming every operation is jump
	code.reserve(tokens.size() * 4); // NOT EVEN CLOSE
	

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
				append_cstr_to_vector(code, "\x57");						    // push rdi
				append_cstr_to_vector(code, "\x48\xc7\xc0\x01\x00\x00\x00", 7); // mov rax, 1
				append_cstr_to_vector(code, "\x48\x89\xfe");					// mov rsi, rdi
				append_cstr_to_vector(code, "\x48\xc7\xc7\x00\x00\x00\x00", 7); // mov rdi, 0
				append_cstr_to_vector(code, "\x48\xc7\xc2\x01\x00\x00\x00", 7); // mov rdx, 1
				append_cstr_to_vector(code, "\x0f\x05");					    // syscall
				append_cstr_to_vector(code, "\x5f");						    // pop rdi
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

int main() {
	std::string stringCode = fileToString(RESOURCES_PATH "hello.bf"); 
	std::vector<Token> tokens;
	if (!tokenizer(stringCode, tokens)) {
		std::cerr << "Tokenization failed\n";
		return EXIT_FAILURE;
	}

	// interpretor(tokens);
	std::cout << '\n';
	// return EXIT_SUCCESS;

	std::vector<uint8_t> rawCode;
	jit_compile(tokens, rawCode);
	// printHexArray(rawCode);
	void* executableCode = getExecutableMemory(rawCode.size());
	
	memcpy(executableCode, rawCode.data(), rawCode.size());
	makeMemoryExecutable(executableCode, rawCode.size());
	uint8_t programMemory[JIT_MEMORY_CAP * 2] = {0};

	((RunFunc)executableCode)(programMemory);
	// interpretor(tokens);

	return EXIT_SUCCESS;
}