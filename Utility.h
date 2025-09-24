#ifndef UTILITY_H
#define UTILITY_H

#include <cstdint>

inline constexpr unsigned int AlignTo(unsigned int operand, uint16_t alignment){
    return ((operand + (alignment - 1)) & ~(alignment - 1));
};

#endif // UTILITY_H