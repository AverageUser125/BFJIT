#pragma

#include <vector>
#include <cstdint>
struct Token;

void jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code);
void jit_run(const std::vector<uint8_t> code);

typedef void (*RunFunc)(void* memory);