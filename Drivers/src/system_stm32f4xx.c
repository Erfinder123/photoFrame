
#include <stdint.h>
#include "system_stm32f4xx.h"
#include "cmsis_gcc.h"
#include "stm32f469xx.h"
#include "stm32f4xx.h"


static void SDIOGPIOConfig(void);
static void SDIOClockConfig(void);
static void RTCClockConfig(void);
static void SDRAMGPIOConfig(void);
static void SDRAMConfig(void);
static void configureFmcPins(GPIO_TypeDef* gpio, uint32_t pins);
static int waitForSdram(void);


uint32_t SystemCoreClock = 16000000U;

void SystemInit()
{
    SCB->CPACR |= 3UL << 10*2 | 3UL << 11*2;
    __DSB();
    __ISB();

    SystemCoreClockConfig();
    RTCClockConfig();
    SDRAMGPIOConfig();
    SDRAMConfig();
    SDIOGPIOConfig();
    SDIOClockConfig();
}

static void RTCClockConfig(void)
{
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_PWREN);
    (void)RCC->APB1ENR;
    SET_BIT(PWR->CR, PWR_CR_DBP);

    if (READ_BIT(RCC->BDCR, RCC_BDCR_RTCSEL) != RCC_BDCR_RTCSEL_0)
    {
        SET_BIT(RCC->BDCR, RCC_BDCR_BDRST);
        CLEAR_BIT(RCC->BDCR, RCC_BDCR_BDRST);
    }

    SET_BIT(RCC->BDCR, RCC_BDCR_LSEON);
    while (READ_BIT(RCC->BDCR, RCC_BDCR_LSERDY) == 0U) {}
    MODIFY_REG(RCC->BDCR, RCC_BDCR_RTCSEL, RCC_BDCR_RTCSEL_0);
    SET_BIT(RCC->BDCR, RCC_BDCR_RTCEN);

    const uint32_t rtcWasInitialized = READ_BIT(RTC->ISR, RTC_ISR_INITS);
    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
    SET_BIT(RTC->ISR, RTC_ISR_INIT);
    while (READ_BIT(RTC->ISR, RTC_ISR_INITF) == 0U) {}

    RTC->PRER = (15UL << RTC_PRER_PREDIV_A_Pos)
              | (2047UL << RTC_PRER_PREDIV_S_Pos);
    CLEAR_BIT(RTC->CR, RTC_CR_FMT);
    if (rtcWasInitialized == 0U)
    {
        RTC->TR = 0U;
        RTC->DR = (1UL << RTC_DR_YU_Pos)
                | (1UL << RTC_DR_MU_Pos)
                | (1UL << RTC_DR_WDU_Pos)
                | (1UL << RTC_DR_DU_Pos);
    }
    CLEAR_BIT(RTC->ISR, RTC_ISR_INIT);
    RTC->WPR = 0xFFU;
}

static void configureFmcPins(GPIO_TypeDef* gpio, const uint32_t pins)
{
    for (uint32_t pin = 0U; pin < 16U; ++pin)
    {
        const uint32_t pinBit = 1UL << pin;
        if ((pins & pinBit) == 0U)
            continue;

        const uint32_t modePosition = pin * 2U;
        MODIFY_REG(gpio->MODER, 3UL << modePosition, 2UL << modePosition);
        CLEAR_BIT(gpio->OTYPER, pinBit);
        MODIFY_REG(gpio->OSPEEDR, 3UL << modePosition, 3UL << modePosition);
        MODIFY_REG(gpio->PUPDR, 3UL << modePosition, 1UL << modePosition);

        const uint32_t alternateRegister = pin / 8U;
        const uint32_t alternatePosition = (pin % 8U) * 4U;
        MODIFY_REG(
            gpio->AFR[alternateRegister],
            0xFUL << alternatePosition,
            12UL << alternatePosition);
    }
}

static void SDRAMGPIOConfig(void)
{
    SET_BIT(
        RCC->AHB1ENR,
        RCC_AHB1ENR_GPIOCEN
        | RCC_AHB1ENR_GPIODEN
        | RCC_AHB1ENR_GPIOEEN
        | RCC_AHB1ENR_GPIOFEN
        | RCC_AHB1ENR_GPIOGEN
        | RCC_AHB1ENR_GPIOHEN
        | RCC_AHB1ENR_GPIOIEN);
    (void)RCC->AHB1ENR;

    configureFmcPins(GPIOC, GPIO_ODR_OD0);
    configureFmcPins(
        GPIOD,
        GPIO_ODR_OD0 | GPIO_ODR_OD1 | GPIO_ODR_OD8 | GPIO_ODR_OD9
        | GPIO_ODR_OD10 | GPIO_ODR_OD14 | GPIO_ODR_OD15);
    configureFmcPins(
        GPIOE,
        GPIO_ODR_OD0 | GPIO_ODR_OD1 | GPIO_ODR_OD7 | GPIO_ODR_OD8
        | GPIO_ODR_OD9 | GPIO_ODR_OD10 | GPIO_ODR_OD11 | GPIO_ODR_OD12
        | GPIO_ODR_OD13 | GPIO_ODR_OD14 | GPIO_ODR_OD15);
    configureFmcPins(
        GPIOF,
        GPIO_ODR_OD0 | GPIO_ODR_OD1 | GPIO_ODR_OD2 | GPIO_ODR_OD3
        | GPIO_ODR_OD4 | GPIO_ODR_OD5 | GPIO_ODR_OD11 | GPIO_ODR_OD12
        | GPIO_ODR_OD13 | GPIO_ODR_OD14 | GPIO_ODR_OD15);
    configureFmcPins(
        GPIOG,
        GPIO_ODR_OD0 | GPIO_ODR_OD1 | GPIO_ODR_OD4 | GPIO_ODR_OD5
        | GPIO_ODR_OD8 | GPIO_ODR_OD15);
    configureFmcPins(
        GPIOH,
        GPIO_ODR_OD2 | GPIO_ODR_OD3 | GPIO_ODR_OD8 | GPIO_ODR_OD9
        | GPIO_ODR_OD10 | GPIO_ODR_OD11 | GPIO_ODR_OD12 | GPIO_ODR_OD13
        | GPIO_ODR_OD14 | GPIO_ODR_OD15);
    configureFmcPins(
        GPIOI,
        GPIO_ODR_OD0 | GPIO_ODR_OD1 | GPIO_ODR_OD2 | GPIO_ODR_OD3
        | GPIO_ODR_OD4 | GPIO_ODR_OD5 | GPIO_ODR_OD6 | GPIO_ODR_OD7
        | GPIO_ODR_OD9 | GPIO_ODR_OD10);
}

static int waitForSdram(void)
{
    uint32_t timeout = 0xFFFFU;
    while ((FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) != 0U && timeout > 0U)
        --timeout;
    return timeout != 0U;
}

static void SDRAMConfig(void)
{
    SET_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FMCEN);
    (void)RCC->AHB3ENR;

    /* MT48LC4M32B2: 8 column bits, 12 row bits, 32-bit bus, four banks. */
    FMC_Bank5_6->SDCR[0] = 0x000019E4UL;
    FMC_Bank5_6->SDTR[0] = 0x01115351UL;

    FMC_Bank5_6->SDCMR = FMC_SDCMR_CTB1 | FMC_SDCMR_MODE_0;
    if (!waitForSdram())
        return;

    for (volatile uint32_t delay = 0U; delay < 1000U; ++delay) {}

    FMC_Bank5_6->SDCMR = FMC_SDCMR_CTB1 | FMC_SDCMR_MODE_1;
    if (!waitForSdram())
        return;

    FMC_Bank5_6->SDCMR = FMC_SDCMR_CTB1
                       | FMC_SDCMR_MODE_0
                       | FMC_SDCMR_MODE_1
                       | (7UL << FMC_SDCMR_NRFS_Pos);
    if (!waitForSdram())
        return;

    const uint32_t modeRegister = 0x230U;
    FMC_Bank5_6->SDCMR = FMC_SDCMR_CTB1
                       | FMC_SDCMR_MODE_2
                       | (modeRegister << FMC_SDCMR_MRD_Pos);
    if (!waitForSdram())
        return;

    const uint32_t sdramClock = SystemCoreClock / 2U;
    const uint32_t refreshCount = ((sdramClock / 1000U) * 64U / 4096U) - 20U;
    MODIFY_REG(
        FMC_Bank5_6->SDRTR,
        FMC_SDRTR_COUNT,
        refreshCount << FMC_SDRTR_COUNT_Pos);
}

void SystemCoreClockConfig()
{
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_PWREN);
    (void)RCC->APB1ENR;
    MODIFY_REG(PWR->CR, PWR_CR_VOS, PWR_CR_VOS);

    SET_BIT(RCC->CR, RCC_CR_HSEON);
    while(READ_BIT(RCC->CR, RCC_CR_HSERDY) != RCC_CR_HSERDY){}
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_HSE);
    while(READ_BIT(RCC->CFGR, RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE){}

    MODIFY_REG(
        RCC->PLLCFGR,
        RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP
        | RCC_PLLCFGR_PLLQ | RCC_PLLCFGR_PLLR | RCC_PLLCFGR_PLLSRC,
        (HSE_CLOCK_HZ / 1000000U)
        | (360UL << RCC_PLLCFGR_PLLN_Pos)
        | (7UL << RCC_PLLCFGR_PLLQ_Pos)
        | (6UL << RCC_PLLCFGR_PLLR_Pos)
        | RCC_PLLCFGR_PLLSRC_HSE);

    SET_BIT(RCC->CR, RCC_CR_PLLON);
    while (READ_BIT(RCC->CR, RCC_CR_PLLRDY) == 0U) {}

    SET_BIT(PWR->CR, PWR_CR_ODEN);
    while (READ_BIT(PWR->CSR, PWR_CSR_ODRDY) == 0U) {}
    SET_BIT(PWR->CR, PWR_CR_ODSWEN);
    while (READ_BIT(PWR->CSR, PWR_CSR_ODSWRDY) == 0U) {}

    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    MODIFY_REG(
        RCC->CFGR,
        RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2,
        RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2);
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
    while (READ_BIT(RCC->CFGR, RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SystemCoreClock = SYSTEM_CLOCK_HZ;
}

static void SDIOGPIOConfig(void)
{
    const uint32_t dataAndClockPins = GPIO_ODR_OD8
                                    | GPIO_ODR_OD9
                                    | GPIO_ODR_OD10
                                    | GPIO_ODR_OD11
                                    | GPIO_ODR_OD12;
    const uint32_t dataAndClockModeMask = GPIO_MODER_MODER8
                                        | GPIO_MODER_MODER9
                                        | GPIO_MODER_MODER10
                                        | GPIO_MODER_MODER11
                                        | GPIO_MODER_MODER12;
    const uint32_t dataAndClockAlternateMode = GPIO_MODER_MODER8_1
                                             | GPIO_MODER_MODER9_1
                                             | GPIO_MODER_MODER10_1
                                             | GPIO_MODER_MODER11_1
                                             | GPIO_MODER_MODER12_1;
    const uint32_t dataAndClockHighSpeed = GPIO_OSPEEDER_OSPEEDR8
                                         | GPIO_OSPEEDER_OSPEEDR9
                                         | GPIO_OSPEEDER_OSPEEDR10
                                         | GPIO_OSPEEDER_OSPEEDR11
                                         | GPIO_OSPEEDER_OSPEEDR12;
    const uint32_t dataAndClockAlternateFunctionMask = GPIO_AFRH_AFSEL8
                                                     | GPIO_AFRH_AFSEL9
                                                     | GPIO_AFRH_AFSEL10
                                                     | GPIO_AFRH_AFSEL11
                                                     | GPIO_AFRH_AFSEL12;
    const uint32_t dataAndClockAlternateFunction = (12UL << GPIO_AFRH_AFSEL8_Pos)
                                                 | (12UL << GPIO_AFRH_AFSEL9_Pos)
                                                 | (12UL << GPIO_AFRH_AFSEL10_Pos)
                                                 | (12UL << GPIO_AFRH_AFSEL11_Pos)
                                                 | (12UL << GPIO_AFRH_AFSEL12_Pos);
    const uint32_t dataPullUp = GPIO_PUPDR_PUPDR8_0
                              | GPIO_PUPDR_PUPDR9_0
                              | GPIO_PUPDR_PUPDR10_0
                              | GPIO_PUPDR_PUPDR11_0;

    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN);
    (void)RCC->AHB1ENR;

    MODIFY_REG(GPIOC->MODER, dataAndClockModeMask, dataAndClockAlternateMode);
    CLEAR_BIT(GPIOC->OTYPER, dataAndClockPins);
    MODIFY_REG(GPIOC->OSPEEDR, dataAndClockModeMask, dataAndClockHighSpeed);
    MODIFY_REG(GPIOC->PUPDR, dataAndClockModeMask, dataPullUp);
    MODIFY_REG(
        GPIOC->AFR[1],
        dataAndClockAlternateFunctionMask,
        dataAndClockAlternateFunction);

    MODIFY_REG(GPIOD->MODER, GPIO_MODER_MODER2, GPIO_MODER_MODER2_1);
    CLEAR_BIT(GPIOD->OTYPER, GPIO_ODR_OD2);
    MODIFY_REG(GPIOD->OSPEEDR, GPIO_OSPEEDER_OSPEEDR2, GPIO_OSPEEDER_OSPEEDR2);
    MODIFY_REG(GPIOD->PUPDR, GPIO_PUPDR_PUPDR2, GPIO_PUPDR_PUPDR2_0);
    MODIFY_REG(
        GPIOD->AFR[0],
        GPIO_AFRL_AFSEL2,
        12UL << GPIO_AFRL_AFSEL2_Pos);
}

static void SDIOClockConfig(void)
{
    /* SDIOCLK = 48 MHz PLLSAI clock, SDIO_CK = SDIOCLK / (CLKDIV + 2). */
    const uint32_t identificationClockDivider = 118U;

    CLEAR_BIT(RCC->DCKCFGR, RCC_DCKCFGR_SDIOSEL);
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_SDIOEN);
    (void)RCC->APB2ENR;

    SET_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SDIORST);
    CLEAR_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SDIORST);

    MODIFY_REG(
        SDIO->CLKCR,
        SDIO_CLKCR_CLKDIV
        | SDIO_CLKCR_CLKEN
        | SDIO_CLKCR_PWRSAV
        | SDIO_CLKCR_BYPASS
        | SDIO_CLKCR_WIDBUS
        | SDIO_CLKCR_NEGEDGE
        | SDIO_CLKCR_HWFC_EN,
        identificationClockDivider | SDIO_CLKCR_CLKEN);

    MODIFY_REG(SDIO->POWER, SDIO_POWER_PWRCTRL, SDIO_POWER_PWRCTRL);
}
