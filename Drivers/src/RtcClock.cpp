#include "RtcClock.hpp"

#include "stm32f4xx.h"


RtcClock::Time RtcClock::readTime()
{
    const uint32_t time = RTC->TR;
    static_cast<void>(RTC->DR);

    return
    {
        static_cast<uint8_t>(
            ((time & RTC_TR_HT) >> RTC_TR_HT_Pos) * 10U
            + ((time & RTC_TR_HU) >> RTC_TR_HU_Pos)),
        static_cast<uint8_t>(
            ((time & RTC_TR_MNT) >> RTC_TR_MNT_Pos) * 10U
            + ((time & RTC_TR_MNU) >> RTC_TR_MNU_Pos)),
        static_cast<uint8_t>(
            ((time & RTC_TR_ST) >> RTC_TR_ST_Pos) * 10U
            + ((time & RTC_TR_SU) >> RTC_TR_SU_Pos))
    };
}

void RtcClock::configureSecondInterrupt(const uint32_t interruptPriority)
{
    NVIC_DisableIRQ(RTC_Alarm_IRQn);
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    CLEAR_BIT(EXTI->IMR, EXTI_IMR_MR17);
    CLEAR_BIT(EXTI->RTSR, EXTI_RTSR_TR17);
    WRITE_REG(EXTI->PR, EXTI_PR_PR17);

    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
    CLEAR_BIT(RTC->CR, RTC_CR_ALRAE | RTC_CR_ALRAIE);
    while (READ_BIT(RTC->ISR, RTC_ISR_ALRAWF) == 0U)
    {
    }
    RTC->ALRMAR = RTC_ALRMAR_MSK4
                 | RTC_ALRMAR_MSK3
                 | RTC_ALRMAR_MSK2
                 | RTC_ALRMAR_MSK1;
    RTC->ALRMASSR = 0U;
    CLEAR_BIT(RTC->ISR, RTC_ISR_ALRAF);
    SET_BIT(RTC->CR, RTC_CR_ALRAE | RTC_CR_ALRAIE);
    RTC->WPR = 0xFFU;

    SET_BIT(EXTI->RTSR, EXTI_RTSR_TR17);
    SET_BIT(EXTI->IMR, EXTI_IMR_MR17);
    NVIC_SetPriority(RTC_Alarm_IRQn, interruptPriority);
    NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

void RtcClock::clearSecondInterrupt()
{
    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
    CLEAR_BIT(RTC->ISR, RTC_ISR_ALRAF);
    RTC->WPR = 0xFFU;
    WRITE_REG(EXTI->PR, EXTI_PR_PR17);
}
