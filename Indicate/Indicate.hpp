#ifndef INDICATE_HPP
#define INDICATE_HPP


#include <cstdint>
#include "IndicateLowLevel.hpp"


class Indicate
{
    public:
        Indicate(IndicateColor normalColor, IndicateColor errorColor);
        virtual ~Indicate();

        virtual void initialization();
        virtual void normalOperation();
        virtual void dataExchange();
        virtual void error();

    private:
        Indicate(
            IndicateColor normalColor,
            IndicateColor errorColor,
            bool configureHardware);

        enum class Mode : uint8_t
        {
            off,
            initialization,
            normalOperation,
            dataExchange,
            error
        };

        static constexpr uint32_t initializationBlinkInterval = 250U;
        static constexpr uint32_t exchangeBlinkInterval = 100U;

        void setNormal(bool enabled);
        void setError(bool enabled);
        void turnOff();

        IndicateLowLevel lowLevel_;
        const IndicateColor normalColor_;
        const IndicateColor errorColor_;
        uint32_t lastInitializationToggleTick_ = 0U;
        uint32_t lastExchangeToggleTick_ = 0U;
        bool normalEnabled_ = false;
        bool errorEnabled_ = false;
        bool hardwareEnabled_ = true;
        Mode mode_ = Mode::off;
};


#endif // INDICATE_HPP
