#include "psp_elf_loader.h"
#include <android/log.h>
#include <cstdio>
#include <cstring>

#define LOG_TAG "PspElfLoader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

constexpr uint8_t ELF_MAGIC[4] = {0x7F, 'E', 'L', 'F'};
constexpr uint8_t PSP_MAGIC[4] = {'~', 'P', 'S', 'P'};

PspElfLoader::PspElfLoader() {
    m_moduleInfo = {"", 0, 0, 0, false, false};
}

PspElfLoader::~PspElfLoader() = default;

bool PspElfLoader::loadFromMemory(const uint8_t* data, size_t dataSize, uint8_t* targetRam, size_t targetRamSize) {
    if (!data || dataSize < sizeof(Elf32_Ehdr)) {
        LOGE("Invalid ELF data or buffer too small");
        return false;
    }

    // Check for encrypted/packed PSP header
    if (std::memcmp(data, PSP_MAGIC, 4) == 0) {
        LOGW("Found ~PSP encrypted PRX header. Unpacking/decryption required before loading.");
        m_moduleInfo.isPrx = true;
        m_moduleInfo.isValid = false;
        return false;
    }

    // Validate standard ELF magic
    if (std::memcmp(data, ELF_MAGIC, 4) != 0) {
        LOGE("Invalid ELF magic header");
        return false;
    }

    const auto* ehdr = reinterpret_cast<const Elf32_Ehdr*>(data);

    // Allegrex MIPS architecture check (EM_MIPS = 8)
    if (ehdr->e_machine != 8) {
        LOGW("Non-MIPS architecture in ELF header (machine: %u)", ehdr->e_machine);
    }

    m_moduleInfo.entryPoint = ehdr->e_entry;
    m_moduleInfo.baseAddress = 0xFFFFFFFF;
    m_moduleInfo.totalSize = 0;
    m_moduleInfo.isPrx = (ehdr->e_type == 0xFFA0); // PSP Executable PRX type

    LOGI("ELF Header parsed: Entry=0x%08X, PH count=%u, Type=0x%04X",
         ehdr->e_entry, ehdr->e_phnum, ehdr->e_type);

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf32_Phdr) > dataSize) {
        LOGE("Program header table extends beyond file size");
        return false;
    }

    const auto* phdrArray = reinterpret_cast<const Elf32_Phdr*>(data + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const auto& ph = phdrArray[i];

        if (ph.p_type == PT_LOAD) {
            uint32_t vaddr = ph.p_vaddr;
            if (vaddr < m_moduleInfo.baseAddress) {
                m_moduleInfo.baseAddress = vaddr;
            }

            // Calculate RAM offset relative to PSP User Base
            uint32_t ramOffset = (vaddr >= PSP_USER_MEM_BASE) ? (vaddr - PSP_USER_MEM_BASE) : vaddr;

            if (ramOffset + ph.p_memsz <= targetRamSize) {
                // Copy segment file data
                if (ph.p_filesz > 0 && ph.p_offset + ph.p_filesz <= dataSize) {
                    std::memcpy(targetRam + ramOffset, data + ph.p_offset, ph.p_filesz);
                }

                // Zero-fill BSS
                if (ph.p_memsz > ph.p_filesz) {
                    std::memset(targetRam + ramOffset + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
                }

                m_moduleInfo.totalSize += ph.p_memsz;
                LOGI("Loaded PT_LOAD segment [%u]: VAddr=0x%08X, MemSz=0x%08X (Offset: 0x%08X)",
                     i, vaddr, ph.p_memsz, ramOffset);
            } else {
                LOGE("Segment [%u] exceeds allocated PSP RAM size!", i);
                return false;
            }
        }
    }

    m_moduleInfo.isValid = true;
    LOGI("ELF Module successfully mapped into PSP RAM. Entry point: 0x%08X, Total Size: %u KB",
         m_moduleInfo.entryPoint, m_moduleInfo.totalSize / 1024);
    return true;
}

bool PspElfLoader::loadFromFile(const std::string& filePath, uint8_t* targetRam, size_t targetRamSize) {
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        LOGW("Cannot open ELF file: %s", filePath.c_str());
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(fp);
        return false;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(sz));
    size_t readBytes = fread(buffer.data(), 1, buffer.size(), fp);
    fclose(fp);

    if (readBytes != buffer.size()) {
        LOGE("Failed to read entire ELF file: %s", filePath.c_str());
        return false;
    }

    return loadFromMemory(buffer.data(), buffer.size(), targetRam, targetRamSize);
}
