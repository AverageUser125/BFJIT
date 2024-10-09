#include <iostream>

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
		return 1;
	}
}

void freeExecutableMemory(void*& ptr) {
	if (ptr) {
		VirtualFree(ptr, 0, MEM_RELEASE);
	}
	ptr = nullptr;
}
#endif