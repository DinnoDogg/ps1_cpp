#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <cstdint>
#include <cstddef>
#include <array>

enum class MemoryWidth {
    Byte = 1, Halfword = 2, Word = 4
};

template <std::size_t N>
class Memory {
    protected:
        std::array<uint8_t, N> m_memory;

    public:
        uint32_t read(uint32_t address, MemoryWidth width);
        void write(uint32_t address, uint32_t data, MemoryWidth width);
};

#endif