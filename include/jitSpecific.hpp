#pragma once


struct Token;

void* getExecutableMemory(size_t size);
void freeExectuableMemory(void*& ptr, size_t size);
bool makeMemoryExecutable(void* ptr, size_t size);
bool jit_compile(const std::vector<Token>& tokens, std::vector<uint8_t>& code);