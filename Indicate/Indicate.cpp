#include "Indicate.hpp"

Indicate::Indicate(
    const IndicateColor normalColor,
    const IndicateColor errorColor)
    : Indicate(normalColor, errorColor, true)
{
}

Indicate::Indicate(
    const IndicateColor normalColor,
    const IndicateColor errorColor,
    const bool configureHardware)
    : normalColor_(normalColor),
      errorColor_(errorColor),
      hardwareEnabled_(configureHardware)
{
    if (!hardwareEnabled_)
        return;

    lowLevel_.configure(normalColor_);
    lowLevel_.configure(errorColor_);
    turnOff();
}

Indicate::~Indicate()
{
    if (hardwareEnabled_)
        turnOff();
}

void Indicate::initialization()
{
    if (!hardwareEnabled_)
        return;

    setNormal(false);
    normalEnabled_ = false;

    const uint32_t currentTick = lowLevel_.getTick();
    if (mode_ != Mode::initialization)
    {
        errorEnabled_ = true;
        setError(true);
        lastInitializationToggleTick_ = currentTick;
        mode_ = Mode::initialization;
    }
    else if (currentTick - lastInitializationToggleTick_
             >= initializationBlinkInterval)
    {
        errorEnabled_ = !errorEnabled_;
        setError(errorEnabled_);
        lastInitializationToggleTick_ = currentTick;
    }
}

void Indicate::normalOperation()
{
    if (!hardwareEnabled_)
        return;

    setError(false);
    setNormal(true);
    errorEnabled_ = false;
    normalEnabled_ = true;
    mode_ = Mode::normalOperation;
}

void Indicate::dataExchange()
{
    if (!hardwareEnabled_)
        return;

    setError(false);
    errorEnabled_ = false;

    const uint32_t currentTick = lowLevel_.getTick();
    if (mode_ != Mode::dataExchange)
    {
        normalEnabled_ = false;
        setNormal(false);
        lastExchangeToggleTick_ = currentTick;
        mode_ = Mode::dataExchange;
    }
    else if (currentTick - lastExchangeToggleTick_ >= exchangeBlinkInterval)
    {
        normalEnabled_ = !normalEnabled_;
        setNormal(normalEnabled_);
        lastExchangeToggleTick_ = currentTick;
    }
}

void Indicate::error()
{
    if (!hardwareEnabled_)
        return;

    setNormal(false);
    setError(true);
    normalEnabled_ = false;
    errorEnabled_ = true;
    mode_ = Mode::error;
}

void Indicate::setNormal(const bool enabled)
{
    if (enabled)
        lowLevel_.on(normalColor_);
    else
        lowLevel_.off(normalColor_);
}

void Indicate::setError(const bool enabled)
{
    if (enabled)
        lowLevel_.on(errorColor_);
    else
        lowLevel_.off(errorColor_);
}

void Indicate::turnOff()
{
    setNormal(false);
    setError(false);
    normalEnabled_ = false;
    errorEnabled_ = false;
    mode_ = Mode::off;
}
