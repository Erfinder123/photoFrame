
#include "CardLowLevel.hpp"
#include "stm32f469xx.h"
#include "stm32f4xx.h"

#include <cstring>


void CardLowLevel::configureCardDetectInterrupt(
    const uint32_t interruptPriority) const
{
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIOGEN);
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN);
    static_cast<void>(RCC->AHB1ENR);
    static_cast<void>(RCC->APB2ENR);

    CLEAR_BIT(GPIOG->MODER, GPIO_MODER_MODER2);
    MODIFY_REG(GPIOG->PUPDR, GPIO_PUPDR_PUPDR2, GPIO_PUPDR_PUPDR2_0);

    MODIFY_REG(
        SYSCFG->EXTICR[0],
        SYSCFG_EXTICR1_EXTI2,
        SYSCFG_EXTICR1_EXTI2_PG);

    SET_BIT(EXTI->RTSR, EXTI_RTSR_TR2);
    SET_BIT(EXTI->FTSR, EXTI_FTSR_TR2);
    EXTI->PR = EXTI_PR_PR2;
    SET_BIT(EXTI->IMR, EXTI_IMR_MR2);

    NVIC_SetPriority(EXTI2_IRQn, interruptPriority);
    NVIC_EnableIRQ(EXTI2_IRQn);
}

bool CardLowLevel::isCardDetected() const
{
    return READ_BIT(GPIOG->IDR, GPIO_IDR_ID2) == 0U;
}

void CardLowLevel::clearCardDetectInterrupt() const
{
    EXTI->PR = EXTI_PR_PR2;
}


void CardLowLevel::prepareForInitialization()
{
    constexpr uint32_t identificationClockDivider = 118U;

    resetBlockTransfer();

    CLEAR_BIT(SDIO->CLKCR, SDIO_CLKCR_CLKEN);
    MODIFY_REG(
        SDIO->CLKCR,
        SDIO_CLKCR_CLKDIV
        | SDIO_CLKCR_BYPASS
        | SDIO_CLKCR_WIDBUS
        | SDIO_CLKCR_NEGEDGE
        | SDIO_CLKCR_HWFC_EN
        | SDIO_CLKCR_PWRSAV,
        identificationClockDivider);

    SDIO->ICR = SDIO_ICR_CCRCFAILC
              | SDIO_ICR_DCRCFAILC
              | SDIO_ICR_CTIMEOUTC
              | SDIO_ICR_DTIMEOUTC
              | SDIO_ICR_TXUNDERRC
              | SDIO_ICR_RXOVERRC
              | SDIO_ICR_CMDRENDC
              | SDIO_ICR_CMDSENTC
              | SDIO_ICR_DATAENDC
              | SDIO_ICR_DBCKENDC;

    SET_BIT(SDIO->CLKCR, SDIO_CLKCR_CLKEN);
}

void CardLowLevel::sendCommand(
    const uint8_t command,
    const uint32_t argument,
    const ResponseType responseType)
{
    lastCommand_ = command;
    SDIO->ICR = SDIO_ICR_CCRCFAILC
              | SDIO_ICR_CTIMEOUTC
              | SDIO_ICR_CMDRENDC
              | SDIO_ICR_CMDSENTC;

    uint32_t commandRegister = static_cast<uint32_t>(command) & SDIO_CMD_CMDINDEX;

    switch (responseType)
    {
        case ResponseType::NONE:
            break;
        case ResponseType::SHORT:
            commandRegister |= SDIO_CMD_WAITRESP_0;
            break;
        case ResponseType::LONG:
            commandRegister |= SDIO_CMD_WAITRESP;
            break;
    }

    SDIO->ARG = argument;
    SDIO->CMD = commandRegister | SDIO_CMD_CPSMEN;
}

uint8_t CardLowLevel::getLastCommand() const
{
    return lastCommand_;
}

uint32_t CardLowLevel::getResponse() const
{
    return SDIO->RESP1;
}

LongResponse CardLowLevel::getLongResponse() const
{
    return LongResponse{
        SDIO->RESP1,
        SDIO->RESP2,
        SDIO->RESP3,
        SDIO->RESP4
    };
}

Status CardLowLevel::getStatus()
{
    const uint32_t statusRegister = SDIO->STA;

    if (READ_BIT(statusRegister, SDIO_STA_CTIMEOUT) == SDIO_STA_CTIMEOUT)
        return Status::COMMAND_RESPONSE_TIMEOUT;

    if (READ_BIT(statusRegister, SDIO_STA_CCRCFAIL) == SDIO_STA_CCRCFAIL)
        return Status::COMMAND_RESPONSE_RECEIVED_FAILED;

    if (READ_BIT(statusRegister, SDIO_STA_CMDREND) == SDIO_STA_CMDREND)
        return Status::COMMAND_RESPONSE_RECEIVED_OK;

    if (READ_BIT(statusRegister, SDIO_STA_CMDSENT) == SDIO_STA_CMDSENT)
        return Status::COMMAND_SENT;

    return Status::EMPTY_STATUS;
}

void CardLowLevel::setTransferClock()
{
    /* CK48 / (CLKDIV + 2) = 48 MHz / 4 = 12 MHz.  Keep the first data
       transfers conservative until the complete SDIO path is verified. */
    constexpr uint32_t transferClockDivider = 2U;

    CLEAR_BIT(SDIO->CLKCR, SDIO_CLKCR_CLKEN);
    MODIFY_REG(
        SDIO->CLKCR,
        SDIO_CLKCR_CLKDIV | SDIO_CLKCR_BYPASS,
        transferClockDivider);
    SET_BIT(SDIO->CLKCR, SDIO_CLKCR_CLKEN);
}

void CardLowLevel::setFourBitBus()
{
    MODIFY_REG(SDIO->CLKCR, SDIO_CLKCR_WIDBUS, SDIO_CLKCR_WIDBUS_0);
}

bool CardLowLevel::startReadBlocks(uint8_t* buffer, const uint32_t count)
{
    if (count == 0U || count > 511U)
        return false;

    uint32_t* memory = dmaBuffer_;
    if (count > 1U)
    {
        if ((reinterpret_cast<uintptr_t>(buffer) % alignof(uint32_t)) != 0U)
            return false;
        memory = reinterpret_cast<uint32_t*>(buffer);
    }

    readBuffer_ = count == 1U ? buffer : nullptr;
    readTransfer_ = true;
    configureDma(true, memory, blockWordCount * count);
    return true;
}

bool CardLowLevel::startWriteBlocks(const uint8_t* buffer, const uint32_t count)
{
    if (count == 0U || count > 511U)
        return false;

    uint32_t* memory = dmaBuffer_;
    if (count == 1U)
    {
        std::memcpy(dmaBuffer_, buffer, sdSectorSize);
    }
    else
    {
        if ((reinterpret_cast<uintptr_t>(buffer) % alignof(uint32_t)) != 0U)
            return false;
        memory = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(buffer));
    }

    readBuffer_ = nullptr;
    readTransfer_ = false;
    configureDma(false, memory, blockWordCount * count);
    return true;
}

void CardLowLevel::configureDma(
    const bool read,
    uint32_t* const memory,
    const uint32_t wordCount)
{
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_DMA2EN);
    static_cast<void>(RCC->AHB1ENR);

    CLEAR_BIT(DMA2_Stream3->CR, DMA_SxCR_EN);
    while (READ_BIT(DMA2_Stream3->CR, DMA_SxCR_EN) != 0U) {}

    DMA2->LIFCR = DMA_LIFCR_CTCIF3
                | DMA_LIFCR_CHTIF3
                | DMA_LIFCR_CTEIF3
                | DMA_LIFCR_CDMEIF3
                | DMA_LIFCR_CFEIF3;

    DMA2_Stream3->PAR = reinterpret_cast<uint32_t>(&SDIO->FIFO);
    DMA2_Stream3->M0AR = reinterpret_cast<uint32_t>(memory);
    DMA2_Stream3->NDTR = wordCount;
    DMA2_Stream3->CR = DMA_SxCR_CHSEL_2
                     | DMA_SxCR_PL_1
                     | DMA_SxCR_MSIZE_1
                     | DMA_SxCR_PSIZE_1
                     | DMA_SxCR_MBURST_0
                     | DMA_SxCR_PBURST_0
                     | DMA_SxCR_PFCTRL
                     | DMA_SxCR_MINC
                     | (read ? 0U : DMA_SxCR_DIR_0);
    DMA2_Stream3->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;

    SDIO->ICR = SDIO_ICR_DCRCFAILC
              | SDIO_ICR_DTIMEOUTC
              | SDIO_ICR_TXUNDERRC
              | SDIO_ICR_RXOVERRC
              | SDIO_ICR_DATAENDC;
    SDIO->DTIMER = 0xFFFFFFFFUL;
    SDIO->DLEN = wordCount * sizeof(uint32_t);

    SET_BIT(DMA2_Stream3->CR, DMA_SxCR_EN);
    SDIO->DCTRL = (9UL << SDIO_DCTRL_DBLOCKSIZE_Pos)
                 | SDIO_DCTRL_DMAEN
                 | (read ? SDIO_DCTRL_DTDIR : 0U)
                 | SDIO_DCTRL_DTEN;
}

bool CardLowLevel::waitForBlockTransfer()
{
    constexpr uint32_t dataErrors = SDIO_STA_DCRCFAIL
                                  | SDIO_STA_DTIMEOUT
                                  | SDIO_STA_TXUNDERR
                                  | SDIO_STA_RXOVERR;
    constexpr uint32_t dmaErrors = DMA_LISR_TEIF3
                                 | DMA_LISR_DMEIF3
                                 | DMA_LISR_FEIF3;

    bool success = false;
    for (uint32_t remaining = dataTimeout; remaining > 0U; --remaining)
    {
        if ((SDIO->STA & dataErrors) != 0U || (DMA2->LISR & dmaErrors) != 0U)
            break;

        if ((SDIO->STA & SDIO_STA_DATAEND) != 0U
            && (DMA2->LISR & DMA_LISR_TCIF3) != 0U)
        {
            success = true;
            break;
        }
    }

    resetBlockTransfer();

    if (success && readTransfer_ && readBuffer_ != nullptr)
        std::memcpy(readBuffer_, dmaBuffer_, sdSectorSize);

    readBuffer_ = nullptr;
    return success;
}

void CardLowLevel::resetBlockTransfer()
{
    CLEAR_BIT(SDIO->DCTRL, SDIO_DCTRL_DTEN | SDIO_DCTRL_DMAEN);
    CLEAR_BIT(DMA2_Stream3->CR, DMA_SxCR_EN);
    while (READ_BIT(DMA2_Stream3->CR, DMA_SxCR_EN) != 0U) {}

    DMA2->LIFCR = DMA_LIFCR_CTCIF3
                | DMA_LIFCR_CHTIF3
                | DMA_LIFCR_CTEIF3
                | DMA_LIFCR_CDMEIF3
                | DMA_LIFCR_CFEIF3;
    SDIO->ICR = SDIO_ICR_DCRCFAILC
              | SDIO_ICR_DTIMEOUTC
              | SDIO_ICR_TXUNDERRC
              | SDIO_ICR_RXOVERRC
              | SDIO_ICR_DATAENDC;
}
