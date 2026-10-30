#ifndef PSP_ELF_LOADER_H
#define PSP_ELF_LOADER_H

#include "psp_types.h"
#include <string>
#include <vector>
#include <cstdint>

struct ElfModuleInfo {
    std::string moduleName;
    uint32_t entryPoint;
    uint32_t baseAddress;
    uint32_t totalSize;
    bool isPrx;
    bool isValid;
};

class PspElfLoader {
public:
    PspElfLoader();
    ~PspElfLoader();

    bool loadFromMemory(const uint8_t* elfData, size_t dataSize, uint8_t* targetRam, size_t targetRamSize);
    bool loadFromFile(const std::string& filePath, uint8_t* targetRam, size_t targetRamSize);

    const ElfModuleInfo& getModuleInfo() const { return m_moduleInfo; }

private:
    ElfModuleInfo m_moduleInfo;
};

#endif // PSP_ELF_LOADER_H
