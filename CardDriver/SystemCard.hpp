#ifndef SYSTEMCARD_HPP
#define SYSTEMCARD_HPP


#include <cstdint>
#include "Card.hpp"
#include "Indicate.hpp"
#include "ff.h"
#include "diskio.h"


class SystemCard;

struct CardDetectionResult
{
    CardType type = CardType::UNKNOWN;
    CardInitError error = CardInitError::UNSUPPORTED_CARD;
    uint64_t capacity = 0U;
    uint64_t sectorCount = 0U;
    SystemCard* card = nullptr;
    explicit operator bool() const { return error == CardInitError::NONE; }
};

class SystemCard
{
    public:
        SystemCard(
            IndicateColor normalColor,
            IndicateColor errorColor);
        ~SystemCard();

        SystemCard(const SystemCard&) = delete;
        SystemCard& operator=(const SystemCard&) = delete;

        bool initialize(
            CardLowLevel& lowLevel,
            uint32_t commandTimeout = CardProtocol::defaultTimeout);
        CardDetectionResult detect(
            CardLowLevel& lowLevel,
            uint32_t commandTimeout = CardProtocol::defaultTimeout);
        [[nodiscard]] CardType getType() const;
        [[nodiscard]] CardInitError getInitializationError() const;
        [[nodiscard]] uint8_t getFailedCommand() const;
        [[nodiscard]] uint64_t getCapacity() const;
        [[nodiscard]] uint64_t getSectorCount() const;
        [[nodiscard]] uint32_t getEraseBlockSize() const;
        [[nodiscard]] FRESULT getFileSystemResult() const;
        [[nodiscard]] const char* getCurrentDirectoryPath() const;
        FRESULT openCurrentDirectory(DIR& directory);
        FRESULT readDirectoryEntry(DIR& directory, FILINFO& fileInformation);
        FRESULT closeDirectory(DIR& directory);
        FRESULT enterDirectory(const char* directoryName);
        FRESULT leaveDirectory();
        [[nodiscard]] bool isReady();
        bool readBlocks(uint32_t firstSector, uint8_t* buffer, uint32_t count);
        bool writeBlocks(uint32_t firstSector, const uint8_t* buffer, uint32_t count);

    private:
        void connectFileSystem();
        void disconnectFileSystem();
        void updateFileSystemIndication(FRESULT result);
        static LBA_t provideSectorCount();
        static DWORD provideBlockSize();
        static DSTATUS provideStatus();
        static DRESULT provideRead(BYTE* buffer, LBA_t sector, UINT count);
        static DRESULT provideWrite(const BYTE* buffer, LBA_t sector, UINT count);

        static constexpr uint32_t maximumPathLength = FF_MAX_LFN * 4U + 4U;

        static SystemCard* active_;
        Indicate indicate_;
        Card* implementation_ = nullptr;
        FATFS fileSystem_{};
        FRESULT fileSystemResult_ = FR_NOT_READY;
        CardType type_ = CardType::UNKNOWN;
        CardInitError initializationError_ = CardInitError::UNSUPPORTED_CARD;
        uint8_t failedCommand_ = 0xFFU;
        char currentDirectoryPath_[maximumPathLength] = "0:/";
};


#endif //SYSTEMCARD_HPP
