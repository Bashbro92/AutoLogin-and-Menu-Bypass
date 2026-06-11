#pragma once

#include <windows.h>
#include <string>

// Custom IOCTL Codes (Matches Driver)
#define IOCTL_MOUSE_MOVE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MOUSE_CLICK      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Mouse Input Flags
#define KMDF_MOUSE_MOVE_ABSOLUTE    0x01
#define KMDF_MOUSE_MOVE_RELATIVE    0x02
#define KMDF_MOUSE_CLICK_LEFT_DOWN  0x04
#define KMDF_MOUSE_CLICK_LEFT_UP    0x08
#define KMDF_MOUSE_CLICK_RIGHT_DOWN 0x10
#define KMDF_MOUSE_CLICK_RIGHT_UP   0x20

// Struct to send mouse move
typedef struct _MOUSE_MOVE_REQUEST {
    LONG x;
    LONG y;
    ULONG Flags;
} MOUSE_MOVE_REQUEST, *PMOUSE_MOVE_REQUEST;

// Struct to send mouse click
typedef struct _MOUSE_CLICK_REQUEST {
    ULONG Flags; 
} MOUSE_CLICK_REQUEST, *PMOUSE_CLICK_REQUEST;


class KernelMouseInterface {
public:
    KernelMouseInterface();
    ~KernelMouseInterface();

    bool Initialize();
    bool IsConnected() const;

    // Core Injectors
    bool MoveRelative(LONG dx, LONG dy);
    bool MoveAbsolute(LONG x, LONG y);
    bool LeftClick();
    bool RightClick();

private:
    HANDLE m_hDriver;
    std::wstring m_Symlink;
};
