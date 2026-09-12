#include "CardProtocol.hpp"

CardProtocol::CardProtocol(
    CardLowLevel& lowLevel,
    const uint32_t commandTimeout)
    : lowLevel(lowLevel),
      commandTimeout(commandTimeout)
{
}

Status CardProtocol::writeCommand(CardCommand command, const uint32_t argument)
{
    ResponseType response_type;

    switch (command)
    {
        case CardCommand::CMD0:
            response_type = ResponseType::NONE;
            break;
        case CardCommand::CMD2:
        case CardCommand::CMD9:
        case CardCommand::CMD10:
            response_type = ResponseType::LONG;
            break;
        default:
            response_type = ResponseType::SHORT;
            break;
    }

    lowLevel.sendCommand(static_cast<uint8_t>(command), argument, response_type);
    return waitForCommand();
}

Status CardProtocol::writeCommand(
    CardApplicationCommand command,
    const uint32_t argument,
    const uint32_t relativeCardAddress)
{
    if (writeCommand(CardCommand::CMD55, relativeCardAddress)
        != Status::COMMAND_RESPONSE_RECEIVED_OK)
        return Status::COMMAND_RESPONSE_FAILED;

    if (!isApplicationCommand(lowLevel.getResponse()))
        return Status::COMMAND_RESPONSE_FAILED;

    lowLevel.sendCommand(
        static_cast<uint8_t>(command),
        argument,
        ResponseType::SHORT);

    return waitForCommand();
}

uint32_t CardProtocol::makeOperatingConditions(CapacitySupport capacity_support)
{
    uint32_t argument = OCR_VOLTAGE_WINDOW;

    if (capacity_support == CapacitySupport::HIGH)
        argument |= OCR_HIGH_CAPACITY_SUPPORT;

    return argument;
}

bool CardProtocol::isInterfaceConditionAccepted(uint32_t response, Voltage voltage)
{
    return (response & INTERFACE_CONDITION_MASK) == SEND_IF_COND(voltage);
}

bool CardProtocol::isCardReady(uint32_t response)
{
    return (response & OCR_POWER_UP_COMPLETE) != 0U;
}

bool CardProtocol::isApplicationCommand(uint32_t response)
{
    return (response & R1_APPLICATION_COMMAND) != 0U;
}

Status CardProtocol::waitForCommand()
{
    for (uint32_t remaining = commandTimeout; remaining > 0U; --remaining)
    {
        const Status status = lowLevel.getStatus();
        if (status != Status::EMPTY_STATUS)
            return status;
    }

    return Status::COMMAND_RESPONSE_TIMEOUT;
}
