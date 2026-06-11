#include <winsock2.h>
#include "Hooks.h"
#include "Boilerplate.h"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d11.h>
#include <iostream>
#include "kiero.hpp"
#include "kiero_d3d12.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"

#pragma comment(lib, "ws2_32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Statics
HWND Hooks::m_GameWindow = nullptr;
WNDPROC Hooks::m_OriginalWndProc = nullptr;
bool Hooks::m_IsImGuiInitialized = false;

ID3D12Device* Hooks::m_pDevice = nullptr;
ID3D12CommandQueue* Hooks::m_pCommandQueue = nullptr;
ID3D12DescriptorHeap* Hooks::m_pRtvDescHeap = nullptr;
ID3D12DescriptorHeap* Hooks::m_pSrvDescHeap = nullptr;
ID3D12GraphicsCommandList* Hooks::m_pCommandList = nullptr;
FrameContext* Hooks::m_FrameContexts = nullptr;
UINT Hooks::m_NumFramesInFlight = 0;

Hooks::Present_t Hooks::m_OriginalPresent = nullptr;
Hooks::ExecuteCommandLists_t Hooks::m_OriginalExecuteCommandLists = nullptr;
Hooks::ResizeBuffers_t Hooks::m_OriginalResizeBuffers = nullptr;
Hooks::ProcessEvent_t Hooks::m_OriginalProcessEvent = nullptr;

typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT lpPoint);
static GetCursorPos_t m_OriginalGetCursorPos = nullptr;

typedef SHORT(WINAPI* GetAsyncKeyState_t)(int vKey);
static GetAsyncKeyState_t m_OriginalGetAsyncKeyState = nullptr;

typedef int(WSAAPI* send_t)(SOCKET s, const char* buf, int len, int flags);
static send_t m_OriginalSend = nullptr;

typedef int(WSAAPI* recv_t)(SOCKET s, char* buf, int len, int flags);
static recv_t m_OriginalRecv = nullptr;

BOOL WINAPI Hooks::HookedGetCursorPos(LPPOINT lpPoint) {
    BOOL result = m_OriginalGetCursorPos(lpPoint);
    // Boilerplate spoof mouse logic if needed
    return result;
}

SHORT WINAPI Hooks::HookedGetAsyncKeyState(int vKey) {
    // Boilerplate key interception logic
    // if (Boilerplate::GetInstance()->IsKeyBlocked(vKey)) return 0;
    return m_OriginalGetAsyncKeyState(vKey);
}

int WSAAPI Hooks::HookedSend(SOCKET s, const char* buf, int len, int flags) {
    // Example: log or modify packet here
    // std::cout << "[Network] Sending " << len << " bytes..." << std::endl;
    return m_OriginalSend(s, buf, len, flags);
}

int WSAAPI Hooks::HookedRecv(SOCKET s, char* buf, int len, int flags) {
    int ret = m_OriginalRecv(s, buf, len, flags);
    // Example: log or modify incoming packet here
    // if (ret > 0) std::cout << "[Network] Received " << ret << " bytes..." << std::endl;
    return ret;
}

extern std::atomic<bool> g_WantsToUnhook;

LRESULT WINAPI Hooks::HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_WantsToUnhook) {
        if (m_OriginalWndProc) {
            WNDPROC original = m_OriginalWndProc;
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)original);
            m_OriginalWndProc = nullptr;
            return CallWindowProc(original, hWnd, msg, wParam, lParam);
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    
    if (m_IsImGuiInitialized && ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        LRESULT ImGuiResult = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        if (ImGuiResult) return ImGuiResult;
        
        if (io.WantCaptureMouse) {
            if (msg == WM_INPUT || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSEWHEEL) {
                return 0; // Block game only when interacting with menu
            }
        }
        if (io.WantCaptureKeyboard && (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)) {
            return 0;
        }
    }
    
    return CallWindowProc(m_OriginalWndProc, hWnd, msg, wParam, lParam);
}

void WINAPI Hooks::HookedExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    if (!m_pCommandQueue && pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        m_pCommandQueue = pCommandQueue;
        std::cout << "[Hooks] Captured ID3D12CommandQueue: 0x" << m_pCommandQueue << std::endl;
    }
    return m_OriginalExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

void Hooks::HookedProcessEvent(const void* Obj, void* Function, void* Parms) {
    if (Boilerplate::GetInstance()->HasTasks() && Function) {
        SDK::UFunction* func = static_cast<SDK::UFunction*>(Function);
        std::string name = func->GetName();
        if (name.find("Tick") != std::string::npos || name.find("ReceiveTick") != std::string::npos) {
            Boilerplate::GetInstance()->ExecuteTasks();
        }
    }
    m_OriginalProcessEvent(Obj, Function, Parms);
}

HRESULT WINAPI Hooks::HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (m_IsImGuiInitialized && m_FrameContexts) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
        for (UINT i = 0; i < m_NumFramesInFlight; i++) {
            if (m_FrameContexts[i].MainRenderTargetResource) {
                m_FrameContexts[i].MainRenderTargetResource->Release();
                m_FrameContexts[i].MainRenderTargetResource = nullptr;
            }
        }
    }

    HRESULT hr = m_OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (SUCCEEDED(hr) && m_IsImGuiInitialized && m_FrameContexts) {
        SIZE_T rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < m_NumFramesInFlight; i++) {
            pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_FrameContexts[i].MainRenderTargetResource);
            m_pDevice->CreateRenderTargetView(m_FrameContexts[i].MainRenderTargetResource, nullptr, rtvHandle);
            rtvHandle.ptr += rtvDescriptorSize;
        }
        ImGui_ImplDX12_CreateDeviceObjects();
    }

    return hr;
}

HRESULT WINAPI Hooks::HookedPresent(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!m_pCommandQueue) {
        return m_OriginalPresent(pSwapChain, SyncInterval, Flags);
    }

    if (!m_IsImGuiInitialized) {
        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        if (sd.BufferDesc.Width < 100 || sd.BufferDesc.Height < 100) {
            return m_OriginalPresent(pSwapChain, SyncInterval, Flags);
        }

        std::cout << "[Hooks] Attempting to initialize ImGui for DX12..." << std::endl;
        
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&m_pDevice))) {
            m_GameWindow = sd.OutputWindow;
            m_NumFramesInFlight = sd.BufferCount;
            
            std::cout << "[Hooks] DX12 Device: 0x" << m_pDevice << ", Buffers: " << m_NumFramesInFlight << " (" << sd.BufferDesc.Width << "x" << sd.BufferDesc.Height << ")" << std::endl;

            // Create descriptor heaps
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NumDescriptors = m_NumFramesInFlight;
            rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            rtvDesc.NodeMask = 1;
            m_pDevice->CreateDescriptorHeap(&rtvDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pRtvDescHeap);

            D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
            srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvDesc.NumDescriptors = 50;
            srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            m_pDevice->CreateDescriptorHeap(&srvDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pSrvDescHeap);

            // Allocate frames
            m_FrameContexts = new FrameContext[m_NumFramesInFlight];
            SIZE_T rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < m_NumFramesInFlight; i++) {
                m_FrameContexts[i].MainRenderTargetDescriptor = rtvHandle;
                pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_FrameContexts[i].MainRenderTargetResource);
                m_pDevice->CreateRenderTargetView(m_FrameContexts[i].MainRenderTargetResource, nullptr, rtvHandle);
                m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_FrameContexts[i].CommandAllocator);
                rtvHandle.ptr += rtvDescriptorSize;
            }

            m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_FrameContexts[0].CommandAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&m_pCommandList);
            m_pCommandList->Close();

            m_OriginalWndProc = (WNDPROC)SetWindowLongPtr(m_GameWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(m_GameWindow);
            
            ImGui_ImplDX12_InitInfo init_info = {};
            init_info.Device = m_pDevice;
            init_info.CommandQueue = m_pCommandQueue;
            init_info.NumFramesInFlight = m_NumFramesInFlight;
            init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
            init_info.SrvDescriptorHeap = m_pSrvDescHeap;
            init_info.LegacySingleSrvCpuDescriptor = m_pSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
            init_info.LegacySingleSrvGpuDescriptor = m_pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
            ImGui_ImplDX12_Init(&init_info);

            m_IsImGuiInitialized = true;
            std::cout << "[Hooks] ImGui DX12 Initialized Successfully!" << std::endl;
        } else {
            std::cout << "[Hooks] FAILED to extract DX12 Device from SwapChain!" << std::endl;
            m_IsImGuiInitialized = true; // Prevent spam
        }
    }

    if (m_IsImGuiInitialized && m_pCommandQueue) {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Let the Boilerplate render its UI here
        Boilerplate::GetInstance()->DrawUI();

        ImGui::Render();

        UINT backBufferIdx = pSwapChain->GetCurrentBackBufferIndex();
        FrameContext& frame = m_FrameContexts[backBufferIdx];

        frame.CommandAllocator->Reset();
        m_pCommandList->Reset(frame.CommandAllocator, nullptr);
        
        // Transition backbuffer from PRESENT to RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = frame.MainRenderTargetResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_pCommandList->ResourceBarrier(1, &barrier);

        m_pCommandList->OMSetRenderTargets(1, &frame.MainRenderTargetDescriptor, FALSE, nullptr);
        
        ID3D12DescriptorHeap* descriptorHeaps[] = { m_pSrvDescHeap };
        m_pCommandList->SetDescriptorHeaps(1, descriptorHeaps);
        
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommandList);

        // Transition backbuffer from RENDER_TARGET to PRESENT
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_pCommandList->ResourceBarrier(1, &barrier);

        m_pCommandList->Close();

        m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pCommandList);
    }

    return m_OriginalPresent(pSwapChain, SyncInterval, Flags);
}

bool Hooks::Initialize() {
    std::cout << "[Hooks] Resolving Dynamic AOB Signatures..." << std::endl;
    Boilerplate::GetInstance()->InitializeSDK();

    std::cout << "[Hooks] Initializing MinHook for DX12..." << std::endl;
    if (MH_Initialize() != MH_OK) return false;
    std::cout << "[Hooks] Waiting for d3d12.dll and dxgi.dll to be loaded..." << std::endl;
    while (!GetModuleHandleA("d3d12.dll") || !GetModuleHandleA("dxgi.dll")) {
        Sleep(100);
    }

    HWND hWindow = GetForegroundWindow();
    if (!hWindow) return false;

    // Create a dummy window to prevent DXGI from crashing the game's actual SwapChain
    kiero::D3D12Output d3d12;
    kiero::Error kieroError = kiero::locate<kiero::Implementation_D3D12>(nullptr, &d3d12);
    if (kieroError != kiero::Error_Nil) {
        std::cout << "[Hooks] Kiero2 failed to locate D3D12 functions! Error Code: " << kieroError << std::endl;
        return false;
    }

    void* pExecuteCommandLists = d3d12.command_queue_methods[10]; // ID3D12CommandQueue::ExecuteCommandLists is index 10
    void* pPresent = d3d12.swapchain_methods[8]; // IDXGISwapChain::Present is index 8
    void* pResizeBuffers = d3d12.swapchain_methods[13]; // IDXGISwapChain::ResizeBuffers is index 13
    
    std::cout << "[Hooks] Found ExecuteCommandLists: 0x" << pExecuteCommandLists << std::endl;
    std::cout << "[Hooks] Found Present: 0x" << pPresent << std::endl;

    if (MH_CreateHook(pExecuteCommandLists, (LPVOID)HookedExecuteCommandLists, (LPVOID*)&m_OriginalExecuteCommandLists) != MH_OK) {
        std::cout << "[Hooks] Failed to hook ExecuteCommandLists!" << std::endl;
        return false;
    }
    
    if (MH_CreateHook(pPresent, (LPVOID)HookedPresent, (LPVOID*)&m_OriginalPresent) != MH_OK) {
        std::cout << "[Hooks] Failed to hook Present!" << std::endl;
        return false;
    }

    if (MH_CreateHook(pResizeBuffers, (LPVOID)HookedResizeBuffers, (LPVOID*)&m_OriginalResizeBuffers) != MH_OK) {
        std::cout << "[Hooks] Failed to hook ResizeBuffers!" << std::endl;
        return false;
    }

    void* pProcessEvent = (void*)(SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent);
    if (MH_CreateHook(pProcessEvent, (LPVOID)HookedProcessEvent, (LPVOID*)&m_OriginalProcessEvent) != MH_OK) {
        std::cout << "[Hooks] Failed to hook ProcessEvent!" << std::endl;
    }

    // Qwen Mouse Fix: Hook GetCursorPos to prevent UE5 Engine from reading hardware mouse during AutoWalk
    if (MH_CreateHookApi(L"user32", "GetCursorPos", (LPVOID)HookedGetCursorPos, (LPVOID*)&m_OriginalGetCursorPos) != MH_OK) {
        std::cout << "[Hooks] Failed to hook GetCursorPos!" << std::endl;
    }
    
    if (MH_CreateHookApi(L"user32", "GetAsyncKeyState", (LPVOID)HookedGetAsyncKeyState, (LPVOID*)&m_OriginalGetAsyncKeyState) != MH_OK) {
        std::cout << "[Hooks] Failed to hook GetAsyncKeyState!" << std::endl;
    }

    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (hWs2) {
        void* pSend = GetProcAddress(hWs2, "send");
        if (pSend && MH_CreateHook(pSend, (LPVOID)HookedSend, (LPVOID*)&m_OriginalSend) != MH_OK) {
            std::cout << "[Hooks] Failed to hook send!" << std::endl;
        }
        
        void* pRecv = GetProcAddress(hWs2, "recv");
        if (pRecv && MH_CreateHook(pRecv, (LPVOID)HookedRecv, (LPVOID*)&m_OriginalRecv) != MH_OK) {
            std::cout << "[Hooks] Failed to hook recv!" << std::endl;
        }
    } else {
        std::cout << "[Hooks] ws2_32.dll not loaded yet; skipping network hooks." << std::endl;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        std::cout << "[Hooks] Failed to enable hooks!" << std::endl;
        return false;
    }

    std::cout << "[Hooks] DX12 Hooks Enabled." << std::endl;
    return true;
}

void Hooks::RequestUnhook() {
    g_WantsToUnhook = true;
    if (m_GameWindow && m_OriginalWndProc) {
        // Send a synchronous message to the game thread to force HookedWndProc to run and restore the WndProc
        SendMessage(m_GameWindow, WM_NULL, 0, 0);
    }
}

void Hooks::Unhook() {
    if (m_OriginalWndProc) {
        SetWindowLongPtr(m_GameWindow, GWLP_WNDPROC, (LONG_PTR)m_OriginalWndProc);
        m_OriginalWndProc = nullptr;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (m_IsImGuiInitialized) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_IsImGuiInitialized = false;
    }
    
    if (m_FrameContexts) {
        for (UINT i = 0; i < m_NumFramesInFlight; i++) {
            if (m_FrameContexts[i].CommandAllocator) m_FrameContexts[i].CommandAllocator->Release();
            if (m_FrameContexts[i].MainRenderTargetResource) m_FrameContexts[i].MainRenderTargetResource->Release();
        }
        delete[] m_FrameContexts;
        m_FrameContexts = nullptr;
    }

    if (m_pCommandList) { m_pCommandList->Release(); m_pCommandList = nullptr; }
    if (m_pRtvDescHeap) { m_pRtvDescHeap->Release(); m_pRtvDescHeap = nullptr; }
    if (m_pSrvDescHeap) { m_pSrvDescHeap->Release(); m_pSrvDescHeap = nullptr; }
}
