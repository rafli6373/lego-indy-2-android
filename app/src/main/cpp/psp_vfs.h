#ifndef PSP_VFS_H
#define PSP_VFS_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <android/asset_manager.h>

class IVfsStream {
public:
    virtual ~IVfsStream() = default;
    virtual int64_t read(void* buffer, size_t size) = 0;
    virtual int64_t write(const void* buffer, size_t size) = 0;
    virtual int64_t seek(int64_t offset, int whence) = 0;
    virtual int64_t tell() = 0;
    virtual int64_t size() = 0;
    virtual void close() = 0;
};

class PspVFS {
public:
    static PspVFS& instance();

    bool init(AAssetManager* assetManager, const std::string& primaryStoragePath);
    void shutdown();

    void mount(const std::string& prefix, const std::string& hostPath);
    std::shared_ptr<IVfsStream> open(const std::string& virtualPath, int flags);
    bool exists(const std::string& virtualPath);

    bool isGameArchiveMounted() const { return m_gameArchiveMounted; }
    std::string getArchiveInfo() const { return m_archiveInfo; }
    std::string getMountPath() const { return m_primaryStorage; }

private:
    PspVFS();
    ~PspVFS();

    std::string resolveToHostPath(const std::string& virtualPath);

    AAssetManager* m_assetManager;
    std::string m_primaryStorage;
    std::unordered_map<std::string, std::string> m_mountPoints;

    bool m_gameArchiveMounted;
    std::string m_archiveInfo;
};

#endif // PSP_VFS_H
