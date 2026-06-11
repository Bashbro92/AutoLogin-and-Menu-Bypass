#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <cstdint>

class PatternScanner {
public:
    // Finds an IDA-style string pattern (e.g. "48 8B 05 ? ? ? ? 48 85 C0") in the specified module.
    // If moduleName is empty, it searches the main executable.
    static uintptr_t FindPattern(const std::string& moduleName, const std::string& pattern);
    
    // Resolves a Relative Instruction Pointer (RIP) displacement address.
    // offset: How many bytes into the instruction the 4-byte displacement starts.
    // instructionSize: The total length of the instruction in bytes.
    static uintptr_t ResolveRIP(uintptr_t instructionAddress, int offset, int instructionSize);
};
