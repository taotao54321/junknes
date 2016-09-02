#pragma once

#include <cstdio>
#include <cstdint>
#include <cstdarg>


template<typename T, unsigned int Offset, unsigned int Bits=1>
class BitField{
private:
    static_assert(Offset < 8*sizeof(T), "");
    static_assert(0 < Bits && Bits <= 8*sizeof(T), "");
    static_assert(Offset + Bits <= 8*sizeof(T), "");

    static constexpr T MASK = ((T(1)<<Bits)-1) << Offset;

    T value_;

public:
    operator T() const { return (value_ & MASK) >> Offset; }

    BitField& operator=(const BitField& rhs)
    {
        return *this = T(rhs);
    }

    BitField& operator=(T rhs)
    {
        value_ = (value_ & ~MASK) | ((rhs & (MASK>>Offset)) << Offset);
        return *this;
    }

    BitField& operator+=(T rhs)
    {
        return *this = T(*this) + rhs;
    }

    BitField& operator^=(T rhs)
    {
        return *this = T(*this) ^ rhs;
    }

    BitField& operator++() { return *this += 1; }
};

template<unsigned int Offset, unsigned int Bits=1>
using BitField8 = BitField<std::uint8_t, Offset, Bits>;

template<unsigned int Offset, unsigned int Bits=1>
using BitField16 = BitField<std::uint16_t, Offset, Bits>;


inline void INFO(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
}
