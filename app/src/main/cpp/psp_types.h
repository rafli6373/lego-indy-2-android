#ifndef PSP_TYPES_H
#define PSP_TYPES_H

#include <cstdint>
#include <cstddef>

// Standard PSP Screen dimensions
constexpr int PSP_SCREEN_WIDTH = 480;
constexpr int PSP_SCREEN_HEIGHT = 272;
constexpr float PSP_ASPECT_RATIO = static_cast<float>(PSP_SCREEN_WIDTH) / static_cast<float>(PSP_SCREEN_HEIGHT);

// Memory map constants (PSP User Space)
constexpr uint32_t PSP_USER_MEM_BASE = 0x08800000;
constexpr uint32_t PSP_USER_MEM_SIZE = 56 * 1024 * 1024; // 56 MB available on PSP Slim

// Controller bitmask definitions (matching sceCtrl / Allegrex registers)
enum PspCtrlButtons : uint32_t {
    PSP_CTRL_SELECT    = 0x000001,
    PSP_CTRL_START     = 0x000008,
    PSP_CTRL_UP        = 0x000010,
    PSP_CTRL_RIGHT     = 0x000020,
    PSP_CTRL_DOWN      = 0x000040,
    PSP_CTRL_LEFT      = 0x000080,
    PSP_CTRL_LTRIGGER  = 0x000100,
    PSP_CTRL_RTRIGGER  = 0x000200,
    PSP_CTRL_TRIANGLE  = 0x001000,
    PSP_CTRL_CIRCLE    = 0x002000,
    PSP_CTRL_CROSS     = 0x004000,
    PSP_CTRL_SQUARE    = 0x008000,
    PSP_CTRL_HOME      = 0x010000,
    PSP_CTRL_HOLD      = 0x020000,
};

// Data structure passed to sceCtrlReadBufferPositive
struct SceCtrlData {
    uint32_t timeStamp;
    uint32_t buttons;
    uint8_t  lx; // Analog X (0 - 255, center 128)
    uint8_t  ly; // Analog Y (0 - 255, center 128)
    uint8_t  rsrv[6];
};

// Standard PSP IO Access flags
enum SceIoAccessMode {
    PSP_O_RDONLY = 0x0001,
    PSP_O_WRONLY = 0x0002,
    PSP_O_RDWR   = 0x0003,
    PSP_O_NBLOCK = 0x0004,
    PSP_O_APPEND = 0x0100,
    PSP_O_CREAT  = 0x0200,
    PSP_O_TRUNC  = 0x0400,
    PSP_O_NOWAIT = 0x8000,
};

#endif // PSP_TYPES_H
