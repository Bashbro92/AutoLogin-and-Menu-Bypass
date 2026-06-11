#include "PatternScanner.h"
#include <psapi.h>
#include <sstream>

uintptr_t PatternScanner::ResolveRIP(uintptr_t instructionAddress, int offset, int instructionSize) {
    if (!instructionAddress) return 0;
    int32_t displacement = *(int32_t*)(instructionAddress + offset);
    return instructionAddress + instructionSize + displacement;
}

uintptr_t PatternScanner::FindPattern(const std::string& moduleName, const std::string& pattern) {
    HMODULE hModule = GetModuleHandleA(moduleName.empty() ? nullptr : moduleName.c_str());
    if (!hModule) return 0;

    MODULEINFO moduleInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(MODULEINFO))) {
        return 0;
    }

    uint8_t* base = (uint8_t*)moduleInfo.lpBaseOfDll;
    uint32_t size = moduleInfo.SizeOfImage;

    std::vector<int> patternBytes;
    std::istringstream iss(pattern);
    std::string word;
    while (iss >> word) {
        if (word == "?" || word == "??") {
            patternBytes.push_back(-1);
        } else {
            patternBytes.push_back(std::stoi(word, nullptr, 16));
        }
    }

    size_t patternLength = patternBytes.size();
    if (patternLength == 0 || size < patternLength) return 0;

    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* current = base;

    while (current < base + size) {
        if (!VirtualQuery(current, &mbi, sizeof(mbi))) {
            break;
        }

        bool isReadable = (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
        bool isGuarded = (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0;

        if (mbi.State == MEM_COMMIT && isReadable && !isGuarded) {
            uint8_t* regionEnd = (uint8_t*)mbi.BaseAddress + mbi.RegionSize;
            uint8_t* scanEnd = (regionEnd > base + size) ? base + size : regionEnd;

            if (scanEnd > current && (size_t)(scanEnd - current) >= patternLength) {
                for (uint8_t* p = current; p <= scanEnd - patternLength; p++) {
                    bool found = true;
                    for (size_t j = 0; j < patternLength; j++) {
                        if (patternBytes[j] != -1 && p[j] != (uint8_t)patternBytes[j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) {
                        return (uintptr_t)p;
                    }
                }
            }
        }
        current = (uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}
