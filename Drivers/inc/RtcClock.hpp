#ifndef RTCCLOCK_HPP
#define RTCCLOCK_HPP


#include <cstdint>


namespace RtcClock
{
struct Time
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

[[nodiscard]] Time readTime();
void configureSecondInterrupt(uint32_t interruptPriority);
void clearSecondInterrupt();
}


#endif // RTCCLOCK_HPP
