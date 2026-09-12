#include "SD_V1.hpp"

SD_V1::SD_V1(CardProtocol* const protocol)
    : Card(protocol)
{
}

CardType SD_V1::getType() const
{
    return CardType::SD_V1;
}

bool SD_V1::initialize()
{
    if (protocol_->writeCommand(CardCommand::CMD2) != Status::COMMAND_RESPONSE_RECEIVED_OK
        || protocol_->writeCommand(CardCommand::CMD3) != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return false;
    relativeCardAddress_ = lowLevel_.getResponse() & relativeCardAddressMask;
    if (relativeCardAddress_ == 0U
        || protocol_->writeCommand(CardCommand::CMD9, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return false;
    const LongResponse csd = lowLevel_.getLongResponse();
    if ((csd.word1 & csdStructureMask) != csdVersion1)
        return false;
    const uint32_t readBlockLength = (csd.word2 >> 16U) & readBlockLengthMask;
    const uint32_t deviceSize = ((csd.word2 & deviceSizeHighMask) << 2U)
                              | ((csd.word3 & deviceSizeLowMask) >> 30U);
    const uint32_t multiplier = (csd.word3 >> 15U) & deviceSizeMultiplierMask;
    capacity_ = (static_cast<uint64_t>(deviceSize) + 1ULL)
              * (1ULL << (multiplier + 2U)) * (1ULL << readBlockLength);
    eraseBlockSize_ = calculateEraseBlockSize(csd);
    if (protocol_->writeCommand(CardCommand::CMD7, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK
        || protocol_->writeCommand(CardCommand::CMD16, sdSectorSize)
            != Status::COMMAND_RESPONSE_RECEIVED_OK
        || protocol_->writeCommand(
            CardApplicationCommand::ACMD6, 2U, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return false;
    lowLevel_.setFourBitBus();
    lowLevel_.setTransferClock();
    return true;
}

uint64_t SD_V1::getCapacity() const
{
    return capacity_;
}

uint64_t SD_V1::getSectorCount() const
{
    return capacity_ / sdSectorSize;
}

uint32_t SD_V1::getEraseBlockSize() const
{
    return eraseBlockSize_;
}

uint32_t SD_V1::getCommandAddress(const uint32_t sector) const
{
    return sector * sdSectorSize;
}
