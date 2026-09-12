
#ifndef CARDLOWLEVEL_HPP
#define CARDLOWLEVEL_HPP

#include <cstdint>
#include "ffconf.h"


static_assert(FF_MIN_SS == FF_MAX_SS, "The SD driver requires a fixed FatFs sector size");
inline constexpr uint32_t sdSectorSize = FF_MAX_SS;


enum class Status : uint8_t
{
    COMMAND_SENT,
    COMMAND_RESPONSE_RECEIVED_OK,
    COMMAND_RESPONSE_TIMEOUT,
    COMMAND_RESPONSE_RECEIVED_FAILED,
    COMMAND_RESPONSE_FAILED,
    EMPTY_STATUS
};

enum class ResponseType : uint8_t
{
    NONE,
    SHORT,
    LONG
};

struct LongResponse
{
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
    uint32_t word4;
};

class CardLowLevel
{
    public:
        virtual ~CardLowLevel() = default;
        virtual void prepareForInitialization();
        void configureCardDetectInterrupt(uint32_t interruptPriority) const;
        [[nodiscard]] bool isCardDetected() const;
        void clearCardDetectInterrupt() const;
        virtual void sendCommand(uint8_t command, uint32_t argument, ResponseType responseType);
        [[nodiscard]] uint8_t getLastCommand() const;
        [[nodiscard]] virtual uint32_t getResponse() const;
        [[nodiscard]] virtual LongResponse getLongResponse() const;
        virtual Status getStatus();
        virtual void setTransferClock();
        virtual void setFourBitBus();
        [[nodiscard]] virtual bool startReadBlocks(uint8_t* buffer, uint32_t count);
        [[nodiscard]] virtual bool startWriteBlocks(const uint8_t* buffer, uint32_t count);
        [[nodiscard]] virtual bool waitForBlockTransfer();
        virtual void resetBlockTransfer();

    private:
        static constexpr uint32_t blockWordCount = sdSectorSize / sizeof(uint32_t);
        static constexpr uint32_t dataTimeout = 10'000'000U;

        void configureDma(bool read, uint32_t* memory, uint32_t wordCount);

        alignas(uint32_t) uint32_t dmaBuffer_[blockWordCount]{};
        uint8_t* readBuffer_ = nullptr;
        bool readTransfer_ = false;
        uint8_t lastCommand_ = 0xFFU;
};


#endif //CARDLOWLEVEL_HPP
