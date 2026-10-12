#include "FreeRTOS.h"
#include "task.h"

#include "stm32f4xx.h"
#include "system_stm32f4xx.h"

#include <cstdint>

namespace
{
constexpr uint32_t RTC_CLOCK_HZ = 32768U;
constexpr uint32_t RTC_WAKEUP_DIVIDER = 16U;
constexpr uint32_t RTC_WAKEUP_CLOCK_HZ = RTC_CLOCK_HZ / RTC_WAKEUP_DIVIDER;
constexpr uint32_t RTC_SYNCHRONOUS_PRESCALER = 2047U;
constexpr uint32_t RTC_COUNTS_PER_DAY = 24U * 60U * 60U * RTC_WAKEUP_CLOCK_HZ;
constexpr uint32_t RTC_WAKEUP_MAXIMUM_COUNTS = 65536U;
constexpr TickType_t RTC_WAKEUP_MAXIMUM_TICKS =
    static_cast<TickType_t>(
        (static_cast<uint64_t>(RTC_WAKEUP_MAXIMUM_COUNTS) * configTICK_RATE_HZ)
        / RTC_WAKEUP_CLOCK_HZ);

uint32_t fromBcd(const uint32_t tens, const uint32_t units)
{
    return tens * 10U + units;
}

uint32_t readRtcCount()
{
    const uint32_t subSeconds = RTC->SSR;
    const uint32_t time = RTC->TR;
    (void)RTC->DR;

    const uint32_t hours = fromBcd(
        (time & RTC_TR_HT) >> RTC_TR_HT_Pos,
        (time & RTC_TR_HU) >> RTC_TR_HU_Pos);
    const uint32_t minutes = fromBcd(
        (time & RTC_TR_MNT) >> RTC_TR_MNT_Pos,
        (time & RTC_TR_MNU) >> RTC_TR_MNU_Pos);
    const uint32_t seconds = fromBcd(
        (time & RTC_TR_ST) >> RTC_TR_ST_Pos,
        (time & RTC_TR_SU) >> RTC_TR_SU_Pos);

    return ((hours * 60U + minutes) * 60U + seconds)
             * RTC_WAKEUP_CLOCK_HZ
           + RTC_SYNCHRONOUS_PRESCALER
           - subSeconds;
}

uint32_t getElapsedRtcCounts(
    const uint32_t startCount,
    const uint32_t endCount)
{
    if (endCount >= startCount)
    {
        return endCount - startCount;
    }

    return RTC_COUNTS_PER_DAY - startCount + endCount;
}

uint32_t convertTicksToRtcCounts(const TickType_t ticks)
{
    uint64_t counts =
        (static_cast<uint64_t>(ticks) * RTC_WAKEUP_CLOCK_HZ
         + configTICK_RATE_HZ - 1U)
        / configTICK_RATE_HZ;

    if (counts == 0U)
    {
        counts = 1U;
    }
    if (counts > RTC_WAKEUP_MAXIMUM_COUNTS)
    {
        counts = RTC_WAKEUP_MAXIMUM_COUNTS;
    }

    return static_cast<uint32_t>(counts);
}

void unlockRtc()
{
    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
}

void lockRtc()
{
    RTC->WPR = 0xFFU;
}

void startRtcWakeupTimer(const TickType_t ticks)
{
    NVIC_DisableIRQ(RTC_WKUP_IRQn);
    NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);

    CLEAR_BIT(EXTI->IMR, EXTI_IMR_MR22);
    CLEAR_BIT(EXTI->RTSR, EXTI_RTSR_TR22);
    WRITE_REG(EXTI->PR, EXTI_PR_PR22);

    unlockRtc();
    CLEAR_BIT(RTC->CR, RTC_CR_WUTE | RTC_CR_WUTIE);
    while (READ_BIT(RTC->ISR, RTC_ISR_WUTWF) == 0U)
    {
    }

    MODIFY_REG(RTC->CR, RTC_CR_WUCKSEL, 0U);
    RTC->WUTR = convertTicksToRtcCounts(ticks) - 1U;
    CLEAR_BIT(RTC->ISR, RTC_ISR_WUTF);
    SET_BIT(RTC->CR, RTC_CR_WUTIE | RTC_CR_WUTE);
    lockRtc();

    SET_BIT(EXTI->RTSR, EXTI_RTSR_TR22);
    SET_BIT(EXTI->IMR, EXTI_IMR_MR22);
    NVIC_SetPriority(RTC_WKUP_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

void stopRtcWakeupTimer()
{
    NVIC_DisableIRQ(RTC_WKUP_IRQn);

    unlockRtc();
    CLEAR_BIT(RTC->CR, RTC_CR_WUTE | RTC_CR_WUTIE);
    while (READ_BIT(RTC->ISR, RTC_ISR_WUTWF) == 0U)
    {
    }
    CLEAR_BIT(RTC->ISR, RTC_ISR_WUTF);
    lockRtc();

    CLEAR_BIT(EXTI->IMR, EXTI_IMR_MR22);
    WRITE_REG(EXTI->PR, EXTI_PR_PR22);
    NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}
}

extern "C" void vPortSuppressTicksAndSleep(TickType_t expectedIdleTime)
{
    if (expectedIdleTime > RTC_WAKEUP_MAXIMUM_TICKS)
    {
        expectedIdleTime = RTC_WAKEUP_MAXIMUM_TICKS;
    }

    __disable_irq();
    __DSB();
    __ISB();

    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
    {
        __enable_irq();
        return;
    }

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    const uint32_t startCount = readRtcCount();
    startRtcWakeupTimer(expectedIdleTime);

    CLEAR_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);
    TickType_t modifiableIdleTime = expectedIdleTime;
    configPRE_SLEEP_PROCESSING(modifiableIdleTime);
    if (modifiableIdleTime > 0U)
    {
        __DSB();
        __WFI();
        __ISB();
    }
    configPOST_SLEEP_PROCESSING(expectedIdleTime);

    const uint32_t endCount = readRtcCount();
    stopRtcWakeupTimer();

    const uint32_t elapsedRtcCounts = getElapsedRtcCounts(startCount, endCount);
    TickType_t elapsedTicks = static_cast<TickType_t>(
        (static_cast<uint64_t>(elapsedRtcCounts) * configTICK_RATE_HZ)
        / RTC_WAKEUP_CLOCK_HZ);
    if (elapsedTicks > expectedIdleTime)
    {
        elapsedTicks = expectedIdleTime;
    }

    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    SysTick->LOAD = SystemCoreClock / configTICK_RATE_HZ - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;

    vTaskStepTick(elapsedTicks);
    __enable_irq();
}

extern "C" void RTC_WKUP_IRQHandler()
{
    unlockRtc();
    CLEAR_BIT(RTC->ISR, RTC_ISR_WUTF);
    lockRtc();
    WRITE_REG(EXTI->PR, EXTI_PR_PR22);
}
