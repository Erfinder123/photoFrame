#ifndef SD_V1_HPP
#define SD_V1_HPP

#include "Card.hpp"

class SD_V1 final : public Card
{
    public:
        explicit SD_V1(CardProtocol* protocol);
        [[nodiscard]] CardType getType() const override;
        bool initialize() override;
        [[nodiscard]] uint64_t getCapacity() const override;
        [[nodiscard]] uint64_t getSectorCount() const override;
        [[nodiscard]] uint32_t getEraseBlockSize() const override;

    private:
        static constexpr uint32_t csdStructureMask = 0xC0000000UL;
        static constexpr uint32_t csdVersion1 = 0U;
        static constexpr uint32_t readBlockLengthMask = 0xFU;
        static constexpr uint32_t deviceSizeHighMask = 0x3FFU;
        static constexpr uint32_t deviceSizeLowMask = 0xC0000000UL;
        static constexpr uint32_t deviceSizeMultiplierMask = 0x7U;
        uint64_t capacity_ = 0U;
        uint32_t eraseBlockSize_ = 0U;
        [[nodiscard]] uint32_t getCommandAddress(uint32_t sector) const override;
};

#endif // SD_V1_HPP
