
#include <stdint.h>
#include "system_stm32f4xx.h"
#include "cmsis_gcc.h"
#include "stm32f469xx.h"
#include "stm32f4xx.h"


uint32_t SystemCoreClock = 8000000; // Hz

void SystemInit()
{
    SCB->CPACR |= 3UL << 10*2 | 3UL << 11*2;
    __DSB();
    __ISB();

    SystemCoreClockConfig();
}

void SystemCoreClockConfig()
{
    SET_BIT(RCC->CR, RCC_CR_HSEON);
    while(READ_BIT(RCC->CR, RCC_CR_HSERDY) != RCC_CR_HSERDY){}
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_HSE);
    while(READ_BIT(RCC->CFGR, RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE){}
}
