#ifndef CARD_HPP
#define CARD_HPP

#include <cstdint>
#include "CardProtocol.hpp"

enum class CardType : uint8_t
{
    UNKNOWN,
    SD_V1,
    SD_V2
};

enum class CardInitError : uint8_t
{
    NONE,
    COMMAND_FAILED,
    INVALID_INTERFACE_CONDITION,
    POWER_UP_TIMEOUT,
    UNSUPPORTED_CARD
};

class Card
{
    public:
        virtual ~Card();
        Card(const Card&) = delete;
        Card& operator=(const Card&) = delete;

        [[nodiscard]] virtual CardType getType() const = 0;
        virtual bool initialize() = 0;
        [[nodiscard]] virtual uint64_t getCapacity() const = 0;
        [[nodiscard]] virtual uint64_t getSectorCount() const = 0;
        [[nodiscard]] virtual uint32_t getEraseBlockSize() const = 0;
        [[nodiscard]] bool isReady();
        [[nodiscard]] bool isDetected() const;
        bool readBlocks(uint32_t firstSector, uint8_t* buffer, uint32_t count);
        bool writeBlocks(uint32_t firstSector, const uint8_t* buffer, uint32_t count);

        static Card* detect(
            CardLowLevel& lowLevel,
            CardType& type,
            CardInitError& error,
            uint32_t commandTimeout = CardProtocol::defaultTimeout);

    protected:
        explicit Card(CardProtocol* protocol);

        static constexpr uint32_t relativeCardAddressMask = 0xFFFF0000UL;
        static constexpr uint32_t readyForData = 1UL << 8U;
        static constexpr uint32_t currentStateMask = 0xFUL << 9U;
        static constexpr uint32_t transferState = 4UL << 9U;
        static constexpr uint32_t maximumBlocksPerTransfer = 511U;
        static constexpr uint32_t eraseSingleBlock = 1UL << 14U;
        static constexpr uint32_t eraseSectorSizeMask = 0x7FUL << 7U;

        CardProtocol* protocol_;
        CardLowLevel& lowLevel_;
        uint32_t relativeCardAddress_ = 0U;
        bool initialized_ = false;

        [[nodiscard]] static uint32_t calculateEraseBlockSize(
            const LongResponse& csd);
        [[nodiscard]] virtual uint32_t getCommandAddress(
            uint32_t sector) const = 0;

    private:
        static bool detectType(
            CardProtocol& protocol,
            CardType& type,
            CardInitError& error);
};

#endif // CARD_HPP
