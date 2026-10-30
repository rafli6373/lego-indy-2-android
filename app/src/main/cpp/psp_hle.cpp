#include "psp_hle.h"
#include <android/log.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
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
    : m_assetManager(nullptr),
      m_touchControls(nullptr),
      m_userMemory(nullptr),
      m_gameDataFound(false),
      m_nextFd(10) {
}

PspHLE::~PspHLE() {
    shutdown();
}

bool PspHLE::init(AAssetManager* assetManager, const std::string& storagePath) {
    m_assetManager = assetManager;
    m_storagePath = storagePath;

    // Allocate 56 MB aligned virtual memory for PSP user space
    m_userMemory = static_cast<uint8_t*>(malloc(PSP_USER_MEM_SIZE));
    if (!m_userMemory) {
        LOGE("Failed to allocate %u bytes of PSP User Memory", PSP_USER_MEM_SIZE);
        m_statusMessage = "Err: Out of Memory for PSP RAM";
        return false;
    }
    std::memset(m_userMemory, 0, PSP_USER_MEM_SIZE);

    LOGI("PSP User Memory mapped at %p (%u MB)", m_userMemory, PSP_USER_MEM_SIZE / (1024 * 1024));

    // Check for LEGO Indiana Jones 2 primary archive: legoindy2psp.fib
    std::string testPath = m_storagePath + "/PSP_GAME/USRDIR/legoindy2psp.fib";
    struct stat st;
    if (stat(testPath.c_str(), &st) == 0 && st.st_size > 0) {
        m_gameDataFound = true;
        m_statusMessage = "Found legoindy2psp.fib (" + std::to_string(st.st_size / (1024 * 1024)) + " MB)";
        LOGI("%s", m_statusMessage.c_str());
    } else {
        // Fallback check in root storage
        testPath = m_storagePath + "/legoindy2psp.fib";
        if (stat(testPath.c_str(), &st) == 0 && st.st_size > 0) {
            m_gameDataFound = true;
            m_statusMessage = "Found legoindy2psp.fib (" + std::to_string(st.st_size / (1024 * 1024)) + " MB)";
            LOGI("%s", m_statusMessage.c_str());
        } else {
            m_gameDataFound = false;
            m_statusMessage = "Waiting for game assets in " + m_storagePath;
            LOGW("%s", m_statusMessage.c_str());
        }
    }

    return true;
}

void PspHLE::shutdown() {
    for (auto& [fd, handle] : m_openFiles) {
        if (handle.isAsset && handle.assetFile) {
            AAsset_close(handle.assetFile);
        } else if (handle.hostFile) {
            fclose(handle.hostFile);
        }
    }
    m_openFiles.clear();

    if (m_userMemory) {
        free(m_userMemory);
        m_userMemory = nullptr;
    }
}

std::string PspHLE::resolvePspPath(const std::string& pspPath) {
    std::string cleaned = pspPath;

    // Strip PSP volume prefixes
    if (cleaned.rfind("disc0:/", 0) == 0) {
        cleaned = cleaned.substr(7);
    } else if (cleaned.rfind("ms0:/", 0) == 0) {
        cleaned = cleaned.substr(5);
    }

    // Try in primary storage path
    std::string fullPath = m_storagePath + "/" + cleaned;
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0) {
        return fullPath;
    }

    // Try under PSP_GAME subfolder
    fullPath = m_storagePath + "/PSP_GAME/" + cleaned;
    if (stat(fullPath.c_str(), &st) == 0) {
        return fullPath;
    }

    return m_storagePath + "/" + cleaned;
}

int PspHLE::sceIoOpen(const char* filename, int flags, int mode) {
    (void)flags;
    (void)mode;
    if (!filename) return -1;

    std::string resolved = resolvePspPath(filename);
    FILE* f = fopen(resolved.c_str(), "rb");
    if (f) {
        int fd = m_nextFd++;
        m_openFiles[fd] = {f, nullptr, false};
        LOGI("sceIoOpen host file: %s -> fd %d", resolved.c_str(), fd);
        return fd;
    }

    // Try Android APK internal assets
    if (m_assetManager) {
        AAsset* asset = AAssetManager_open(m_assetManager, filename, AASSET_MODE_RANDOM);
        if (asset) {
            int fd = m_nextFd++;
            m_openFiles[fd] = {nullptr, asset, true};
            LOGI("sceIoOpen asset file: %s -> fd %d", filename, fd);
            return fd;
        }
    }

    LOGW("sceIoOpen failed for: %s (resolved: %s)", filename, resolved.c_str());
    return -1;
}

int PspHLE::sceIoRead(int fd, void* data, size_t size) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || !data) return -1;

    if (it->second.isAsset) {
        return AAsset_read(it->second.assetFile, data, size);
    } else {
        return static_cast<int>(fread(data, 1, size, it->second.hostFile));
    }
}

int PspHLE::sceIoWrite(int fd, const void* data, size_t size) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end() || it->second.isAsset || !data) return -1;

    return static_cast<int>(fwrite(data, 1, size, it->second.hostFile));
}

long long PspHLE::sceIoLseek(int fd, long long offset, int whence) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end()) return -1;

    if (it->second.isAsset) {
        return AAsset_seek(it->second.assetFile, static_cast<off_t>(offset), whence);
    } else {
        fseek(it->second.hostFile, static_cast<long>(offset), whence);
        return ftell(it->second.hostFile);
    }
}

int PspHLE::sceIoClose(int fd) {
    auto it = m_openFiles.find(fd);
    if (it == m_openFiles.end()) return -1;

    if (it->second.isAsset) {
        AAsset_close(it->second.assetFile);
    } else {
        fclose(it->second.hostFile);
    }

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

uint32_t PspHLE::sceKernelGetSystemTimeLow() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void PspHLE::sceKernelDelayThread(uint32_t usec) {
    usleep(usec);
}
