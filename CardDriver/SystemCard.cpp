#include "SystemCard.hpp"

#include <cstring>

SystemCard* SystemCard::active_ = nullptr;

SystemCard::SystemCard(
    const IndicateColor normalColor,
    const IndicateColor errorColor)
    : indicate_(normalColor, errorColor)
{
}

SystemCard::~SystemCard()
{
    disconnectFileSystem();
    delete implementation_;
    implementation_ = nullptr;
}

CardDetectionResult SystemCard::detect(
    CardLowLevel& lowLevel,
    const uint32_t commandTimeout)
{
    const bool initialized = initialize(lowLevel, commandTimeout);
    return {getType(), getInitializationError(), getCapacity(),
            getSectorCount(), initialized ? this : nullptr};
}

bool SystemCard::initialize(
    CardLowLevel& lowLevel,
    const uint32_t commandTimeout)
{
    disconnectFileSystem();
    std::memcpy(currentDirectoryPath_, "0:/", sizeof("0:/"));
    delete implementation_;
    type_ = CardType::UNKNOWN;
    initializationError_ = CardInitError::UNSUPPORTED_CARD;
    failedCommand_ = 0xFFU;
    indicate_.initialization();
    implementation_ = Card::detect(
        lowLevel, type_, initializationError_, commandTimeout);
    if (implementation_ == nullptr)
    {
        failedCommand_ = lowLevel.getLastCommand();
        indicate_.error();
        return false;
    }
    connectFileSystem();
    fileSystemResult_ = f_mount(&fileSystem_, "0:", 1U);
    updateFileSystemIndication(fileSystemResult_);
    return fileSystemResult_ == FR_OK;
}

CardType SystemCard::getType() const
{
    return type_;
}

CardInitError SystemCard::getInitializationError() const
{
    return initializationError_;
}

uint8_t SystemCard::getFailedCommand() const
{
    return failedCommand_;
}

uint64_t SystemCard::getCapacity() const
{
    return implementation_ ? implementation_->getCapacity() : 0U;
}

uint64_t SystemCard::getSectorCount() const
{
    return implementation_ ? implementation_->getSectorCount() : 0U;
}

uint32_t SystemCard::getEraseBlockSize() const
{
    return implementation_ ? implementation_->getEraseBlockSize() : 0U;
}

FRESULT SystemCard::getFileSystemResult() const
{
    return fileSystemResult_;
}

const char* SystemCard::getCurrentDirectoryPath() const
{
    return currentDirectoryPath_;
}

FRESULT SystemCard::openCurrentDirectory(DIR& directory)
{
    if (fileSystemResult_ != FR_OK)
    {
        indicate_.error();
        return fileSystemResult_;
    }

    const FRESULT result = f_opendir(&directory, currentDirectoryPath_);
    updateFileSystemIndication(result);
    return result;
}

FRESULT SystemCard::readDirectoryEntry(
    DIR& directory,
    FILINFO& fileInformation)
{
    const FRESULT result = f_readdir(&directory, &fileInformation);
    updateFileSystemIndication(result);
    return result;
}

FRESULT SystemCard::closeDirectory(DIR& directory)
{
    const FRESULT result = f_closedir(&directory);
    updateFileSystemIndication(result);
    return result;
}

FRESULT SystemCard::enterDirectory(const char* const directoryName)
{
    if (fileSystemResult_ != FR_OK)
        return fileSystemResult_;
    if (directoryName == nullptr || directoryName[0] == '\0'
        || std::strcmp(directoryName, ".") == 0
        || std::strcmp(directoryName, "..") == 0
        || std::strchr(directoryName, '/') != nullptr
        || std::strchr(directoryName, '\\') != nullptr
        || std::strchr(directoryName, ':') != nullptr)
        return FR_INVALID_NAME;

    const uint32_t pathLength = std::strlen(currentDirectoryPath_);
    const uint32_t nameLength = std::strlen(directoryName);
    const bool separatorRequired = currentDirectoryPath_[pathLength - 1U] != '/';
    const uint32_t candidateLength = pathLength
                                   + (separatorRequired ? 1U : 0U)
                                   + nameLength;
    if (candidateLength >= maximumPathLength)
        return FR_INVALID_NAME;

    if (separatorRequired)
        currentDirectoryPath_[pathLength] = '/';
    std::memcpy(
        currentDirectoryPath_ + pathLength + (separatorRequired ? 1U : 0U),
        directoryName,
        nameLength + 1U);

    DIR directory;
    const FRESULT result = f_opendir(&directory, currentDirectoryPath_);
    if (result == FR_OK)
    {
        const FRESULT closeResult = f_closedir(&directory);
        updateFileSystemIndication(closeResult);
        return closeResult;
    }

    currentDirectoryPath_[pathLength] = '\0';
    updateFileSystemIndication(result);
    return result;
}

FRESULT SystemCard::leaveDirectory()
{
    if (fileSystemResult_ != FR_OK)
        return fileSystemResult_;

    const uint32_t pathLength = std::strlen(currentDirectoryPath_);
    if (pathLength <= 3U)
        return FR_OK;

    char* separator = std::strrchr(currentDirectoryPath_, '/');
    if (separator == currentDirectoryPath_ + 2U)
        separator[1] = '\0';
    else
        *separator = '\0';
    return FR_OK;
}

bool SystemCard::isReady()
{
    return implementation_ && implementation_->isReady();
}

void SystemCard::finishDataExchange()
{
    if (implementation_ != nullptr)
        indicate_.normalOperation();
}

bool SystemCard::readBlocks(const uint32_t sector, uint8_t* const buffer, const uint32_t count)
{
    if (!implementation_) return false;
    indicate_.dataExchange();
    const bool ok = implementation_->readBlocks(sector, buffer, count);
    if (!ok)
        indicate_.error();
    return ok;
}

bool SystemCard::writeBlocks(const uint32_t sector, const uint8_t* const buffer, const uint32_t count)
{
    if (!implementation_) return false;
    indicate_.dataExchange();
    const bool ok = implementation_->writeBlocks(sector, buffer, count);
    if (!ok)
        indicate_.error();
    return ok;
}

void SystemCard::connectFileSystem()
{
    active_ = this;
    disk_set_sector_count_provider(provideSectorCount);
    disk_set_block_size_provider(provideBlockSize);
    disk_set_status_provider(provideStatus);
    disk_set_read_provider(provideRead);
    disk_set_write_provider(provideWrite);
}

void SystemCard::disconnectFileSystem()
{
    if (active_ == this)
    {
        static_cast<void>(f_unmount("0:"));
        disk_set_sector_count_provider(nullptr);
        disk_set_block_size_provider(nullptr);
        disk_set_status_provider(nullptr);
        disk_set_read_provider(nullptr);
        disk_set_write_provider(nullptr);
        active_ = nullptr;
    }
    fileSystemResult_ = FR_NOT_READY;
}

void SystemCard::updateFileSystemIndication(const FRESULT result)
{
    if (result == FR_OK)
        indicate_.normalOperation();
    else
        indicate_.error();
}

LBA_t SystemCard::provideSectorCount()
{
    return active_ ? active_->getSectorCount() : 0U;
}

DWORD SystemCard::provideBlockSize()
{
    return active_ ? active_->getEraseBlockSize() : 0U;
}
DSTATUS SystemCard::provideStatus()
{
    return active_ && active_->implementation_
        && active_->implementation_->isDetected()
         ? 0U : STA_NOINIT;
}
DRESULT SystemCard::provideRead(BYTE* buffer, const LBA_t sector, const UINT count)
{
    if (!active_ || !active_->implementation_
        || !active_->implementation_->isDetected()) return RES_NOTRDY;
    return active_->readBlocks(sector, buffer, count) ? RES_OK : RES_ERROR;
}
DRESULT SystemCard::provideWrite(const BYTE* buffer, const LBA_t sector, const UINT count)
{
    if (!active_ || !active_->implementation_
        || !active_->implementation_->isDetected()) return RES_NOTRDY;
    return active_->writeBlocks(sector, buffer, count) ? RES_OK : RES_ERROR;
}
