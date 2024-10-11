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
#include "jit.hpp"
#include <termios.h>
#include <stdio.h>

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

bool parseArguments(int argc, char* argv[], std::string& inputFile) {
	assert(inputFile.empty());

	#ifndef NDEBUG
	if (argc == 1) {
		inputFile = RESOURCES_PATH "cat.bf";
		return false;
	}
	#endif

	// Check valid argument count
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: " << argv[0] << " [--no-jit] <input.bf>\n";
		return false; // Indicates an error
	}

	bool noJitFlag = false;

	// Check for the JIT flag and the input file
	if (argc == 3) { // Two arguments
		if (std::string(argv[1]) == "--no-jit") {
			noJitFlag = true;
			inputFile = argv[2]; // The input file is the second argument
		} else if (std::string(argv[2]) == "--no-jit") {
			inputFile = argv[1]; // The input file is the first argument
			noJitFlag = true;
		} else {
			std::cerr << "Error: Invalid argument '" << argv[2] << "'.\n";
			return false; // Indicates an error
		}
	} else {				 // argc == 2, only one argument is provided
		inputFile = argv[1]; // The input file is the only argument
	}

	// Validate that an input file is specified
	if (inputFile.empty()) {
		std::cerr << "Usage: " << argv[0] << " <input.bf> [--no-jit]\n";
		return false; // Indicates an error
	}

	return noJitFlag; // Return JIT flag status
}

int main(int argc, char** argv) {
	std::string filepath = ""; 
	bool no_jit = parseArguments(argc, argv, filepath);
	if (filepath.empty()) {
		return EXIT_FAILURE;
	}
	std::string stringCode = fileToString(filepath); 
	std::vector<Token> tokens;
	if (!tokenizer(stringCode, tokens)) {
		std::cerr << "Tokenization failed\n";
		return EXIT_FAILURE;
	}
	#if PLATFORM_LINUX
	static struct termios old, current;

	tcgetattr(0, &old);			/* grab old terminal i/o settings */
	current = old;				/* make new settings same as old settings */
	current.c_lflag &= ~ICANON; /* disable buffered i/o */
	current.c_lflag &= ~ECHO; /* set no echo mode */
	tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
    #endif

	if (no_jit) {
		interpretor(tokens);
	} else {
		std::vector<uint8_t> rawCode;
		jit_compile(tokens, rawCode);
		jit_run(rawCode);
	}
    #if PLATFORM_LINUX
	tcsetattr(0, TCSANOW, &old);
	#endif
	return EXIT_SUCCESS;
}