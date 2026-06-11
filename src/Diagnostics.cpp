#include "Diagnostics.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

static std::ofstream g_LogFile;
static void* g_ExceptionHandlerHandle = nullptr;
static FILE* g_fDummyIn = nullptr;
static FILE* g_fDummyOut = nullptr;
static FILE* g_fDummyErr = nullptr;

class TeeBuf : public std::streambuf {
    std::streambuf* sb1;
    std::streambuf* sb2;
public:
    TeeBuf(std::streambuf* sb1, std::streambuf* sb2) : sb1(sb1), sb2(sb2) {}
protected:
    int overflow(int c) override {
        if (c == EOF) return !EOF;
        int r1 = sb1->sputc(c);
        int r2 = sb2->sputc(c);
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    int sync() override {
        int r1 = sb1->pubsync();
        int r2 = sb2->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }
};

static TeeBuf* g_TeeBuf = nullptr;
static std::streambuf* g_OriginalCoutBuf = nullptr;

LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    
    // Ignore common non-fatal exceptions
    if (exceptionCode == DBG_PRINTEXCEPTION_C || exceptionCode == 0x4001000A || exceptionCode == 0x406D1388) { 
        return EXCEPTION_CONTINUE_SEARCH;
    }

    PVOID exceptionAddress = pExceptionInfo->ExceptionRecord->ExceptionAddress;
    HMODULE hModule = GetModuleHandle(NULL);
    uintptr_t offset = (uintptr_t)exceptionAddress - (uintptr_t)hModule;

    if (g_LogFile.is_open()) {
        g_LogFile << "\n==================================================\n";
        g_LogFile << "[CRASH] FATAL EXCEPTION CAUGHT!\n";
        g_LogFile << "[CRASH] Exception Code: 0x" << std::hex << exceptionCode << std::endl;
        g_LogFile << "[CRASH] Exception Address: 0x" << exceptionAddress << std::endl;
        g_LogFile << "[CRASH] Offset from Game Base: 0x" << offset << std::endl;
        
        auto context = pExceptionInfo->ContextRecord;
#ifdef _WIN64
        g_LogFile << "[CRASH] RAX: 0x" << context->Rax << " | RBX: 0x" << context->Rbx << std::endl;
        g_LogFile << "[CRASH] RCX: 0x" << context->Rcx << " | RDX: 0x" << context->Rdx << std::endl;
        g_LogFile << "[CRASH] RSI: 0x" << context->Rsi << " | RDI: 0x" << context->Rdi << std::endl;
        g_LogFile << "[CRASH] RBP: 0x" << context->Rbp << " | RSP: 0x" << context->Rsp << std::endl;
        g_LogFile << "[CRASH] RIP: 0x" << context->Rip << std::endl;
#else
        g_LogFile << "[CRASH] EAX: 0x" << context->Eax << " | EBX: 0x" << context->Ebx << std::endl;
        g_LogFile << "[CRASH] ECX: 0x" << context->Ecx << " | EDX: 0x" << context->Edx << std::endl;
        g_LogFile << "[CRASH] ESI: 0x" << context->Esi << " | EDI: 0x" << context->Edi << std::endl;
        g_LogFile << "[CRASH] EBP: 0x" << context->Ebp << " | ESP: 0x" << context->Esp << std::endl;
        g_LogFile << "[CRASH] EIP: 0x" << context->Eip << std::endl;
#endif
        g_LogFile << "==================================================\n\n";
        g_LogFile.flush();
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void Diagnostics::Initialize() {
    AllocConsole();
    freopen_s(&g_fDummyIn, "CONIN$", "r", stdin);
    freopen_s(&g_fDummyOut, "CONOUT$", "w", stdout);
    freopen_s(&g_fDummyErr, "CONOUT$", "w", stderr);

    // Ensure the Diagnostics directory exists
    const char* diagDir = "M:\\MCP Server\\Trainers\\AutoLogin and Menu Bypass\\Diagnostics";
    CreateDirectoryA(diagDir, NULL);
    
    // Check if the current log exists, and if so, archive it
    const char* logPath = "M:\\MCP Server\\Trainers\\AutoLogin and Menu Bypass\\Diagnostics\\AutoLogin_Diagnostics.log";
    DWORD fileAttr = GetFileAttributesA(logPath);
    if (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        const char* archiveDir = "M:\\MCP Server\\Trainers\\AutoLogin and Menu Bypass\\Diagnostics\\Archive";
        CreateDirectoryA(archiveDir, NULL);
        
        // Generate timestamp
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
        localtime_s(&buf, &in_time_t);
        
        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", &buf);
        
        std::string newLogName = std::string(archiveDir) + "\\AutoLogin_Diagnostics_" + timeStr + ".log";
        MoveFileA(logPath, newLogName.c_str());
    }

    // Open a log file in the specified directory
    g_LogFile.open(logPath, std::ios::out | std::ios::trunc);
    if (g_LogFile.is_open()) {
        g_OriginalCoutBuf = std::cout.rdbuf();
        g_TeeBuf = new TeeBuf(g_OriginalCoutBuf, g_LogFile.rdbuf());
        std::cout.rdbuf(g_TeeBuf);
        std::cerr.rdbuf(g_TeeBuf);
    }
    
    std::cout << "[Diagnostics] Console and File Logging Allocated Successfully!" << std::endl;
    std::cout << "[Diagnostics] Build Version: " << __DATE__ << " " << __TIME__ << std::endl;

    // Log Process ID and Base Address
    HMODULE hModule = GetModuleHandle(NULL);
    DWORD pid = GetCurrentProcessId();
    std::cout << "[Diagnostics] Process ID (PID): " << pid << std::endl;
    std::cout << "[Diagnostics] Game Base Address: 0x" << std::hex << (uintptr_t)hModule << std::dec << std::endl;
    
    // Log Critical Global Offsets
    std::cout << "[Diagnostics] --- Engine Global Offsets ---" << std::endl;
    std::cout << "[Diagnostics] GObjects: 0x" << std::hex << 0x0A62C060 << std::endl;
    std::cout << "[Diagnostics] GWorld: 0x" << std::hex << 0x0A7B8DB8 << std::endl;
    std::cout << "[Diagnostics] ProcessEvent: 0x" << std::hex << 0x01542580 << std::dec << std::endl;
    
    // Log Class Member Offsets
    std::cout << "[Diagnostics] --- Class Member Offsets ---" << std::endl;
    std::cout << "[Diagnostics] AController::Pawn Offset: 0x02F8" << std::endl;
    std::cout << "[Diagnostics] APlayerController::AcknowledgedPawn Offset: 0x0360" << std::endl;
    std::cout << "[Diagnostics] ACharacter::CharacterMovement Offset: 0x0318" << std::endl;
    std::cout << "[Diagnostics] Pointer Validation Check: Enabled." << std::endl;

    g_ExceptionHandlerHandle = AddVectoredExceptionHandler(1, VectoredExceptionHandler);
    if (g_ExceptionHandlerHandle) {
        std::cout << "[Diagnostics] Vectored Exception Handler installed successfully." << std::endl;
    } else {
        std::cout << "[Diagnostics] Failed to install Exception Handler!" << std::endl;
    }
}

void Diagnostics::Shutdown() {
    std::cout << "[Diagnostics] Shutting down diagnostics..." << std::endl;
    
    if (g_ExceptionHandlerHandle) {
        RemoveVectoredExceptionHandler(g_ExceptionHandlerHandle);
        g_ExceptionHandlerHandle = nullptr;
    }
    
    if (g_OriginalCoutBuf) {
        std::cout.rdbuf(g_OriginalCoutBuf);
        std::cerr.rdbuf(g_OriginalCoutBuf);
    }
    if (g_TeeBuf) {
        delete g_TeeBuf;
        g_TeeBuf = nullptr;
    }
    if (g_LogFile.is_open()) {
        g_LogFile.close();
    }
    
    if (g_fDummyIn) fclose(g_fDummyIn);
    if (g_fDummyOut) fclose(g_fDummyOut);
    if (g_fDummyErr) fclose(g_fDummyErr);
    
    FreeConsole();
}

void Diagnostics::ClearConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
    std::cout << "[Diagnostics] Console cleared!" << std::endl;
}
