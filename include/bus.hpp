#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>

#include "memory.hpp"

class Bus {
    public:
        virtual uint32_t read(uint32_t address, MemoryWidth width, bool fetch) = 0;
        virtual void write(uint32_t address, uint32_t data, MemoryWidth width, bool fetch) = 0;
};

#endif