#include "SD_V2.hpp"

SD_V2::SD_V2(CardProtocol* const protocol)
    : Card(protocol)
{
}

CardType SD_V2::getType() const
{
    return CardType::SD_V2;
}

bool SD_V2::initialize()
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
    if ((csd.word1 & csdStructureMask) != csdVersion2)
        return false;
    const uint32_t deviceSize = ((csd.word2 & csdSizeHighMask) << 16U)
                              | ((csd.word3 & csdSizeLowMask) >> 16U);
    capacity_ = (static_cast<uint64_t>(deviceSize) + 1ULL)
              * static_cast<uint64_t>(sdSectorSize) * 1024ULL;
    eraseBlockSize_ = calculateEraseBlockSize(csd);
    if (protocol_->writeCommand(CardCommand::CMD7, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK
        || protocol_->writeCommand(
            CardApplicationCommand::ACMD6, 2U, relativeCardAddress_)
            != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return false;
    lowLevel_.setFourBitBus();
    lowLevel_.setTransferClock();
    return true;
}

uint64_t SD_V2::getCapacity() const
{
    return capacity_;
}

uint64_t SD_V2::getSectorCount() const
{
    return capacity_ / sdSectorSize;
}

uint32_t SD_V2::getEraseBlockSize() const
{
    return eraseBlockSize_;
}

uint32_t SD_V2::getCommandAddress(const uint32_t sector) const
{
    return sector;
}
