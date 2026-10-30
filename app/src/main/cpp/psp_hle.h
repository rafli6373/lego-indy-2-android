#ifndef PSP_HLE_H
#define PSP_HLE_H

#include "psp_types.h"
#include "touch_controls.h"
#include "psp_vfs.h"
#include "psp_elf_loader.h"
#include "psp_ge.h"
#include <android/asset_manager.h>
#include <string>
#include <unordered_map>
#include <memory>

class PspHLE {
public:
    static PspHLE& instance();

    bool init(AAssetManager* assetManager, const std::string& storagePath);
    void shutdown();

    void setTouchControls(TouchControls* controls) { m_touchControls = controls; }

    // --- File I/O High-Level Emulation (sceIo) ---
    int sceIoOpen(const char* filename, int flags, int mode);
    int sceIoRead(int fd, void* data, size_t size);
    int sceIoWrite(int fd, const void* data, size_t size);
    long long sceIoLseek(int fd, long long offset, int whence);
    int sceIoClose(int fd);

    // --- Controller / Input (sceCtrl) ---
    int sceCtrlReadBufferPositive(SceCtrlData* pad_data, int count);
    int sceCtrlPeekBufferPositive(SceCtrlData* pad_data, int count);

    // --- Display & Graphics (sceDisplay, sceGe) ---
    int sceDisplayWaitVblankStart();
    int sceDisplaySetMode(int mode, int width, int height);
    int sceDisplaySetFrameBuf(uint32_t topaddr, int bufferwidth, int pixelformat, int sync);

    // --- Memory & Kernel (sceKernel) ---
    uint8_t* getBaseUserMemory() const { return m_userMemory; }
    uint32_t sceKernelGetSystemTimeLow();
    void sceKernelDelayThread(uint32_t usec);

    // System Status
    bool isGameReady() const { return m_gameReady; }
    std::string getStatusMessage() const { return m_statusMessage; }

private:
    PspHLE();
    ~PspHLE();

    TouchControls* m_touchControls;
    uint8_t* m_userMemory;
    size_t m_userMemorySize;

    PspElfLoader m_elfLoader;
    std::unordered_map<int, std::shared_ptr<IVfsStream>> m_openFiles;
    int m_nextFd;

    bool m_gameReady;
    std::string m_statusMessage;
};

#endif // PSP_HLE_H
