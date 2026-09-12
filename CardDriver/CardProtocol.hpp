#ifndef CARDPROTOCOL_HPP
#define CARDPROTOCOL_HPP


#include <cstdint>
#include "CardLowLevel.hpp"


enum class CardCommand : uint8_t
{
    CMD0 = 0,
    CMD1 = 1,
    CMD2 = 2,
    CMD3 = 3,
    CMD4 = 4,
    CMD5 = 5,
    CMD6 = 6,
    CMD7 = 7,
    CMD8 = 8,
    CMD9 = 9,
    CMD10 = 10,
    CMD11 = 11,
    CMD12 = 12,
    CMD13 = 13,
    CMD15 = 15,
    CMD16 = 16,
    CMD17 = 17,
    CMD18 = 18,
    CMD19 = 19,
    CMD20 = 20,
    CMD21 = 21,
    CMD23 = 23,
    CMD24 = 24,
    CMD25 = 25,
    CMD27 = 27,
    CMD28 = 28,
    CMD29 = 29,
    CMD30 = 30,
    CMD32 = 32,
    CMD33 = 33,
    CMD34 = 34,
    CMD35 = 35,
    CMD36 = 36,
    CMD37 = 37,
    CMD38 = 38,
    CMD40 = 40,
    CMD42 = 42,
    CMD43 = 43,
    CMD44 = 44,
    CMD45 = 45,
    CMD46 = 46,
    CMD47 = 47,
    CMD48 = 48,
    CMD49 = 49,
    CMD50 = 50,
    CMD52 = 52,
    CMD53 = 53,
    CMD55 = 55,
    CMD56 = 56,
    CMD57 = 57,
    CMD58 = 58,
    CMD59 = 59,
};

enum class CardApplicationCommand : uint8_t
{
    ACMD6 = 6,
    ACMD13 = 13,
    ACMD14 = 14,
    ACMD15 = 15,
    ACMD16 = 16,
    ACMD22 = 22,
    ACMD23 = 23,
    ACMD28 = 28,
    ACMD41 = 41,
    ACMD42 = 42,
    ACMD51 = 51,
};

enum class Voltage : uint8_t
{
    V2_7_3_6 = 0b0001,
    LOW_LEVEL = 0b0010,
};

enum class CapacitySupport : uint8_t
{
    STANDARD,
    HIGH
};

inline constexpr uint32_t GO_IDLE_STATE = 0UL;
inline constexpr uint32_t OCR_VOLTAGE_WINDOW = 0x00FF8000UL;
inline constexpr uint32_t OCR_HIGH_CAPACITY_SUPPORT = 1UL << 30;
inline constexpr uint32_t OCR_POWER_UP_COMPLETE = 1UL << 31;
inline constexpr uint32_t R1_APPLICATION_COMMAND = 1UL << 5;
inline constexpr uint32_t INTERFACE_CONDITION_MASK = 0xFFFUL;

inline constexpr auto SEND_IF_COND = [](Voltage voltage)
{
    constexpr uint32_t check_pattern = 0xAAU;
    return (static_cast<uint32_t>(voltage) << 8) | check_pattern;
};

class CardProtocol
{
    public:
        static constexpr uint32_t defaultTimeout = 1'000'000U;

        explicit CardProtocol(
            CardLowLevel& lowLevel,
            uint32_t commandTimeout = defaultTimeout);

        Status writeCommand(CardCommand command, uint32_t argument = GO_IDLE_STATE);
        Status writeCommand(
            CardApplicationCommand command,
            uint32_t argument = GO_IDLE_STATE,
            uint32_t relativeCardAddress = 0U);

        static uint32_t makeOperatingConditions(CapacitySupport capacity_support);
        static bool isInterfaceConditionAccepted(uint32_t response, Voltage voltage);
        static bool isCardReady(uint32_t response);
        static bool isApplicationCommand(uint32_t response);

    private:
        Status waitForCommand();

    public:
        CardLowLevel& lowLevel;
        const uint32_t commandTimeout;
};


#endif //CARDPROTOCOL_HPP
