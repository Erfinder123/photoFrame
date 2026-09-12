#ifndef SD_V2_HPP
#define SD_V2_HPP

#include "Card.hpp"

class SD_V2 final : public Card
{
    public:
        explicit SD_V2(CardProtocol* protocol);
        [[nodiscard]] CardType getType() const override;
        bool initialize() override;
        [[nodiscard]] uint64_t getCapacity() const override;
        [[nodiscard]] uint64_t getSectorCount() const override;
        [[nodiscard]] uint32_t getEraseBlockSize() const override;

    private:
        static constexpr uint32_t csdStructureMask = 0xC0000000UL;
        static constexpr uint32_t csdVersion2 = 0x40000000UL;
        static constexpr uint32_t csdSizeHighMask = 0x3FUL;
        static constexpr uint32_t csdSizeLowMask = 0xFFFF0000UL;
        uint64_t capacity_ = 0U;
        uint32_t eraseBlockSize_ = 0U;
        [[nodiscard]] uint32_t getCommandAddress(uint32_t sector) const override;
};

#endif // SD_V2_HPP
