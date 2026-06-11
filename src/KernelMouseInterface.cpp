#include "KernelMouseInterface.h"
#include <iostream>

KernelMouseInterface::KernelMouseInterface() {
    m_hDriver = INVALID_HANDLE_VALUE;
    m_Symlink = L"\\\\.\\KernelTrainerMouse";
}

KernelMouseInterface::~KernelMouseInterface() {
    if (m_hDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDriver);
    }
}

bool KernelMouseInterface::Initialize() {
    if (m_hDriver != INVALID_HANDLE_VALUE) return true;

    m_hDriver = CreateFileW(
        m_Symlink.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (m_hDriver == INVALID_HANDLE_VALUE) {
        std::cerr << "[KernelMouse] Failed to open handle to driver! Is it loaded?\n";
        return false;
    }

    std::cout << "[KernelMouse] Successfully connected to Kernel Driver.\n";
    return true;
}

bool KernelMouseInterface::IsConnected() const {
    return (m_hDriver != INVALID_HANDLE_VALUE);
}

bool KernelMouseInterface::MoveRelative(LONG dx, LONG dy) {
    if (!IsConnected() && !Initialize()) return false;

    MOUSE_MOVE_REQUEST req = {0};
    req.x = dx;
    req.y = dy;
    req.Flags = KMDF_MOUSE_MOVE_RELATIVE;

    DWORD bytesReturned = 0;
    return DeviceIoControl(m_hDriver, IOCTL_MOUSE_MOVE, &req, sizeof(req), NULL, 0, &bytesReturned, NULL);
}

bool KernelMouseInterface::MoveAbsolute(LONG x, LONG y) {
    if (!IsConnected() && !Initialize()) return false;

    MOUSE_MOVE_REQUEST req = {0};
    req.x = x;
    req.y = y;
    req.Flags = KMDF_MOUSE_MOVE_ABSOLUTE;

    DWORD bytesReturned = 0;
    return DeviceIoControl(m_hDriver, IOCTL_MOUSE_MOVE, &req, sizeof(req), NULL, 0, &bytesReturned, NULL);
}

bool KernelMouseInterface::LeftClick() {
    if (!IsConnected() && !Initialize()) return false;

    MOUSE_CLICK_REQUEST reqDown = { KMDF_MOUSE_CLICK_LEFT_DOWN };
    MOUSE_CLICK_REQUEST reqUp = { KMDF_MOUSE_CLICK_LEFT_UP };
    DWORD bytesReturned = 0;

    bool down = DeviceIoControl(m_hDriver, IOCTL_MOUSE_CLICK, &reqDown, sizeof(reqDown), NULL, 0, &bytesReturned, NULL);
    bool up = DeviceIoControl(m_hDriver, IOCTL_MOUSE_CLICK, &reqUp, sizeof(reqUp), NULL, 0, &bytesReturned, NULL);
    
    return down && up;
}

bool KernelMouseInterface::RightClick() {
    if (!IsConnected() && !Initialize()) return false;

    MOUSE_CLICK_REQUEST reqDown = { KMDF_MOUSE_CLICK_RIGHT_DOWN };
    MOUSE_CLICK_REQUEST reqUp = { KMDF_MOUSE_CLICK_RIGHT_UP };
    DWORD bytesReturned = 0;

    bool down = DeviceIoControl(m_hDriver, IOCTL_MOUSE_CLICK, &reqDown, sizeof(reqDown), NULL, 0, &bytesReturned, NULL);
    bool up = DeviceIoControl(m_hDriver, IOCTL_MOUSE_CLICK, &reqUp, sizeof(reqUp), NULL, 0, &bytesReturned, NULL);
    
    return down && up;
}
