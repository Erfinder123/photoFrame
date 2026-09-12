#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "Card.hpp"
#include "SystemCard.hpp"
#include "Indicate.hpp"
#include "SD_V1.hpp"
#include "SD_V2.hpp"


// The host tests replace every hardware operation in FakeCardLowLevel.  These
// definitions provide CardLowLevel's vtable without linking the CMSIS-backed
// CardLowLevel.cpp into the native test executable.
void CardLowLevel::prepareForInitialization() {}
bool CardLowLevel::isCardDetected() const { return true; }
void CardLowLevel::sendCommand(uint8_t, uint32_t, ResponseType) {}
uint8_t CardLowLevel::getLastCommand() const { return 0xFFU; }
uint32_t CardLowLevel::getResponse() const { return 0U; }
LongResponse CardLowLevel::getLongResponse() const { return {}; }
Status CardLowLevel::getStatus() { return Status::EMPTY_STATUS; }
void CardLowLevel::setTransferClock() {}
void CardLowLevel::setFourBitBus() {}
bool CardLowLevel::startReadBlocks(uint8_t*, uint32_t) { return false; }
bool CardLowLevel::startWriteBlocks(const uint8_t*, uint32_t) { return false; }
bool CardLowLevel::waitForBlockTransfer() { return false; }
void CardLowLevel::resetBlockTransfer() {}
Indicate::Indicate(IndicateColor normalColor, IndicateColor errorColor, bool)
    : normalColor_(normalColor), errorColor_(errorColor)
{
}
Indicate::Indicate(IndicateColor normalColor, IndicateColor errorColor)
    : Indicate(normalColor, errorColor, false)
{
}
Indicate::~Indicate() = default;
void Indicate::initialization() {}
void Indicate::normalOperation() {}
void Indicate::dataExchange() {}
void Indicate::error() {}
extern "C" FRESULT f_mount(FATFS*, const TCHAR*, BYTE) { return FR_OK; }
extern "C" FRESULT f_opendir(DIR*, const TCHAR*) { return FR_OK; }
extern "C" FRESULT f_closedir(DIR*) { return FR_OK; }
extern "C" FRESULT f_readdir(DIR*, FILINFO* fileInformation)
{
    fileInformation->fname[0] = '\0';
    return FR_OK;
}
extern "C" void disk_set_sector_count_provider(DiskSectorCountProvider) {}
extern "C" void disk_set_block_size_provider(DiskBlockSizeProvider) {}
extern "C" void disk_set_status_provider(DiskStatusProvider) {}
extern "C" void disk_set_read_provider(DiskReadProvider) {}
extern "C" void disk_set_write_provider(DiskWriteProvider) {}


namespace
{
    constexpr uint32_t testTimeout = 2U;
    constexpr uint32_t applicationCommandResponse = R1_APPLICATION_COMMAND;
    constexpr uint32_t readyResponse = OCR_POWER_UP_COMPLETE;
    constexpr uint32_t relativeCardAddress = 0x12340000UL;

    constexpr uint32_t operatingConditions(const CapacitySupport capacitySupport)
    {
        return OCR_VOLTAGE_WINDOW
             | (capacitySupport == CapacitySupport::HIGH
                ? OCR_HIGH_CAPACITY_SUPPORT
                : 0U);
    }

    struct CommandStep
    {
        uint8_t command;
        uint32_t argument;
        ResponseType responseType;
        Status status;
        uint32_t response = 0U;
        LongResponse longResponse{};
    };

    class FakeCardLowLevel final : public CardLowLevel
    {
        public:
            explicit FakeCardLowLevel(std::vector<CommandStep> steps)
                : steps_(std::move(steps))
            {
            }

            void prepareForInitialization() override
            {
            }

            void sendCommand(
                const uint8_t command,
                const uint32_t argument,
                const ResponseType responseType) override
            {
                if (position_ >= steps_.size())
                {
                    valid_ = false;
                    return;
                }

                active_ = &steps_[position_++];
                valid_ = valid_
                      && active_->command == command
                      && active_->argument == argument
                      && active_->responseType == responseType;
            }

            [[nodiscard]] uint32_t getResponse() const override
            {
                return active_ == nullptr ? 0U : active_->response;
            }

            [[nodiscard]] LongResponse getLongResponse() const override
            {
                return active_ == nullptr ? LongResponse{} : active_->longResponse;
            }

            Status getStatus() override
            {
                return active_ == nullptr ? Status::EMPTY_STATUS : active_->status;
            }

            void setTransferClock() override
            {
                valid_ = valid_
                      && active_ != nullptr
                      && active_->command
                         == static_cast<uint8_t>(CardApplicationCommand::ACMD6);
                ++setTransferClockCount;
            }

            void setFourBitBus() override
            {
                valid_ = valid_
                      && active_ != nullptr
                      && active_->command
                         == static_cast<uint8_t>(CardApplicationCommand::ACMD6);
                ++setFourBitBusCount;
            }

            bool startReadBlocks(uint8_t* buffer, const uint32_t count) override
            {
                readBuffer = buffer;
                transferredBlockCount += count;
                ++readBlockCount;
                return startBlockTransferSuccessful;
            }

            bool startWriteBlocks(const uint8_t* buffer, const uint32_t count) override
            {
                writeBuffer = buffer;
                transferredBlockCount += count;
                ++writeBlockCount;
                return startBlockTransferSuccessful;
            }

            [[nodiscard]] bool waitForBlockTransfer() override
            {
                ++waitForBlockTransferCount;
                return blockTransferSuccessful;
            }

            void resetBlockTransfer() override
            {
                ++resetBlockTransferCount;
            }

            [[nodiscard]] bool complete() const
            {
                return valid_ && position_ == steps_.size();
            }

            uint32_t setTransferClockCount = 0U;
            uint32_t setFourBitBusCount = 0U;
            uint8_t* readBuffer = nullptr;
            const uint8_t* writeBuffer = nullptr;
            uint32_t readBlockCount = 0U;
            uint32_t writeBlockCount = 0U;
            uint32_t waitForBlockTransferCount = 0U;
            uint32_t resetBlockTransferCount = 0U;
            uint32_t transferredBlockCount = 0U;
            bool startBlockTransferSuccessful = true;
            bool blockTransferSuccessful = true;

        private:
            std::vector<CommandStep> steps_;
            const CommandStep* active_ = nullptr;
            std::size_t position_ = 0U;
            bool valid_ = true;
    };

    CommandStep command(
        const CardCommand value,
        const Status status,
        const uint32_t response = 0U,
        const uint32_t argument = 0U)
    {
        ResponseType responseType = ResponseType::SHORT;
        if (value == CardCommand::CMD0)
            responseType = ResponseType::NONE;
        else if (value == CardCommand::CMD2 || value == CardCommand::CMD9)
            responseType = ResponseType::LONG;

        return CommandStep{
            static_cast<uint8_t>(value), argument, responseType, status, response, {}};
    }

    void addApplicationCommand(
        std::vector<CommandStep>& steps,
        const CapacitySupport capacitySupport,
        const Status status,
        const uint32_t response)
    {
        steps.push_back(command(
            CardCommand::CMD55,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            applicationCommandResponse));
        steps.push_back(CommandStep{
            static_cast<uint8_t>(CardApplicationCommand::ACMD41),
            operatingConditions(capacitySupport),
            ResponseType::SHORT,
            status,
            response,
            {}});
    }

    LongResponse makeCsd(const uint32_t deviceSize)
    {
        return LongResponse{
            0x40000000UL,
            static_cast<uint32_t>((deviceSize >> 16U) & 0x3FUL),
            static_cast<uint32_t>(
                ((deviceSize & 0xFFFFUL) << 16U) | (1UL << 14U)),
            0U
        };
    }

    void addSdV2Completion(std::vector<CommandStep>& steps, const uint32_t deviceSize)
    {
        steps.push_back(command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK));
        steps.push_back(command(
            CardCommand::CMD3,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            relativeCardAddress));

        CommandStep csd = command(
            CardCommand::CMD9,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress);
        csd.longResponse = makeCsd(deviceSize);
        steps.push_back(csd);
        steps.push_back(command(
            CardCommand::CMD7,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress));
        steps.push_back(command(
            CardCommand::CMD55,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            applicationCommandResponse,
            relativeCardAddress));
        steps.push_back(CommandStep{
            static_cast<uint8_t>(CardApplicationCommand::ACMD6),
            2U,
            ResponseType::SHORT,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            {}});
    }

    LongResponse makeCsdV1(
        const uint32_t readBlockLength,
        const uint32_t deviceSize,
        const uint32_t deviceSizeMultiplier)
    {
        return LongResponse{
            0U,
            (readBlockLength << 16U) | ((deviceSize >> 2U) & 0x3FFU),
            static_cast<uint32_t>(((deviceSize & 0x3U) << 30U)
                | (deviceSizeMultiplier << 15U)
                | (1UL << 14U)),
            0U
        };
    }

    void addSdV1Completion(
        std::vector<CommandStep>& steps,
        const uint32_t readBlockLength,
        const uint32_t deviceSize,
        const uint32_t deviceSizeMultiplier)
    {
        steps.push_back(command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK));
        steps.push_back(command(
            CardCommand::CMD3,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            relativeCardAddress));

        CommandStep csd = command(
            CardCommand::CMD9,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress);
        csd.longResponse = makeCsdV1(
            readBlockLength,
            deviceSize,
            deviceSizeMultiplier);
        steps.push_back(csd);
        steps.push_back(command(
            CardCommand::CMD7,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress));
        steps.push_back(command(
            CardCommand::CMD16,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            sdSectorSize));
        steps.push_back(command(
            CardCommand::CMD55,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            applicationCommandResponse,
            relativeCardAddress));
        steps.push_back(CommandStep{
            static_cast<uint8_t>(CardApplicationCommand::ACMD6),
            2U,
            ResponseType::SHORT,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            {}});
    }

    bool sdV2IsInitializedAndCapacityIsCalculated()
    {
        constexpr uint32_t deviceSize = 0x12345U;
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                SEND_IF_COND(Voltage::V2_7_3_6),
                SEND_IF_COND(Voltage::V2_7_3_6))
        };
        addApplicationCommand(
            steps, CapacitySupport::HIGH, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U);
        addApplicationCommand(
            steps, CapacitySupport::HIGH, Status::COMMAND_RESPONSE_RECEIVED_OK, readyResponse);
        addSdV2Completion(steps, deviceSize);

        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        constexpr uint64_t expectedCapacity =
            (static_cast<uint64_t>(deviceSize) + 1ULL) * sdSectorSize * 1024ULL;
        constexpr uint64_t expectedSectorCount = expectedCapacity / sdSectorSize;
        return result
            && result.type == CardType::SD_V2
            && result.card != nullptr
            && result.capacity == expectedCapacity
            && result.sectorCount == expectedSectorCount
            && result.card->getEraseBlockSize() == 1U
            && lowLevel.setTransferClockCount == 1U
            && lowLevel.setFourBitBusCount == 1U
            && lowLevel.resetBlockTransferCount == 0U
            && lowLevel.complete();
    }

    bool invalidInterfaceConditionIsRejected()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                0x1ABU,
                SEND_IF_COND(Voltage::V2_7_3_6))
        };
        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::INVALID_INTERFACE_CONDITION
            && result.type == CardType::UNKNOWN
            && lowLevel.complete();
    }

    bool sdV2PowerUpTimeoutIsReported()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                SEND_IF_COND(Voltage::V2_7_3_6),
                SEND_IF_COND(Voltage::V2_7_3_6))
        };
        addApplicationCommand(
            steps, CapacitySupport::HIGH, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U);
        addApplicationCommand(
            steps, CapacitySupport::HIGH, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U);

        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::POWER_UP_TIMEOUT
            && result.type == CardType::SD_V2
            && lowLevel.complete();
    }

    bool sdV1IsDetected()
    {
        constexpr uint32_t readBlockLength = 9U;
        constexpr uint32_t deviceSize = 0xABCU;
        constexpr uint32_t deviceSizeMultiplier = 3U;
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_TIMEOUT,
                0U,
                SEND_IF_COND(Voltage::V2_7_3_6)),
            command(
                CardCommand::CMD55,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                applicationCommandResponse)
        };
        addApplicationCommand(
            steps, CapacitySupport::STANDARD, Status::COMMAND_RESPONSE_RECEIVED_OK, readyResponse);
        addSdV1Completion(
            steps,
            readBlockLength,
            deviceSize,
            deviceSizeMultiplier);

        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        constexpr uint64_t expectedCapacity =
            (static_cast<uint64_t>(deviceSize) + 1ULL)
            * (1ULL << (deviceSizeMultiplier + 2U))
            * (1ULL << readBlockLength);
        return result
            && result.type == CardType::SD_V1
            && result.card != nullptr
            && result.capacity == expectedCapacity
            && result.sectorCount == expectedCapacity / sdSectorSize
            && result.card->getEraseBlockSize() == 1U
            && lowLevel.setTransferClockCount == 1U
            && lowLevel.setFourBitBusCount == 1U
            && lowLevel.resetBlockTransferCount == 0U
            && lowLevel.complete();
    }

    bool sdV1PowerUpTimeoutIsReported()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_TIMEOUT,
                0U,
                SEND_IF_COND(Voltage::V2_7_3_6)),
            command(
                CardCommand::CMD55,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                applicationCommandResponse)
        };
        addApplicationCommand(
            steps, CapacitySupport::STANDARD, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U);
        addApplicationCommand(
            steps, CapacitySupport::STANDARD, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U);

        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::POWER_UP_TIMEOUT
            && result.type == CardType::SD_V1
            && lowLevel.complete();
    }

    bool nonSdCardIsRejected()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_TIMEOUT,
                0U,
                SEND_IF_COND(Voltage::V2_7_3_6)),
            command(CardCommand::CMD55, Status::COMMAND_RESPONSE_TIMEOUT)
        };

        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::UNSUPPORTED_CARD
            && result.type == CardType::UNKNOWN
            && lowLevel.complete();
    }

    bool initialCommandFailureIsReported()
    {
        FakeCardLowLevel lowLevel({
            command(CardCommand::CMD0, Status::COMMAND_RESPONSE_TIMEOUT)});
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::COMMAND_FAILED
            && result.type == CardType::UNKNOWN
            && lowLevel.complete();
    }

    bool unexpectedCmd8FailureIsReported()
    {
        FakeCardLowLevel lowLevel({
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_RECEIVED_FAILED,
                0U,
                SEND_IF_COND(Voltage::V2_7_3_6))});
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);
        const CardDetectionResult result = card.detect(lowLevel, testTimeout);

        return result.error == CardInitError::COMMAND_FAILED
            && result.type == CardType::UNKNOWN
            && lowLevel.complete();
    }

    [[maybe_unused]] bool sdV2RejectsMissingRca()
    {
        FakeCardLowLevel lowLevel({
            command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
            command(CardCommand::CMD3, Status::COMMAND_RESPONSE_RECEIVED_OK, 0U)});
        SD_V2 card(new CardProtocol(lowLevel, testTimeout));

        return !card.initialize()
            && card.getCapacity() == 0U
            && card.getSectorCount() == 0U
            && lowLevel.setTransferClockCount == 0U
            && lowLevel.complete();
    }

    [[maybe_unused]] bool sdV1RejectsWrongCsdVersion()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
            command(CardCommand::CMD3, Status::COMMAND_RESPONSE_RECEIVED_OK, relativeCardAddress)
        };
        CommandStep csd = command(
            CardCommand::CMD9,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress);
        csd.longResponse = makeCsd(1U);
        steps.push_back(csd);

        FakeCardLowLevel lowLevel(std::move(steps));
        SD_V1 card(new CardProtocol(lowLevel, testTimeout));
        return !card.initialize()
            && card.getCapacity() == 0U
            && card.getSectorCount() == 0U
            && lowLevel.setTransferClockCount == 0U
            && lowLevel.complete();
    }

    [[maybe_unused]] bool sdV2RejectsWrongCsdVersion()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
            command(CardCommand::CMD3, Status::COMMAND_RESPONSE_RECEIVED_OK, relativeCardAddress)
        };
        CommandStep csd = command(
            CardCommand::CMD9,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress);
        csd.longResponse = LongResponse{};
        steps.push_back(csd);

        FakeCardLowLevel lowLevel(std::move(steps));
        SD_V2 card(new CardProtocol(lowLevel, testTimeout));
        return !card.initialize()
            && card.getCapacity() == 0U
            && card.getSectorCount() == 0U
            && lowLevel.complete();
    }

    [[maybe_unused]] bool sdV2CommandFailuresAreRejected()
    {
        const auto fails = [](std::vector<CommandStep> steps)
        {
            FakeCardLowLevel lowLevel(std::move(steps));
            SD_V2 card(new CardProtocol(lowLevel, testTimeout));
            return !card.initialize() && lowLevel.complete();
        };

        std::vector<CommandStep> cmd9Steps{
            command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
            command(CardCommand::CMD3, Status::COMMAND_RESPONSE_RECEIVED_OK, relativeCardAddress),
            command(CardCommand::CMD9, Status::COMMAND_RESPONSE_TIMEOUT, 0U, relativeCardAddress)
        };

        std::vector<CommandStep> cmd7Steps{
            command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
            command(CardCommand::CMD3, Status::COMMAND_RESPONSE_RECEIVED_OK, relativeCardAddress)
        };
        CommandStep csd = command(
            CardCommand::CMD9,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            relativeCardAddress);
        csd.longResponse = makeCsd(1U);
        cmd7Steps.push_back(csd);
        cmd7Steps.push_back(command(
            CardCommand::CMD7,
            Status::COMMAND_RESPONSE_TIMEOUT,
            0U,
            relativeCardAddress));

        return fails({command(CardCommand::CMD2, Status::COMMAND_RESPONSE_TIMEOUT)})
            && fails({
                command(CardCommand::CMD2, Status::COMMAND_RESPONSE_RECEIVED_OK),
                command(CardCommand::CMD3, Status::COMMAND_RESPONSE_TIMEOUT)})
            && fails(std::move(cmd9Steps))
            && fails(std::move(cmd7Steps));
    }

    [[maybe_unused]] bool cardStatusIsReadWithCmd13()
    {
        constexpr uint32_t transferState = 4UL << 9U;
        constexpr uint32_t readyForData = 1UL << 8U;

        const auto checkStatus = [](const Status commandStatus, const uint32_t response)
        {
            std::vector<CommandStep> steps;
            addSdV2Completion(steps, 1U);
            steps.push_back(command(
                CardCommand::CMD13,
                commandStatus,
                response,
                relativeCardAddress));

            FakeCardLowLevel lowLevel(std::move(steps));
            SD_V2 card(new CardProtocol(lowLevel, testTimeout));
            const bool initialized = card.initialize();
            const bool ready = card.isReady();
            return initialized && lowLevel.complete() && ready;
        };

        const auto checkNotReady = [](const Status commandStatus, const uint32_t response)
        {
            std::vector<CommandStep> steps;
            addSdV2Completion(steps, 1U);
            steps.push_back(command(
                CardCommand::CMD13,
                commandStatus,
                response,
                relativeCardAddress));

            FakeCardLowLevel lowLevel(std::move(steps));
            SD_V2 card(new CardProtocol(lowLevel, testTimeout));
            const bool initialized = card.initialize();
            const bool ready = card.isReady();
            return initialized && lowLevel.complete() && !ready;
        };

        return checkStatus(
                   Status::COMMAND_RESPONSE_RECEIVED_OK,
                   readyForData | transferState)
            && checkNotReady(
                   Status::COMMAND_RESPONSE_RECEIVED_OK,
                   7UL << 9U)
            && checkNotReady(Status::COMMAND_RESPONSE_TIMEOUT, 0U);
    }

    [[maybe_unused]] bool sdV2ReadsAndWritesBlocks()
    {
        constexpr uint32_t transferState = 4UL << 9U;
        constexpr uint32_t readyForData = 1UL << 8U;
        std::vector<CommandStep> steps;
        addSdV2Completion(steps, 1U);
        steps.push_back(command(
            CardCommand::CMD18,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            3U));
        steps.push_back(command(
            CardCommand::CMD12,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            0U));
        steps.push_back(command(
            CardCommand::CMD25,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            5U));
        steps.push_back(command(
            CardCommand::CMD12,
            Status::COMMAND_RESPONSE_RECEIVED_OK));
        steps.push_back(command(
            CardCommand::CMD13,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            readyForData | transferState,
            relativeCardAddress));

        FakeCardLowLevel lowLevel(std::move(steps));
        SD_V2 card(new CardProtocol(lowLevel, testTimeout));
        alignas(uint32_t) uint8_t buffer[1024]{};

        return card.initialize()
            && card.readBlocks(3U, buffer, 2U)
            && card.writeBlocks(5U, buffer, 2U)
            && lowLevel.readBlockCount == 1U
            && lowLevel.writeBlockCount == 1U
            && lowLevel.waitForBlockTransferCount == 2U
            && lowLevel.transferredBlockCount == 4U
            && lowLevel.resetBlockTransferCount == 0U
            && lowLevel.complete();
    }

    [[maybe_unused]] bool sdV1UsesByteAddressForBlocks()
    {
        std::vector<CommandStep> steps;
        addSdV1Completion(steps, 9U, 0x3FFU, 3U);
        steps.push_back(command(
            CardCommand::CMD17,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            0U,
            7U * sdSectorSize));

        FakeCardLowLevel lowLevel(std::move(steps));
        SD_V1 card(new CardProtocol(lowLevel, testTimeout));
        uint8_t buffer[512]{};

        return card.initialize()
            && card.readBlocks(7U, buffer, 1U)
            && lowLevel.readBlockCount == 1U
            && lowLevel.waitForBlockTransferCount == 1U
            && lowLevel.complete();
    }

    [[maybe_unused]] bool systemCardNavigatesDirectories()
    {
        std::vector<CommandStep> steps{
            command(CardCommand::CMD0, Status::COMMAND_SENT),
            command(
                CardCommand::CMD8,
                Status::COMMAND_RESPONSE_RECEIVED_OK,
                SEND_IF_COND(Voltage::V2_7_3_6),
                SEND_IF_COND(Voltage::V2_7_3_6))
        };
        addApplicationCommand(
            steps,
            CapacitySupport::HIGH,
            Status::COMMAND_RESPONSE_RECEIVED_OK,
            readyResponse);
        addSdV2Completion(steps, 1U);
        FakeCardLowLevel lowLevel(std::move(steps));
        SystemCard card(IndicateColor::GREEN, IndicateColor::ORANGE);

        return card.initialize(lowLevel, testTimeout)
            && std::strcmp(card.getCurrentDirectoryPath(), "0:/") == 0
            && card.enterDirectory("Photos") == FR_OK
            && std::strcmp(card.getCurrentDirectoryPath(), "0:/Photos") == 0
            && card.enterDirectory("2026") == FR_OK
            && std::strcmp(card.getCurrentDirectoryPath(), "0:/Photos/2026") == 0
            && card.leaveDirectory() == FR_OK
            && std::strcmp(card.getCurrentDirectoryPath(), "0:/Photos") == 0
            && card.leaveDirectory() == FR_OK
            && card.leaveDirectory() == FR_OK
            && std::strcmp(card.getCurrentDirectoryPath(), "0:/") == 0
            && card.enterDirectory("../invalid") == FR_INVALID_NAME
            && lowLevel.complete();
    }

}

int main()
{
    assert(sdV2IsInitializedAndCapacityIsCalculated());
    assert(invalidInterfaceConditionIsRejected());
    assert(sdV2PowerUpTimeoutIsReported());
    assert(sdV1IsDetected());
    assert(sdV1PowerUpTimeoutIsReported());
    assert(nonSdCardIsRejected());
    assert(initialCommandFailureIsReported());
    assert(unexpectedCmd8FailureIsReported());
    assert(sdV1RejectsWrongCsdVersion());
    assert(sdV2RejectsMissingRca());
    assert(sdV2RejectsWrongCsdVersion());
    assert(sdV2CommandFailuresAreRejected());
    assert(cardStatusIsReadWithCmd13());
    assert(sdV2ReadsAndWritesBlocks());
    assert(sdV1UsesByteAddressForBlocks());
    assert(systemCardNavigatesDirectories());
    return 0;
}
