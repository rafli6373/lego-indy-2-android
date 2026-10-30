#ifndef PSP_TYPES_H
#define PSP_TYPES_H

#include <cstdint>
#include <cstddef>

// Standard PSP Screen Dimensions
constexpr int PSP_SCREEN_WIDTH = 480;
constexpr int PSP_SCREEN_HEIGHT = 272;
constexpr float PSP_ASPECT_RATIO = static_cast<float>(PSP_SCREEN_WIDTH) / static_cast<float>(PSP_SCREEN_HEIGHT);

// Memory Map Constants (PSP User Space)
constexpr uint32_t PSP_USER_MEM_BASE = 0x08800000;
constexpr uint32_t PSP_USER_MEM_SIZE = 56 * 1024 * 1024; // 56 MB on PSP Slim / Modern target
constexpr uint32_t PSP_EDRAM_BASE    = 0x04000000;
constexpr uint32_t PSP_EDRAM_SIZE    = 4 * 1024 * 1024;  // 4 MB Embedded DRAM

// Controller Bitmask Definitions (matching sceCtrl / Allegrex registers)
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

// Data Structure passed to sceCtrlReadBufferPositive
struct SceCtrlData {
    uint32_t timeStamp;
    uint32_t buttons;
    uint8_t  lx; // Analog X (0 - 255, center 128)
    uint8_t  ly; // Analog Y (0 - 255, center 128)
    uint8_t  rsrv[6];
};

// Standard PSP IO Access Flags
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

// Standard 32-bit ELF Header Structure (Allegrex MIPS)
#pragma pack(push, 1)
struct Elf32_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};
#pragma pack(pop)

constexpr uint32_t PT_LOAD = 1;

// PSP Graphics Engine (GE) Primitive Types
enum PspGePrimitiveType {
    GE_PRIM_POINTS         = 0,
    GE_PRIM_LINES          = 1,
    GE_PRIM_LINE_STRIP     = 2,
    GE_PRIM_TRIANGLES      = 3,
    GE_PRIM_TRIANGLE_STRIP = 4,
    GE_PRIM_TRIANGLE_FAN   = 5,
    GE_PRIM_RECTANGLES     = 6,
};

// PSP Display Framebuffer Format
enum PspDisplayPixelFormat {
    PSP_DISPLAY_PIXEL_FORMAT_565 = 0,
    PSP_DISPLAY_PIXEL_FORMAT_5551 = 1,
    PSP_DISPLAY_PIXEL_FORMAT_4444 = 2,
    PSP_DISPLAY_PIXEL_FORMAT_8888 = 3,
};

#endif // PSP_TYPES_H
