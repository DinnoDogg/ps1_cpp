#ifndef CPU_HPP
#define CPU_HPP

#include <cstdint>

#include "r3000.hpp"
#include "memory.hpp"

class Cpu : public R3000 {
    private:
        //gte, dma, timers
        Memory<1024> m_scratchpad;
        
        uint32_t read(uint32_t address, MemoryWidth width) override;
        void write(uint32_t address, uint32_t data, MemoryWidth width) override;

    public:
        Cpu(Bus& bus);
};

#endif