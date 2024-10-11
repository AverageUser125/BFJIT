#pragma once


struct Token;

void* getRawMemory(size_t size);
void freeExectuableMemory(void*& ptr, size_t size);
bool makeMemoryExecutable(void* ptr, size_t size);
