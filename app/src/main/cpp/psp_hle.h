#ifndef PSP_HLE_H
#define PSP_HLE_H

#include "psp_types.h"
#include "touch_controls.h"
#include <android/asset_manager.h>
#include <string>
#include <unordered_map>
#include <vector>

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

    // --- Memory / Kernel (sceKernel) ---
    void* getBaseUserMemory() const { return m_userMemory; }
    uint32_t sceKernelGetSystemTimeLow();
    void sceKernelDelayThread(uint32_t usec);

    // Status / Asset info
    bool isGameDataFound() const { return m_gameDataFound; }
    std::string getStatusMessage() const { return m_statusMessage; }

private:
    PspHLE();
    ~PspHLE();

    std::string resolvePspPath(const std::string& pspPath);

    AAssetManager* m_assetManager;
    std::string m_storagePath;
    TouchControls* m_touchControls;

    uint8_t* m_userMemory;
    bool m_gameDataFound;
    std::string m_statusMessage;

    struct OpenFileHandle {
        FILE* hostFile;
        AAsset* assetFile;
        bool isAsset;
    };

    std::unordered_map<int, OpenFileHandle> m_openFiles;
    int m_nextFd;
};

#endif // PSP_HLE_H
