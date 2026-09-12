#include "DisplayLowLevel.hpp"

#include "stm32f469xx.h"
#include "stm32f4xx.h"
#include "system_stm32f4xx.h"


extern "C" uint8_t _sframebuffer_front[];
extern "C" uint8_t _sframebuffer_back[];

// Values: 0 - no response, 1 - OTM8009A, 2 - NT35510.
// Kept as a symbol so the detected panel can be inspected in a debugger.
volatile uint8_t detectedDisplayController = 0U;


namespace
{
    constexpr uint32_t timeout = 1000000U;
    constexpr uint32_t horizontalSync = 2U;
    constexpr uint32_t horizontalBackPorch = 34U;
    constexpr uint32_t horizontalFrontPorch = 34U;
    constexpr uint32_t verticalSync = 1U;
    constexpr uint32_t verticalBackPorch = 15U;
    constexpr uint32_t verticalFrontPorch = 16U;
    constexpr uint32_t dsiPllInputDivider = HSE_CLOCK_HZ / 4000000U;
    constexpr uint32_t dsiPllMultiplier = 125U;
    constexpr uint32_t dsiPhyClockKHz =
        (HSE_CLOCK_HZ / 1000U) * dsiPllMultiplier / dsiPllInputDivider;
    constexpr uint32_t laneByteClockKHz = dsiPhyClockKHz / 8U;
    constexpr uint32_t lcdClockKHz = 27429U;
    constexpr uint32_t dsiRgb888 = 5U;
    constexpr uint32_t frameBufferSize = 800U * 480U * 2U;

    void clearRgb565Buffer(uint8_t* const buffer)
    {
        for (uint32_t index = 0U; index < frameBufferSize; ++index)
            buffer[index] = 0U;
    }

}


bool DisplayLowLevel::enable()
{
    frontBuffer_ = _sframebuffer_front;
    backBuffer_ = _sframebuffer_back;
    clearRgb565Buffer(frontBuffer_);
    clearRgb565Buffer(backBuffer_);

    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOJEN);
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_LTDCEN | RCC_APB2ENR_DSIEN);
    static_cast<void>(RCC->APB2ENR);
    SET_BIT(RCC->APB2RSTR, RCC_APB2RSTR_LTDCRST | RCC_APB2RSTR_DSIRST);
    CLEAR_BIT(RCC->APB2RSTR, RCC_APB2RSTR_LTDCRST | RCC_APB2RSTR_DSIRST);

    MODIFY_REG(GPIOH->MODER, GPIO_MODER_MODER7, GPIO_MODER_MODER7_0);
    CLEAR_BIT(GPIOH->OTYPER, GPIO_ODR_OD7);
    MODIFY_REG(GPIOH->PUPDR, GPIO_PUPDR_PUPDR7, GPIO_PUPDR_PUPDR7_0);
    MODIFY_REG(GPIOH->OSPEEDR, GPIO_OSPEEDER_OSPEEDR7, GPIO_OSPEEDER_OSPEEDR7);
    resetController();

    if (!configureDsi())
        return false;

    configureLtdc();
    SET_BIT(DSI->CR, DSI_CR_EN);
    SET_BIT(DSI->WCR, DSI_WCR_DSIEN);
    if (!writeDcs(0x28U, 0x00U) || !initializeController())
        return false;
    return true;
}

void DisplayLowLevel::resetController() const
{
    GPIOH->BSRR = GPIO_BSRR_BR7;
    delayMilliseconds(20U);
    GPIOH->BSRR = GPIO_BSRR_BS7;
    delayMilliseconds(20U);
}

bool DisplayLowLevel::configureDsi()
{
    SET_BIT(DSI->WRPCR, DSI_WRPCR_REGEN);
    uint32_t remaining = timeout;
    while ((DSI->WISR & DSI_WISR_RRS) == 0U && remaining > 0U)
        --remaining;
    if (remaining == 0U)
        return false;

    MODIFY_REG(
        DSI->WRPCR,
        DSI_WRPCR_PLL_NDIV | DSI_WRPCR_PLL_IDF | DSI_WRPCR_PLL_ODF,
        (dsiPllMultiplier << DSI_WRPCR_PLL_NDIV_Pos)
        | (dsiPllInputDivider << DSI_WRPCR_PLL_IDF_Pos));
    SET_BIT(DSI->WRPCR, DSI_WRPCR_PLLEN);
    delayMilliseconds(1U);
    remaining = timeout;
    while ((DSI->WISR & DSI_WISR_PLLLS) == 0U && remaining > 0U)
        --remaining;
    if (remaining == 0U)
        return false;

    SET_BIT(DSI->CR, DSI_CR_EN);
    SET_BIT(DSI->PCTLR, DSI_PCTLR_CKE | DSI_PCTLR_DEN);
    MODIFY_REG(
        DSI->PCONFR,
        DSI_PCONFR_NL | DSI_PCONFR_SW_TIME,
        DSI_PCONFR_NL0 | (10UL << DSI_PCONFR_SW_TIME_Pos));
    remaining = timeout;
    const uint32_t stoppedLanes = DSI_PSR_PSS0 | DSI_PSR_PSS1 | DSI_PSR_PSSC;
    while ((DSI->PSR & stoppedLanes) != stoppedLanes && remaining > 0U)
        --remaining;
    if (remaining == 0U)
        return false;

    const uint32_t unitIntervalX4 = 4000000U / dsiPhyClockKHz;
    MODIFY_REG(DSI->WPCR[0], DSI_WPCR0_UIX4, unitIntervalX4);
    DSI->CCR = 4U << DSI_CCR_TXECKDIV_Pos;
    SET_BIT(DSI->CLCR, DSI_CLCR_DPCC);
    DSI->CLTCR = (35UL << DSI_CLTCR_HS2LP_TIME_Pos)
               | (35UL << DSI_CLTCR_LP2HS_TIME_Pos);
    DSI->DLTCR = (35UL << DSI_DLTCR_HS2LP_TIME_Pos)
               | (35UL << DSI_DLTCR_LP2HS_TIME_Pos);

    /* ST used both OTM8009A and NT35510 panels on this board.  Detect the
       controller in command mode before applying panel-specific settings. */
    SET_BIT(DSI->MCR, DSI_MCR_CMDM);
    MODIFY_REG(
        DSI->WCFGR,
        DSI_WCFGR_DSIM | DSI_WCFGR_COLMUX,
        DSI_WCFGR_DSIM | (dsiRgb888 << DSI_WCFGR_COLMUX_Pos));
    SET_BIT(DSI->PCR, DSI_PCR_BTAE);
    SET_BIT(DSI->CR, DSI_CR_EN);
    SET_BIT(DSI->WCR, DSI_WCR_DSIEN);

    controller_ = detectController();
    detectedDisplayController = static_cast<uint8_t>(controller_);

    CLEAR_BIT(DSI->WCR, DSI_WCR_DSIEN);
    CLEAR_BIT(DSI->CR, DSI_CR_EN);
    CLEAR_BIT(DSI->PCR, DSI_PCR_BTAE);
    resetController();

    if (controller_ == Controller::unknown)
        return false;

    DSI->LVCIDR = 0U;
    DSI->LCOLCR = dsiRgb888;
    DSI->VPCR = width;
    DSI->VCCR = 0U;
    DSI->VNPCR = 0xFFFU;
    configureVideoTimings();
    DSI->LPMCR = 16UL << DSI_LPMCR_LPSIZE_Pos;
    DSI->VMCR = DSI_VMCR_VMT1
              | DSI_VMCR_LPVSAE | DSI_VMCR_LPVBPE | DSI_VMCR_LPVFPE
              | DSI_VMCR_LPVAE | DSI_VMCR_LPHBPE | DSI_VMCR_LPHFPE
              | DSI_VMCR_LPCE;
    CLEAR_BIT(DSI->MCR, DSI_MCR_CMDM);
    MODIFY_REG(
        DSI->WCFGR,
        DSI_WCFGR_DSIM | DSI_WCFGR_COLMUX,
        dsiRgb888 << DSI_WCFGR_COLMUX_Pos);
    CLEAR_BIT(DSI->CR, DSI_CR_EN);
    return true;
}

void DisplayLowLevel::configureVideoTimings() const
{
    const bool nt35510 = controller_ == Controller::nt35510;
    const uint32_t hSync = nt35510 ? 120U : horizontalSync;
    const uint32_t hBackPorch = nt35510 ? 150U : horizontalBackPorch;
    const uint32_t hFrontPorch = nt35510 ? 150U : horizontalFrontPorch;
    const uint32_t vSync = nt35510 ? 2U : verticalSync;
    const uint32_t vBackPorch = nt35510 ? 34U : verticalBackPorch;
    const uint32_t vFrontPorch = nt35510 ? 34U : verticalFrontPorch;

    DSI->VHSACR = hSync * laneByteClockKHz / lcdClockKHz;
    DSI->VHBPCR = hBackPorch * laneByteClockKHz / lcdClockKHz;
    DSI->VLCR = (width + hSync + hBackPorch + hFrontPorch)
              * laneByteClockKHz / lcdClockKHz;
    DSI->VVSACR = vSync;
    DSI->VVBPCR = vBackPorch;
    DSI->VVFPCR = vFrontPorch;
    DSI->VVACR = height;
}

void DisplayLowLevel::configureLtdc() const
{
    CLEAR_BIT(RCC->CR, RCC_CR_PLLSAION);
    while ((RCC->CR & RCC_CR_PLLSAIRDY) != 0U) {}
    MODIFY_REG(
        RCC->PLLSAICFGR,
        RCC_PLLSAICFGR_PLLSAIN | RCC_PLLSAICFGR_PLLSAIP | RCC_PLLSAICFGR_PLLSAIR,
        (384UL << RCC_PLLSAICFGR_PLLSAIN_Pos)
        | (3UL << RCC_PLLSAICFGR_PLLSAIP_Pos)
        | (7UL << RCC_PLLSAICFGR_PLLSAIR_Pos));
    MODIFY_REG(
        RCC->DCKCFGR,
        RCC_DCKCFGR_PLLSAIDIVR | RCC_DCKCFGR_CK48MSEL,
        RCC_DCKCFGR_CK48MSEL);
    SET_BIT(RCC->CR, RCC_CR_PLLSAION);
    while ((RCC->CR & RCC_CR_PLLSAIRDY) == 0U) {}

    const uint32_t accumulatedHbp = horizontalSync + horizontalBackPorch;
    const uint32_t accumulatedVbp = verticalSync + verticalBackPorch;
    LTDC->SSCR = ((horizontalSync - 1U) << LTDC_SSCR_HSW_Pos)
               | ((verticalSync - 1U) << LTDC_SSCR_VSH_Pos);
    LTDC->BPCR = ((accumulatedHbp - 1U) << LTDC_BPCR_AHBP_Pos)
               | ((accumulatedVbp - 1U) << LTDC_BPCR_AVBP_Pos);
    LTDC->AWCR = ((accumulatedHbp + width - 1U) << LTDC_AWCR_AAW_Pos)
               | ((accumulatedVbp + height - 1U) << LTDC_AWCR_AAH_Pos);
    LTDC->TWCR = ((accumulatedHbp + width + horizontalFrontPorch - 1U)
                  << LTDC_TWCR_TOTALW_Pos)
               | ((accumulatedVbp + height + verticalFrontPorch - 1U)
                  << LTDC_TWCR_TOTALH_Pos);
    LTDC->BCCR = 0U;

    LTDC_Layer1->WHPCR = (accumulatedHbp << LTDC_LxWHPCR_WHSTPOS_Pos)
                       | ((accumulatedHbp + width - 1U) << LTDC_LxWHPCR_WHSPPOS_Pos);
    LTDC_Layer1->WVPCR = (accumulatedVbp << LTDC_LxWVPCR_WVSTPOS_Pos)
                       | ((accumulatedVbp + height - 1U) << LTDC_LxWVPCR_WVSPPOS_Pos);
    LTDC_Layer1->PFCR = 2U;
    LTDC_Layer1->CACR = 0xFFU;
    LTDC_Layer1->DCCR = 0U;
    LTDC_Layer1->BFCR = (6UL << LTDC_LxBFCR_BF1_Pos)
                      | (7UL << LTDC_LxBFCR_BF2_Pos);
    LTDC_Layer1->CFBAR = reinterpret_cast<uint32_t>(frontBuffer_);
    LTDC_Layer1->CFBLR = (width * 2U << LTDC_LxCFBLR_CFBP_Pos)
                       | (width * 2U + 3U);
    LTDC_Layer1->CFBLNR = height;
    SET_BIT(LTDC_Layer1->CR, LTDC_LxCR_LEN);
    CLEAR_BIT(LTDC->GCR, LTDC_GCR_PCPOL);
    SET_BIT(LTDC->GCR, LTDC_GCR_LTDCEN);
    LTDC->SRCR = LTDC_SRCR_IMR;
}

bool DisplayLowLevel::readDcs(const uint8_t command, uint8_t& value) const
{
    // Discard a stale response left by a previous read attempt.
    uint32_t staleWordsRemaining = 16U;
    while ((DSI->GPSR & DSI_GPSR_PRDFE) == 0U && staleWordsRemaining > 0U)
    {
        static_cast<void>(DSI->GPDR);
        --staleWordsRemaining;
    }

    if (!waitCommandFifo())
        return false;
    DSI->GHCR = 0x06U | (static_cast<uint32_t>(command) << 8U);

    uint32_t remaining = timeout;
    while ((DSI->GPSR & DSI_GPSR_PRDFE) != 0U && remaining > 0U)
        --remaining;
    if (remaining == 0U)
        return false;

    value = static_cast<uint8_t>(DSI->GPDR);
    return true;
}

DisplayLowLevel::Controller DisplayLowLevel::detectController() const
{
    uint8_t identifier = 0U;
    if (readDcs(0xDBU, identifier) && identifier == 0x80U)
        return Controller::nt35510;
    if (readDcs(0xDAU, identifier) && identifier == 0x40U)
        return Controller::otm8009a;
    return Controller::unknown;
}

bool DisplayLowLevel::initializeController() const
{
    return controller_ == Controller::nt35510
         ? initializeNt35510()
         : initializeOtm8009a();
}

bool DisplayLowLevel::initializeNt35510() const
{
    static constexpr uint8_t page1[] = {0x55U, 0xAAU, 0x52U, 0x08U, 0x01U};
    static constexpr uint8_t avdd[] = {0x03U, 0x03U, 0x03U};
    static constexpr uint8_t avddRatio[] = {0x46U, 0x46U, 0x46U};
    static constexpr uint8_t avee[] = {0x03U, 0x03U, 0x03U};
    static constexpr uint8_t aveeRatio[] = {0x36U, 0x36U, 0x36U};
    static constexpr uint8_t vcl[] = {0x00U, 0x00U, 0x02U};
    static constexpr uint8_t vclRatio[] = {0x26U, 0x26U, 0x26U};
    static constexpr uint8_t vgh[] = {0x01U};
    static constexpr uint8_t vghControl[] = {0x09U, 0x09U, 0x09U};
    static constexpr uint8_t vghRatio[] = {0x36U, 0x36U, 0x36U};
    static constexpr uint8_t vgl[] = {0x08U, 0x08U, 0x08U};
    static constexpr uint8_t vglRatio[] = {0x26U, 0x26U, 0x26U};
    static constexpr uint8_t vgmp[] = {0x00U, 0x80U, 0x00U};
    static constexpr uint8_t vgmn[] = {0x00U, 0x80U, 0x00U};
    static constexpr uint8_t vcom[] = {0x00U, 0x50U};
    static constexpr uint8_t page0[] = {0x55U, 0xAAU, 0x52U, 0x08U, 0x00U};
    static constexpr uint8_t displayControl[] = {0xFCU, 0x00U};
    static constexpr uint8_t sourceHold[] = {0x03U};
    static constexpr uint8_t resolution[] = {0x50U};
    static constexpr uint8_t gateEq[] = {0x00U, 0x00U};
    static constexpr uint8_t sourceEq[] = {0x01U, 0x02U, 0x02U, 0x02U};
    static constexpr uint8_t inversion[] = {0x00U, 0x00U, 0x00U};
    static constexpr uint8_t sourceTiming[] = {0x03U, 0x00U, 0x00U};
    static constexpr uint8_t gateTiming[] = {0x01U};
    static constexpr uint8_t columns[] = {0x00U, 0x00U, 0x03U, 0x1FU};
    static constexpr uint8_t pages[] = {0x00U, 0x00U, 0x01U, 0xDFU};

    const auto writeData = [this](const uint8_t command, const auto& values)
    {
        return writeDcs(command, values, sizeof(values));
    };

    if (!writeData(0xF0U, page1)
        || !writeData(0xB0U, avdd)
        || !writeData(0xB6U, avddRatio)
        || !writeData(0xB1U, avee)
        || !writeData(0xB7U, aveeRatio)
        || !writeData(0xB2U, vcl)
        || !writeData(0xB8U, vclRatio)
        || !writeData(0xBFU, vgh)
        || !writeData(0xB3U, vghControl)
        || !writeData(0xB9U, vghRatio)
        || !writeData(0xB5U, vgl)
        || !writeData(0xBAU, vglRatio)
        || !writeData(0xBCU, vgmp)
        || !writeData(0xBDU, vgmn)
        || !writeData(0xBEU, vcom)
        || !writeData(0xF0U, page0)
        || !writeData(0xB1U, displayControl)
        || !writeData(0xB6U, sourceHold)
        || !writeData(0xB5U, resolution)
        || !writeData(0xB7U, gateEq)
        || !writeData(0xB8U, sourceEq)
        || !writeData(0xBCU, inversion)
        || !writeData(0xCCU, sourceTiming)
        || !writeData(0xBAU, gateTiming))
        return false;

    delayMilliseconds(200U);
    if (!writeDcs(0x36U, 0x60U)
        || !writeData(0x2AU, columns)
        || !writeData(0x2BU, pages)
        || !writeDcs(0x11U))
        return false;
    delayMilliseconds(20U);
    return writeDcs(0x3AU, 0x77U)
        && writeDcs(0x51U, 0x7FU)
        && writeDcs(0x53U, 0x2CU)
        && writeDcs(0x55U, 0x02U)
        && writeDcs(0x5EU, 0xFFU)
        && writeDcs(0x29U)
        && writeDcs(0x2CU);
}

bool DisplayLowLevel::initializeOtm8009a() const
{
    // The OTM8009A does not leave reset in a state in which standard DCS
    // commands alone are sufficient.  The following values configure its
    // analogue supplies, source/gate drivers and gamma tables.  They are the
    // panel-specific sequence specified by ST for STM32F469I-DISCO.
    static constexpr uint8_t command2Enable[] = {0x80U, 0x09U, 0x01U};
    static constexpr uint8_t oriseCommand2[] = {0x80U, 0x09U};
    static constexpr uint8_t gamma[] = {
        0x00U, 0x09U, 0x0FU, 0x0EU, 0x07U, 0x10U, 0x0BU, 0x0AU,
        0x04U, 0x07U, 0x0BU, 0x08U, 0x0FU, 0x10U, 0x0AU, 0x01U};
    static constexpr uint8_t gvdd[] = {0x79U, 0x79U};
    static constexpr uint8_t panelTiming[] = {0x00U, 0x01U};
    static constexpr uint8_t goaVst[] = {0x85U, 0x01U, 0x00U, 0x84U, 0x01U, 0x00U};
    static constexpr uint8_t goaClock1[] = {
        0x18U, 0x04U, 0x03U, 0x39U, 0x00U, 0x00U, 0x00U,
        0x18U, 0x03U, 0x03U, 0x3AU, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t goaClock2[] = {
        0x18U, 0x02U, 0x03U, 0x3BU, 0x00U, 0x00U, 0x00U,
        0x18U, 0x01U, 0x03U, 0x3CU, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t goaEclk[] = {
        0x01U, 0x01U, 0x20U, 0x20U, 0x00U, 0x00U, 0x01U, 0x02U, 0x00U, 0x00U};
    static constexpr uint8_t zeros10[10] = {};
    static constexpr uint8_t zeros15[15] = {};
    static constexpr uint8_t cb50[] = {
        0x00U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t cbD0[] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x04U, 0x04U,
        0x04U, 0x04U, 0x04U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t ones10[] = {
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU};
    static constexpr uint8_t cc00[] = {
        0x00U, 0x26U, 0x09U, 0x0BU, 0x01U, 0x25U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t cc90[] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x26U, 0x0AU, 0x0CU, 0x02U};
    static constexpr uint8_t ccA0[] = {
        0x25U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t ccB0[] = {
        0x00U, 0x25U, 0x0CU, 0x0AU, 0x02U, 0x26U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t ccD0[] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x25U, 0x0BU, 0x09U, 0x01U};
    static constexpr uint8_t ccE0[] = {
        0x26U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static constexpr uint8_t command2Disable[] = {0xFFU, 0xFFU, 0xFFU};

    const auto selectAddress = [this](const uint8_t address)
    {
        return writeDcs(0x00U, address);
    };
    const auto writeData = [this](const uint8_t command, const auto& values)
    {
        return writeDcs(command, values, sizeof(values));
    };

    if (!selectAddress(0x00U)
        || !writeData(0xFFU, command2Enable)
        || !selectAddress(0x80U)
        || !writeData(0xFFU, oriseCommand2)
        || !selectAddress(0x80U) || !writeDcs(0xC4U, 0x30U))
        return false;
    delayMilliseconds(10U);
    if (!selectAddress(0x8AU) || !writeDcs(0xC4U, 0x40U))
        return false;
    delayMilliseconds(10U);

    if (!selectAddress(0xB1U) || !writeDcs(0xC5U, 0xA9U)
        || !selectAddress(0x91U) || !writeDcs(0xC5U, 0x34U)
        || !selectAddress(0xB4U) || !writeDcs(0xC0U, 0x50U)
        || !selectAddress(0x00U) || !writeDcs(0xD9U, 0x4EU)
        || !selectAddress(0x81U) || !writeDcs(0xC1U, 0x66U)
        || !selectAddress(0xA1U) || !writeDcs(0xC1U, 0x08U)
        || !selectAddress(0x92U) || !writeDcs(0xC5U, 0x01U)
        || !selectAddress(0x95U) || !writeDcs(0xC5U, 0x34U)
        || !selectAddress(0x00U) || !writeData(0xD8U, gvdd)
        || !selectAddress(0x94U) || !writeDcs(0xC5U, 0x33U)
        || !selectAddress(0xA3U) || !writeDcs(0xC0U, 0x1BU)
        || !selectAddress(0x82U) || !writeDcs(0xC5U, 0x83U)
        || !selectAddress(0x81U) || !writeDcs(0xC4U, 0x83U)
        || !selectAddress(0xA1U) || !writeDcs(0xC1U, 0x0EU)
        || !selectAddress(0xA6U) || !writeData(0xB3U, panelTiming)
        || !selectAddress(0x80U) || !writeData(0xCEU, goaVst)
        || !selectAddress(0xA0U) || !writeData(0xCEU, goaClock1)
        || !selectAddress(0xB0U) || !writeData(0xCEU, goaClock2)
        || !selectAddress(0xC0U) || !writeData(0xCFU, goaEclk)
        || !selectAddress(0xD0U) || !writeDcs(0xCFU, 0x00U)
        || !selectAddress(0x80U) || !writeData(0xCBU, zeros10)
        || !selectAddress(0x90U) || !writeData(0xCBU, zeros15)
        || !selectAddress(0xA0U) || !writeData(0xCBU, zeros15)
        || !selectAddress(0xB0U) || !writeData(0xCBU, zeros10)
        || !selectAddress(0xC0U) || !writeData(0xCBU, cb50)
        || !selectAddress(0xD0U) || !writeData(0xCBU, cbD0)
        || !selectAddress(0xE0U) || !writeData(0xCBU, zeros10)
        || !selectAddress(0xF0U) || !writeData(0xCBU, ones10)
        || !selectAddress(0x80U) || !writeData(0xCCU, cc00)
        || !selectAddress(0x90U) || !writeData(0xCCU, cc90)
        || !selectAddress(0xA0U) || !writeData(0xCCU, ccA0)
        || !selectAddress(0xB0U) || !writeData(0xCCU, ccB0)
        || !selectAddress(0xC0U) || !writeData(0xCCU, ccD0)
        || !selectAddress(0xD0U) || !writeData(0xCCU, ccE0)
        || !selectAddress(0x81U) || !writeDcs(0xC5U, 0x66U)
        || !selectAddress(0xB6U) || !writeDcs(0xF5U, 0x06U)
        || !selectAddress(0xB1U) || !writeDcs(0xC6U, 0x06U)
        || !selectAddress(0x00U) || !writeData(0xFFU, command2Disable)
        || !selectAddress(0x00U)
        || !writeData(0xE1U, gamma)
        || !selectAddress(0x00U)
        || !writeData(0xE2U, gamma))
        return false;

    const uint8_t columns[] = {0x00U, 0x00U, 0x03U, 0x1FU};
    const uint8_t pages[] = {0x00U, 0x00U, 0x01U, 0xDFU};
    if (!writeDcs(0x11U))
        return false;
    delayMilliseconds(120U);
    if (!writeDcs(0x3AU, 0x77U)
        || !writeDcs(0x36U, 0x60U)
        || !writeDcs(0x2AU, columns, sizeof(columns))
        || !writeDcs(0x2BU, pages, sizeof(pages))
        || !writeDcs(0x51U, 0xFFU)
        || !writeDcs(0x53U, 0x24U)
        || !writeDcs(0x55U, 0x00U)
        || !writeDcs(0x5EU, 0xFFU)
        || !writeDcs(0x29U))
        return false;
    delayMilliseconds(20U);
    return writeDcs(0x2CU);
}

bool DisplayLowLevel::writeDcs(const uint8_t command) const
{
    if (!waitCommandFifo())
        return false;
    DSI->GHCR = 0x05U | (static_cast<uint32_t>(command) << 8U);
    return waitCommandFifo();
}

bool DisplayLowLevel::writeDcs(const uint8_t command, const uint8_t value) const
{
    if (!waitCommandFifo())
        return false;
    DSI->GHCR = 0x15U | (static_cast<uint32_t>(command) << 8U)
              | (static_cast<uint32_t>(value) << 16U);
    return waitCommandFifo();
}

bool DisplayLowLevel::writeDcs(
    const uint8_t command,
    const uint8_t* const data,
    const uint32_t size) const
{
    if (data == nullptr || !waitCommandFifo())
        return false;
    uint32_t payload = command;
    uint32_t bytePosition = 1U;
    for (uint32_t index = 0U; index < size; ++index)
    {
        payload |= static_cast<uint32_t>(data[index]) << (bytePosition * 8U);
        if (++bytePosition == 4U)
        {
            DSI->GPDR = payload;
            payload = 0U;
            bytePosition = 0U;
        }
    }
    if (bytePosition != 0U)
        DSI->GPDR = payload;
    DSI->GHCR = 0x39U | ((size + 1U) << DSI_GHCR_WCLSB_Pos);
    return waitCommandFifo();
}

bool DisplayLowLevel::waitCommandFifo() const
{
    uint32_t remaining = timeout;
    while ((DSI->GPSR & DSI_GPSR_CMDFE) == 0U && remaining > 0U)
        --remaining;
    return remaining != 0U;
}

void DisplayLowLevel::delayMilliseconds(const uint32_t milliseconds)
{
    const uint32_t cycles = SystemCoreClock / 8000U;
    for (uint32_t millisecond = 0U; millisecond < milliseconds; ++millisecond)
        for (uint32_t cycle = 0U; cycle < cycles; ++cycle)
            __NOP();
}

uint8_t* DisplayLowLevel::getFrontBuffer() const
{
    return frontBuffer_;
}

uint8_t* DisplayLowLevel::getBackBuffer() const
{
    return backBuffer_;
}

void DisplayLowLevel::swapBuffers()
{
    uint8_t* const previousFrontBuffer = frontBuffer_;
    frontBuffer_ = backBuffer_;
    backBuffer_ = previousFrontBuffer;

    LTDC_Layer1->CFBAR = reinterpret_cast<uint32_t>(frontBuffer_);
    LTDC->SRCR = LTDC_SRCR_IMR;
    refresh();
}

void DisplayLowLevel::refresh() const
{
    // Video mode streams the active LTDC layer continuously.
}
