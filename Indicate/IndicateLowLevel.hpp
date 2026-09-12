#ifndef INDICATELOWLEVEL_HPP
#define INDICATELOWLEVEL_HPP


#include <cstdint>


enum class IndicateColor : uint8_t
{
    GREEN,
    ORANGE,
    RED,
    BLUE
};

class IndicateLowLevel
{
    public:
        void configure(IndicateColor color) const;
        void on(IndicateColor color) const;
        void off(IndicateColor color) const;
        [[nodiscard]] uint32_t getTick() const;
};


#endif // INDICATELOWLEVEL_HPP
