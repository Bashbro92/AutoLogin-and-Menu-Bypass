#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <d3d12.h>
#include <dxgi1_4.h>

struct FrameContext {
    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Resource* MainRenderTargetResource;
    D3D12_CPU_DESCRIPTOR_HANDLE MainRenderTargetDescriptor;
};

class Hooks {
public:
    static bool Initialize();
    static void RequestUnhook();
    static void Unhook();
    
    static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // Hook functions
    typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
    typedef void(WINAPI* ExecuteCommandLists_t)(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
    typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    typedef void(*ProcessEvent_t)(const void* Obj, void* Function, void* Parms);

    static Present_t m_OriginalPresent;
    static ExecuteCommandLists_t m_OriginalExecuteCommandLists;
    static ResizeBuffers_t m_OriginalResizeBuffers;
    static ProcessEvent_t m_OriginalProcessEvent;

    static HRESULT WINAPI HookedPresent(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
    static void WINAPI HookedExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
    static HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    static void HookedProcessEvent(const void* Obj, void* Function, void* Parms);
    static BOOL WINAPI HookedGetCursorPos(LPPOINT lpPoint);
    static SHORT WINAPI HookedGetAsyncKeyState(int vKey);
    static int WSAAPI HookedSend(SOCKET s, const char* buf, int len, int flags);
    static int WSAAPI HookedRecv(SOCKET s, char* buf, int len, int flags);

    // UI state
    static HWND m_GameWindow;
    static WNDPROC m_OriginalWndProc;
    static bool m_IsImGuiInitialized;

    // DX12 State
    static ID3D12Device* m_pDevice;
    static ID3D12CommandQueue* m_pCommandQueue;
    static ID3D12DescriptorHeap* m_pRtvDescHeap;
    static ID3D12DescriptorHeap* m_pSrvDescHeap;
    static ID3D12GraphicsCommandList* m_pCommandList;
    static FrameContext* m_FrameContexts;
    static UINT m_NumFramesInFlight;

};
