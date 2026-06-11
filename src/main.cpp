#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "Hooks.h"

#include "Diagnostics.h"
#include "KernelMouseInterface.h"

std::atomic<bool> g_WantsToUnhook{false};

// Main thread for the injected DLL
DWORD WINAPI MainThread(LPVOID lpReserved) {
    Diagnostics::Initialize();
    
    // Initialize Hooks (DX12 + ImGui)
    Hooks::Initialize();
    
    std::cout << "[Diagnostics] Hooks initialized successfully!" << std::endl;
    
    // Check KernelMouse connection
    KernelMouseInterface kernelMouse;
    if (kernelMouse.Initialize()) {
        std::cout << "[Diagnostics] KernelMouse Driver Detected & Ready to use!" << std::endl;
    } else {
        std::cout << "[Diagnostics] KernelMouse Driver NOT Detected!" << std::endl;
    }
    
    // Wait for End key to unload
    std::cout << "[Diagnostics] Waiting for End key to unhook..." << std::endl;
    while (!GetAsyncKeyState(VK_END)) {
        Sleep(100);
    }
    
    std::cout << "[Diagnostics] Unhooking and shutting down..." << std::endl;
    Hooks::RequestUnhook();
    Hooks::Unhook();
    
    // Small delay to allow threads to finish using hooked functions
    Sleep(1000);
    
    // Shutdown Diagnostics (Removes VEH and restores console streams)
    Diagnostics::Shutdown();
    
    FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(lpReserved), 0);
    return TRUE;
}

// DLL Entry Point
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hMod);
            CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            // CleanupHooks();
            break;
    }
    return TRUE;
}
