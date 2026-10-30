#include "psp_hle.h"
#include <android/log.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#define LOG_TAG "PspHLE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

PspHLE& PspHLE::instance() {
    static PspHLE s_instance;
    return s_instance;
}

PspHLE::PspHLE()
    : m_touchControls(nullptr),
      m_userMemory(nullptr),
      m_userMemorySize(PSP_USER_MEM_SIZE),
      m_nextFd(10),
      m_gameReady(false) {
}

PspHLE::~PspHLE() {
    shutdown();
}

bool PspHLE::init(AAssetManager* assetManager, const std::string& storagePath) {
    // 1. Initialize Virtual File System
    if (!PspVFS::instance().init(assetManager, storagePath)) {
        LOGE("Failed to initialize VFS");
        return false;
    }

    // 2. Initialize Graphics Engine subsystem
    PspGE::instance().init();

    // 3. Allocate 56 MB aligned virtual memory for PSP User RAM
    m_userMemory = static_cast<uint8_t*>(malloc(m_userMemorySize));
    if (!m_userMemory) {
        LOGE("Failed to allocate %zu bytes of PSP RAM", m_userMemorySize);
        m_statusMessage = "Err: Out of Memory for PSP RAM";
        return false;
    }
    std::memset(m_userMemory, 0, m_userMemorySize);
    LOGI("PSP RAM mapped at %p (%zu MB)", m_userMemory, m_userMemorySize / (1024 * 1024));

    // 4. Check for LEGO Indiana Jones 2 executable and data
    std::string bootPath = storagePath + "/PSP_GAME/SYSDIR/BOOT.BIN";
    if (m_elfLoader.loadFromFile(bootPath, m_userMemory, m_userMemorySize)) {
        m_gameReady = true;
        m_statusMessage = "BOOT.BIN loaded! Entry: 0x" + std::to_string(m_elfLoader.getModuleInfo().entryPoint);
        LOGI("%s", m_statusMessage.c_str());
    } else {
        if (PspVFS::instance().isGameArchiveMounted()) {
            m_gameReady = true;
            m_statusMessage = PspVFS::instance().getArchiveInfo();
        } else {
            m_gameReady = false;
            m_statusMessage = "Ready: Place legoindy2psp.fib in " + storagePath;
        }
    }

    return true;
}

void PspHLE::shutdown() {
    for (auto& [fd, stream] : m_openFiles) {
        if (stream) stream->close();
    }
    m_openFiles.clear();

    PspGE::instance().shutdown();
    PspVFS::instance().shutdown();

    if (m_userMemory) {
        free(m_userMemory);
        m_userMemory = nullptr;
    }
}

int PspHLE::sceIoOpen(const char* filename, int flags, int mode) {
    (void)mode;
    if (!filename) return -1;

    auto stream = PspVFS::instance().open(filename, flags);
    if (!stream) {
        LOGW("sceIoOpen failed for: %s", filename);
        return -1;
    }

    int fd = m_nextFd++;
    m_openFiles[fd] = stream;
    LOGI("sceIoOpen success: %s -> fd %d", filename, fd);
    return fd;
}

int PspHLE::sceIoRead(int fd, void* data, size_t size) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || !it->second || !data) return -1;
    return static_cast<int>(it->second->read(data, size));
}

int PspHLE::sceIoWrite(int fd, const void* data, size_t size) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || !it->second || !data) return -1;
    return static_cast<int>(it->second->write(data, size));
}

long long PspHLE::sceIoLseek(int fd, long long offset, int whence) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || !it->second) return -1;
    return it->second->seek(offset, whence);
}

int PspHLE::sceIoClose(int fd) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || !it->second) return -1;
    it->second->close();
    m_openFiles.erase(it);
    return 0;
}

int PspHLE::sceCtrlReadBufferPositive(SceCtrlData* pad_data, int count) {
    if (!pad_data || count <= 0) return 0;

    if (m_touchControls) {
        for (int i = 0; i < count; ++i) {
            m_touchControls->getCtrlData(&pad_data[i]);
            pad_data[i].timeStamp = sceKernelGetSystemTimeLow();
        }
    } else {
        for (int i = 0; i < count; ++i) {
            pad_data[i].timeStamp = sceKernelGetSystemTimeLow();
            pad_data[i].buttons = 0;
            pad_data[i].lx = 128;
            pad_data[i].ly = 128;
        }
    }
    return count;
}

int PspHLE::sceCtrlPeekBufferPositive(SceCtrlData* pad_data, int count) {
    return sceCtrlReadBufferPositive(pad_data, count);
}

int PspHLE::sceDisplayWaitVblankStart() {
    PspGE::instance().waitVblank();
    return 0;
}

int PspHLE::sceDisplaySetMode(int mode, int width, int height) {
    (void)mode;
    PspGE::instance().setViewport(0, 0, static_cast<float>(width), static_cast<float>(height));
    return 0;
}

int PspHLE::sceDisplaySetFrameBuf(uint32_t topaddr, int bufferwidth, int pixelformat, int sync) {
    (void)sync;
    PspGE::instance().setFrameBuffer(topaddr, bufferwidth, pixelformat);
    return 0;
}

uint32_t PspHLE::sceKernelGetSystemTimeLow() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void PspHLE::sceKernelDelayThread(uint32_t usec) {
    usleep(usec);
}
