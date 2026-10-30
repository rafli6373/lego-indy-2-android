#include "psp_vfs.h"
#include <android/log.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

#define LOG_TAG "PspVFS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class HostFileStream : public IVfsStream {
public:
    explicit HostFileStream(FILE* fp) : m_fp(fp) {}
    ~HostFileStream() override { close(); }

    int64_t read(void* buffer, size_t size) override {
        if (!m_fp) return -1;
        return static_cast<int64_t>(fread(buffer, 1, size, m_fp));
    }

    int64_t write(const void* buffer, size_t size) override {
        if (!m_fp) return -1;
        return static_cast<int64_t>(fwrite(buffer, 1, size, m_fp));
    }

    int64_t seek(int64_t offset, int whence) override {
        if (!m_fp) return -1;
        fseek(m_fp, static_cast<long>(offset), whence);
        return ftell(m_fp);
    }

    int64_t tell() override {
        if (!m_fp) return -1;
        return ftell(m_fp);
    }

    int64_t size() override {
        if (!m_fp) return -1;
        long cur = ftell(m_fp);
        fseek(m_fp, 0, SEEK_END);
        long sz = ftell(m_fp);
        fseek(m_fp, cur, SEEK_SET);
        return sz;
    }

    void close() override {
        if (m_fp) {
            fclose(m_fp);
            m_fp = nullptr;
        }
    }

private:
    FILE* m_fp;
};

class AssetFileStream : public IVfsStream {
public:
    explicit AssetFileStream(AAsset* asset) : m_asset(asset) {}
    ~AssetFileStream() override { close(); }

    int64_t read(void* buffer, size_t size) override {
        if (!m_asset) return -1;
        return AAsset_read(m_asset, buffer, size);
    }

    int64_t write(const void*, size_t) override {
        return -1; // Read-only
    }

    int64_t seek(int64_t offset, int whence) override {
        if (!m_asset) return -1;
        return AAsset_seek(m_asset, static_cast<off_t>(offset), whence);
    }

    int64_t tell() override {
        if (!m_asset) return -1;
        return AAsset_getLength(m_asset) - AAsset_getRemainingLength(m_asset);
    }

    int64_t size() override {
        if (!m_asset) return -1;
        return AAsset_getLength(m_asset);
    }

    void close() override {
        if (m_asset) {
            AAsset_close(m_asset);
            m_asset = nullptr;
        }
    }

private:
    AAsset* m_asset;
};

PspVFS& PspVFS::instance() {
    static PspVFS s_instance;
    return s_instance;
}

PspVFS::PspVFS()
    : m_assetManager(nullptr),
      m_gameArchiveMounted(false) {
}

PspVFS::~PspVFS() {
    shutdown();
}

bool PspVFS::init(AAssetManager* assetManager, const std::string& primaryStoragePath) {
    m_assetManager = assetManager;
    m_primaryStorage = primaryStoragePath;

    mount("disc0:/", primaryStoragePath);
    mount("ms0:/", primaryStoragePath + "/savedata");

    LOGI("PspVFS initialized with primary path: %s", primaryStoragePath.c_str());

    // Probe for LEGO Indiana Jones 2 main game archive across standard storage locations
    struct Candidate {
        std::string filePath;
        std::string rootMount;
    };

    const std::vector<Candidate> candidates = {
        { primaryStoragePath + "/PSP_GAME/USRDIR/legoindy2psp.fib", primaryStoragePath },
        { "/sdcard/LEGO_Indy_2/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard/LEGO_Indy_2" },
        { "/sdcard/LEGO_Indy_2/USRDIR/legoindy2psp.fib", "/sdcard/LEGO_Indy_2" },
        { "/sdcard/LEGO Indiana Jones 2 The Adventure Continues/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard/LEGO Indiana Jones 2 The Adventure Continues" },
        { "/sdcard/Download/LEGO_Indy_2/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard/Download/LEGO_Indy_2" },
        { "/sdcard/Download/LEGO Indiana Jones 2 The Adventure Continues/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard/Download/LEGO Indiana Jones 2 The Adventure Continues" },
        { "/sdcard/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard" },
        { "/sdcard/Android/data/com.ttgames.legoindy2/files/PSP_GAME/USRDIR/legoindy2psp.fib", "/sdcard/Android/data/com.ttgames.legoindy2/files" }
    };

    struct stat st{};
    for (const auto& c : candidates) {
        if (stat(c.filePath.c_str(), &st) == 0 && st.st_size > 0) {
            m_gameArchiveMounted = true;
            m_primaryStorage = c.rootMount;
            mount("disc0:/", c.rootMount);
            m_archiveInfo = "Mounted: legoindy2psp.fib (" + std::to_string(st.st_size / (1024 * 1024)) + " MB)";
            LOGI("%s from %s", m_archiveInfo.c_str(), c.filePath.c_str());
            return true;
        }
    }

    m_gameArchiveMounted = false;
    m_archiveInfo = "Awaiting folder LEGO_Indy_2 in phone storage";
    LOGW("legoindy2psp.fib not found in candidates");
    return true;
}

void PspVFS::shutdown() {
    m_mountPoints.clear();
}

void PspVFS::mount(const std::string& prefix, const std::string& hostPath) {
    m_mountPoints[prefix] = hostPath;
}

std::string PspVFS::resolveToHostPath(const std::string& virtualPath) {
    for (const auto& [prefix, hostBase] : m_mountPoints) {
        if (virtualPath.rfind(prefix, 0) == 0) {
            std::string sub = virtualPath.substr(prefix.length());
            return hostBase + "/" + sub;
        }
    }
    return m_primaryStorage + "/" + virtualPath;
}

std::shared_ptr<IVfsStream> PspVFS::open(const std::string& virtualPath, int flags) {
    std::string hostPath = resolveToHostPath(virtualPath);

    const char* mode = (flags & 0x0002) ? "wb+" : "rb";
    FILE* fp = fopen(hostPath.c_str(), mode);
    if (fp) {
        return std::make_shared<HostFileStream>(fp);
    }

    // Try secondary under PSP_GAME folder
    std::string altPath = m_primaryStorage + "/PSP_GAME/" + virtualPath;
    fp = fopen(altPath.c_str(), mode);
    if (fp) {
        return std::make_shared<HostFileStream>(fp);
    }

    // Fallback to internal Android APK assets
    if (m_assetManager) {
        AAsset* asset = AAssetManager_open(m_assetManager, virtualPath.c_str(), AASSET_MODE_RANDOM);
        if (asset) {
            return std::make_shared<AssetFileStream>(asset);
        }
    }

    return nullptr;
}

bool PspVFS::exists(const std::string& virtualPath) {
    std::string host = resolveToHostPath(virtualPath);
    struct stat st{};
    return stat(host.c_str(), &st) == 0;
}
