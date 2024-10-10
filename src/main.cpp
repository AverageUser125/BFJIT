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
#include <iomanip> // For std::hex and std::setfill
#include "main.hpp"
#include "runner.hpp"
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

int main() {
	std::string stringCode = fileToString(RESOURCES_PATH "mandelbrot.bf"); 
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
	jit_run(rawCode);
	// interpretor(tokens);

	return EXIT_SUCCESS;
}