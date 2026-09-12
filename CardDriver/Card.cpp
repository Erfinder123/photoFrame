#include "Card.hpp"
#include "SD_V1.hpp"
#include "SD_V2.hpp"
#include <new>

Card::Card(CardProtocol* const protocol)
    : protocol_(protocol), lowLevel_(protocol->lowLevel)
{
}

Card::~Card()
{
    if (initialized_)
        lowLevel_.resetBlockTransfer();
    delete protocol_;
    protocol_ = nullptr;
}

Card* Card::detect(
    CardLowLevel& lowLevel,
    CardType& type,
    CardInitError& error,
    const uint32_t commandTimeout)
{
    type = CardType::UNKNOWN;
    error = CardInitError::UNSUPPORTED_CARD;
    lowLevel.prepareForInitialization();

    CardProtocol* const protocol =
        new (std::nothrow) CardProtocol(lowLevel, commandTimeout);
    if (protocol == nullptr)
    {
        error = CardInitError::COMMAND_FAILED;
        return nullptr;
    }
    if (!detectType(*protocol, type, error))
    {
        delete protocol;
        return nullptr;
    }

    Card* card = nullptr;
    if (type == CardType::SD_V1)
        card = new (std::nothrow) SD_V1(protocol);
    else
        card = new (std::nothrow) SD_V2(protocol);
    if (card == nullptr)
    {
        delete protocol;
        error = CardInitError::COMMAND_FAILED;
        return nullptr;
    }
    if (!card->initialize())
    {
        error = CardInitError::COMMAND_FAILED;
        lowLevel.resetBlockTransfer();
        delete card;
        return nullptr;
    }
    card->initialized_ = true;
    error = CardInitError::NONE;
    return card;
}

bool Card::detectType(
    CardProtocol& protocol,
    CardType& type,
    CardInitError& error)
{
    if (protocol.writeCommand(CardCommand::CMD0) != Status::COMMAND_SENT)
    {
        error = CardInitError::COMMAND_FAILED;
        return false;
    }
    constexpr Voltage voltage = Voltage::V2_7_3_6;
    const Status cmd8Status = protocol.writeCommand(
        CardCommand::CMD8, SEND_IF_COND(voltage));
    CapacitySupport capacitySupport = CapacitySupport::STANDARD;
    if (cmd8Status == Status::COMMAND_RESPONSE_RECEIVED_OK)
    {
        if (!CardProtocol::isInterfaceConditionAccepted(
                protocol.lowLevel.getResponse(), voltage))
        {
            error = CardInitError::INVALID_INTERFACE_CONDITION;
            return false;
        }
        type = CardType::SD_V2;
        capacitySupport = CapacitySupport::HIGH;
    }
    else if (cmd8Status == Status::COMMAND_RESPONSE_TIMEOUT)
    {
        if (protocol.writeCommand(CardCommand::CMD55)
                != Status::COMMAND_RESPONSE_RECEIVED_OK
            || !CardProtocol::isApplicationCommand(
                protocol.lowLevel.getResponse()))
        {
            error = CardInitError::UNSUPPORTED_CARD;
            return false;
        }
        type = CardType::SD_V1;
    }
    else
    {
        error = CardInitError::COMMAND_FAILED;
        return false;
    }

    for (uint32_t remaining = protocol.commandTimeout;
         remaining > 0U; --remaining)
    {
        const Status status = protocol.writeCommand(
            CardApplicationCommand::ACMD41,
            CardProtocol::makeOperatingConditions(capacitySupport));
        const bool responseReceived =
            status == Status::COMMAND_RESPONSE_RECEIVED_OK
            || status == Status::COMMAND_RESPONSE_RECEIVED_FAILED;
        if (responseReceived
            && CardProtocol::isCardReady(protocol.lowLevel.getResponse()))
            return true;
    }
    error = CardInitError::POWER_UP_TIMEOUT;
    return false;
}

bool Card::isDetected() const
{
    return lowLevel_.isCardDetected();
}

bool Card::isReady()
{
    if (relativeCardAddress_ == 0U
        || protocol_->writeCommand(CardCommand::CMD13, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return false;
    const uint32_t response = lowLevel_.getResponse();
    return (response & readyForData) != 0U
        && (response & currentStateMask) == transferState;
}

uint32_t Card::calculateEraseBlockSize(const LongResponse& csd)
{
    return (csd.word3 & eraseSingleBlock) != 0U
         ? 1U
         : ((csd.word3 & eraseSectorSizeMask) >> 7U) + 1U;
}

bool Card::readBlocks(
    const uint32_t firstSector,
    uint8_t* const buffer,
    const uint32_t count)
{
    if (buffer == nullptr || count == 0U
        || static_cast<uint64_t>(firstSector) + count > getSectorCount())
        return false;
    uint32_t transferred = 0U;
    while (transferred < count)
    {
        const uint32_t remaining = count - transferred;
        uint32_t transferCount = remaining > maximumBlocksPerTransfer
                               ? maximumBlocksPerTransfer : remaining;
        if ((reinterpret_cast<uintptr_t>(buffer + transferred * sdSectorSize)
             % alignof(uint32_t)) != 0U)
            transferCount = 1U;
        if (!lowLevel_.startReadBlocks(
                buffer + transferred * sdSectorSize, transferCount))
            return false;
        const CardCommand command = transferCount == 1U
                                  ? CardCommand::CMD17 : CardCommand::CMD18;
        if (protocol_->writeCommand(
                command, getCommandAddress(firstSector + transferred))
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        {
            lowLevel_.resetBlockTransfer();
            return false;
        }
        if (!lowLevel_.waitForBlockTransfer())
            return false;
        if (transferCount > 1U
            && protocol_->writeCommand(CardCommand::CMD12)
                != Status::COMMAND_RESPONSE_RECEIVED_OK)
            return false;
        transferred += transferCount;
    }
    return true;
}

bool Card::writeBlocks(
    const uint32_t firstSector,
    const uint8_t* const buffer,
    const uint32_t count)
{
    if (buffer == nullptr || count == 0U
        || static_cast<uint64_t>(firstSector) + count > getSectorCount())
        return false;
    uint32_t transferred = 0U;
    while (transferred < count)
    {
        const uint32_t remaining = count - transferred;
        uint32_t transferCount = remaining > maximumBlocksPerTransfer
                               ? maximumBlocksPerTransfer : remaining;
        if ((reinterpret_cast<uintptr_t>(buffer + transferred * sdSectorSize)
             % alignof(uint32_t)) != 0U)
            transferCount = 1U;
        if (!lowLevel_.startWriteBlocks(
                buffer + transferred * sdSectorSize, transferCount))
            return false;
        const CardCommand command = transferCount == 1U
                                  ? CardCommand::CMD24 : CardCommand::CMD25;
        if (protocol_->writeCommand(
                command, getCommandAddress(firstSector + transferred))
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        {
            lowLevel_.resetBlockTransfer();
            return false;
        }
        if (!lowLevel_.waitForBlockTransfer())
            return false;
        if (transferCount > 1U
            && protocol_->writeCommand(CardCommand::CMD12)
                != Status::COMMAND_RESPONSE_RECEIVED_OK)
            return false;
        bool ready = false;
        for (uint32_t remainingTicks = protocol_->commandTimeout;
             remainingTicks > 0U; --remainingTicks)
        {
            if (isReady())
            {
                ready = true;
                break;
            }
        }
        if (!ready)
            return false;
        transferred += transferCount;
    }
    return true;
}
