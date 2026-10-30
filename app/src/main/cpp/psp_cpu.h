#ifndef PSP_CPU_H
#define PSP_CPU_H

#include "psp_types.h"
#include <cstdint>
#include <atomic>
#include <thread>

class PspCPU {
public:
    static PspCPU& instance();

    void init(uint8_t* ram, size_t ramSize, uint32_t ramBase);
    void reset(uint32_t entryPoint, uint32_t stackPointer);
    void start();
    void stop();
    void pause();
    void resume();

    void step();
    void runCycles(int cycles);

    // Register access
    uint32_t getPC() const { return m_pc; }
    void setPC(uint32_t pc) { m_pc = pc; }
    uint32_t getReg(int index) const { return (index == 0) ? 0 : m_r[index]; }
    void setReg(int index, uint32_t val) { if (index != 0) m_r[index] = val; }

    bool isRunning() const { return m_running.load(); }

    // Memory Access
    uint32_t read32(uint32_t addr);
    void write32(uint32_t addr, uint32_t val);
    uint16_t read16(uint32_t addr);
    void write16(uint32_t addr, uint16_t val);
    uint8_t read8(uint32_t addr);
    void write8(uint32_t addr, uint8_t val);

private:
    PspCPU();
    ~PspCPU();

    void executeInstruction(uint32_t inst);
    void cpuThreadLoop();

    uint8_t* m_ram;
    size_t m_ramSize;
    uint32_t m_ramBase;

    uint32_t m_r[32];
    uint32_t m_pc;
    uint32_t m_nextPc;
    uint32_t m_hi;
    uint32_t m_lo;

    bool m_branchDelay;
    uint32_t m_branchTarget;

    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::thread m_thread;
};

#endif // PSP_CPU_H
