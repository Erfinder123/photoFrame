
#ifndef SYSTEM_STM32F4XX_H
#define SYSTEM_STM32F4XX_H


#include <stdint.h>


enum
{
    HSE_CLOCK_HZ = 8000000U,
    SYSTEM_CLOCK_HZ = 180000000U
};

extern uint32_t SystemCoreClock;

void SystemInit();
void SystemCoreClockConfig();


#endif //SYSTEM_STM32F4XX_H
