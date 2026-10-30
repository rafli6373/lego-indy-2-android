#include "psp_cpu.h"
#include "psp_hle.h"
#include <android/log.h>
#include <cstring>
#include <chrono>

#define LOG_TAG "PspCPU"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

PspCPU& PspCPU::instance() {
    static PspCPU s_instance;
    return s_instance;
}

PspCPU::PspCPU()
    : m_ram(nullptr),
      m_ramSize(0),
      m_ramBase(PSP_USER_MEM_BASE),
      m_pc(0),
      m_nextPc(0),
      m_hi(0),
      m_lo(0),
      m_branchDelay(false),
      m_branchTarget(0),
      m_running(false),
      m_paused(false) {
    std::memset(m_r, 0, sizeof(m_r));
}

PspCPU::~PspCPU() {
    stop();
}

void PspCPU::init(uint8_t* ram, size_t ramSize, uint32_t ramBase) {
    m_ram = ram;
    m_ramSize = ramSize;
    m_ramBase = ramBase;
    LOGI("PspCPU Initialized: RAM %zu MB mapped at 0x%08X", ramSize / (1024 * 1024), ramBase);
}

void PspCPU::reset(uint32_t entryPoint, uint32_t stackPointer) {
    std::memset(m_r, 0, sizeof(m_r));
    m_pc = entryPoint;
    m_nextPc = entryPoint + 4;
    m_r[29] = stackPointer; // $sp
    m_hi = 0;
    m_lo = 0;
    m_branchDelay = false;
    LOGI("PspCPU Reset: PC=0x%08X, SP=0x%08X", m_pc, stackPointer);
}

void PspCPU::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_paused.store(false);
    m_thread = std::thread(&PspCPU::cpuThreadLoop, this);
    LOGI("PspCPU Execution Thread started.");
}

void PspCPU::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    m_paused.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    LOGI("PspCPU Execution Thread stopped.");
}

void PspCPU::pause() {
    m_paused.store(true);
}

void PspCPU::resume() {
    m_paused.store(false);
}

void PspCPU::cpuThreadLoop() {
    constexpr int BATCH_CYCLES = 10000;
    while (m_running.load()) {
        if (m_paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        runCycles(BATCH_CYCLES);
    }
}

uint32_t PspCPU::read32(uint32_t addr) {
    if (addr >= m_ramBase && (addr - m_ramBase + 4) <= m_ramSize) {
        uint32_t offset = addr - m_ramBase;
        uint32_t val;
        std::memcpy(&val, m_ram + offset, 4);
        return val;
    }
    return 0;
}

void PspCPU::write32(uint32_t addr, uint32_t val) {
    if (addr >= m_ramBase && (addr - m_ramBase + 4) <= m_ramSize) {
        uint32_t offset = addr - m_ramBase;
        std::memcpy(m_ram + offset, &val, 4);
    }
}

uint16_t PspCPU::read16(uint32_t addr) {
    if (addr >= m_ramBase && (addr - m_ramBase + 2) <= m_ramSize) {
        uint32_t offset = addr - m_ramBase;
        uint16_t val;
        std::memcpy(&val, m_ram + offset, 2);
        return val;
    }
    return 0;
}

void PspCPU::write16(uint32_t addr, uint16_t val) {
    if (addr >= m_ramBase && (addr - m_ramBase + 2) <= m_ramSize) {
        uint32_t offset = addr - m_ramBase;
        std::memcpy(m_ram + offset, &val, 2);
    }
}

uint8_t PspCPU::read8(uint32_t addr) {
    if (addr >= m_ramBase && (addr - m_ramBase) < m_ramSize) {
        return m_ram[addr - m_ramBase];
    }
    return 0;
}

void PspCPU::write8(uint32_t addr, uint8_t val) {
    if (addr >= m_ramBase && (addr - m_ramBase) < m_ramSize) {
        m_ram[addr - m_ramBase] = val;
    }
}

void PspCPU::runCycles(int cycles) {
    for (int i = 0; i < cycles && m_running.load() && !m_paused.load(); ++i) {
        step();
    }
}

void PspCPU::step() {
    uint32_t inst = read32(m_pc);

    uint32_t currentPc = m_pc;
    m_pc = m_nextPc;
    m_nextPc += 4;

    if (m_branchDelay) {
        m_pc = m_branchTarget;
        m_nextPc = m_branchTarget + 4;
        m_branchDelay = false;
    }

    executeInstruction(inst);
    m_r[0] = 0; // $zero is hardwired to 0
}

void PspCPU::executeInstruction(uint32_t inst) {
    uint32_t op     = (inst >> 26) & 0x3F;
    uint32_t rs     = (inst >> 21) & 0x1F;
    uint32_t rt     = (inst >> 16) & 0x1F;
    uint32_t rd     = (inst >> 11) & 0x1F;
    uint32_t sa     = (inst >> 6)  & 0x1F;
    uint32_t funct  = inst & 0x3F;
    uint16_t imm16  = inst & 0xFFFF;
    int16_t simm16  = static_cast<int16_t>(imm16);
    int32_t simm32  = static_cast<int32_t>(simm16);
    uint32_t uimm32 = static_cast<uint32_t>(imm16);
    uint32_t target = inst & 0x03FFFFFF;

    switch (op) {
        case 0x00: // SPECIAL
            switch (funct) {
                case 0x00: m_r[rd] = m_r[rt] << sa; break; // SLL
                case 0x02: m_r[rd] = m_r[rt] >> sa; break; // SRL
                case 0x03: m_r[rd] = static_cast<int32_t>(m_r[rt]) >> sa; break; // SRA
                case 0x04: m_r[rd] = m_r[rt] << (m_r[rs] & 0x1F); break; // SLLV
                case 0x06: m_r[rd] = m_r[rt] >> (m_r[rs] & 0x1F); break; // SRLV
                case 0x07: m_r[rd] = static_cast<int32_t>(m_r[rt]) >> (m_r[rs] & 0x1F); break; // SRAV
                case 0x08: // JR
                    m_branchDelay = true;
                    m_branchTarget = m_r[rs];
                    break;
                case 0x09: // JALR
                    m_r[rd] = m_nextPc;
                    m_branchDelay = true;
                    m_branchTarget = m_r[rs];
                    break;
                case 0x0C: { // SYSCALL
                    uint32_t code = (inst >> 6) & 0xFFFFF;
                    PspHLE::instance().dispatchSyscall(code);
                    break;
                }
                case 0x10: m_r[rd] = m_hi; break; // MFHI
                case 0x11: m_hi = m_r[rs]; break; // MTHI
                case 0x12: m_r[rd] = m_lo; break; // MFLO
                case 0x13: m_lo = m_r[rs]; break; // MTLO
                case 0x18: { // MULT
                    int64_t res = static_cast<int64_t>(static_cast<int32_t>(m_r[rs])) * static_cast<int32_t>(m_r[rt]);
                    m_lo = static_cast<uint32_t>(res & 0xFFFFFFFF);
                    m_hi = static_cast<uint32_t>((res >> 32) & 0xFFFFFFFF);
                    break;
                }
                case 0x19: { // MULTU
                    uint64_t res = static_cast<uint64_t>(m_r[rs]) * static_cast<uint64_t>(m_r[rt]);
                    m_lo = static_cast<uint32_t>(res & 0xFFFFFFFF);
                    m_hi = static_cast<uint32_t>((res >> 32) & 0xFFFFFFFF);
                    break;
                }
                case 0x1A: // DIV
                    if (m_r[rt] != 0) {
                        m_lo = static_cast<int32_t>(m_r[rs]) / static_cast<int32_t>(m_r[rt]);
                        m_hi = static_cast<int32_t>(m_r[rs]) % static_cast<int32_t>(m_r[rt]);
                    }
                    break;
                case 0x1B: // DIVU
                    if (m_r[rt] != 0) {
                        m_lo = m_r[rs] / m_r[rt];
                        m_hi = m_r[rs] % m_r[rt];
                    }
                    break;
                case 0x20: // ADD
                case 0x21: m_r[rd] = m_r[rs] + m_r[rt]; break; // ADDU
                case 0x22: // SUB
                case 0x23: m_r[rd] = m_r[rs] - m_r[rt]; break; // SUBU
                case 0x24: m_r[rd] = m_r[rs] & m_r[rt]; break; // AND
                case 0x25: m_r[rd] = m_r[rs] | m_r[rt]; break; // OR
                case 0x26: m_r[rd] = m_r[rs] ^ m_r[rt]; break; // XOR
                case 0x27: m_r[rd] = ~(m_r[rs] | m_r[rt]); break; // NOR
                case 0x2A: m_r[rd] = (static_cast<int32_t>(m_r[rs]) < static_cast<int32_t>(m_r[rt])) ? 1 : 0; break; // SLT
                case 0x2B: m_r[rd] = (m_r[rs] < m_r[rt]) ? 1 : 0; break; // SLTU
                default: break;
            }
            break;

        case 0x02: // J
            m_branchDelay = true;
            m_branchTarget = (m_pc & 0xF0000000) | (target << 2);
            break;
        case 0x03: // JAL
            m_r[31] = m_nextPc;
            m_branchDelay = true;
            m_branchTarget = (m_pc & 0xF0000000) | (target << 2);
            break;
        case 0x04: // BEQ
            if (m_r[rs] == m_r[rt]) {
                m_branchDelay = true;
                m_branchTarget = m_pc + (simm32 << 2);
            }
            break;
        case 0x05: // BNE
            if (m_r[rs] != m_r[rt]) {
                m_branchDelay = true;
                m_branchTarget = m_pc + (simm32 << 2);
            }
            break;
        case 0x08: // ADDI
        case 0x09: m_r[rt] = m_r[rs] + simm32; break; // ADDIU
        case 0x0A: m_r[rt] = (static_cast<int32_t>(m_r[rs]) < simm32) ? 1 : 0; break; // SLTI
        case 0x0B: m_r[rt] = (m_r[rs] < static_cast<uint32_t>(simm32)) ? 1 : 0; break; // SLTIU
        case 0x0C: m_r[rt] = m_r[rs] & uimm32; break; // ANDI
        case 0x0D: m_r[rt] = m_r[rs] | uimm32; break; // ORI
        case 0x0E: m_r[rt] = m_r[rs] ^ uimm32; break; // XORI
        case 0x0F: m_r[rt] = uimm32 << 16; break; // LUI

        case 0x20: m_r[rt] = static_cast<int32_t>(static_cast<int8_t>(read8(m_r[rs] + simm32))); break; // LB
        case 0x21: m_r[rt] = static_cast<int32_t>(static_cast<int16_t>(read16(m_r[rs] + simm32))); break; // LH
        case 0x23: m_r[rt] = read32(m_r[rs] + simm32); break; // LW
        case 0x24: m_r[rt] = read8(m_r[rs] + simm32); break; // LBU
        case 0x25: m_r[rt] = read16(m_r[rs] + simm32); break; // LHU

        case 0x28: write8(m_r[rs] + simm32, static_cast<uint8_t>(m_r[rt] & 0xFF)); break; // SB
        case 0x29: write16(m_r[rs] + simm32, static_cast<uint16_t>(m_r[rt] & 0xFFFF)); break; // SH
        case 0x2B: write32(m_r[rs] + simm32, m_r[rt]); break; // SW

        default:
            break;
    }
}
