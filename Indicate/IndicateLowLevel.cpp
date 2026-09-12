#include "IndicateLowLevel.hpp"

#include "FreeRTOS.h"
#include "stm32f469xx.h"
#include "stm32f4xx.h"
#include "task.h"


namespace
{
    struct LedHardware
    {
        GPIO_TypeDef* gpio;
        uint32_t gpioClock;
        uint32_t pinNumber;
    };

    LedHardware getHardware(const IndicateColor color)
    {
        switch (color)
        {
            case IndicateColor::GREEN:
                return {GPIOG, RCC_AHB1ENR_GPIOGEN, 6U};
            case IndicateColor::ORANGE:
                return {GPIOD, RCC_AHB1ENR_GPIODEN, 4U};
            case IndicateColor::RED:
                return {GPIOD, RCC_AHB1ENR_GPIODEN, 5U};
            case IndicateColor::BLUE:
                return {GPIOK, RCC_AHB1ENR_GPIOKEN, 3U};
        }

        return {GPIOG, RCC_AHB1ENR_GPIOGEN, 6U};
    }
}


void IndicateLowLevel::configure(const IndicateColor color) const
{
    const LedHardware hardware = getHardware(color);
    const uint32_t pinMask = 1UL << hardware.pinNumber;
    const uint32_t modePosition = hardware.pinNumber * 2U;
    const uint32_t modeMask = 0x3UL << modePosition;

    SET_BIT(RCC->AHB1ENR, hardware.gpioClock);
    static_cast<void>(RCC->AHB1ENR);

    MODIFY_REG(hardware.gpio->MODER, modeMask, 0x1UL << modePosition);
    CLEAR_BIT(hardware.gpio->OTYPER, pinMask);
    CLEAR_BIT(hardware.gpio->OSPEEDR, modeMask);
    CLEAR_BIT(hardware.gpio->PUPDR, modeMask);
}

void IndicateLowLevel::on(const IndicateColor color) const
{
    const LedHardware hardware = getHardware(color);
    hardware.gpio->BSRR = (1UL << hardware.pinNumber) << 16U;
}

void IndicateLowLevel::off(const IndicateColor color) const
{
    const LedHardware hardware = getHardware(color);
    hardware.gpio->BSRR = 1UL << hardware.pinNumber;
}

uint32_t IndicateLowLevel::getTick() const
{
    return xTaskGetTickCount();
}
