#ifndef DISPLAYLOWLEVEL_HPP
#define DISPLAYLOWLEVEL_HPP


#include <cstdint>


class DisplayLowLevel
{
    public:
        [[nodiscard]] bool enable();
        [[nodiscard]] uint8_t* getFrontBuffer() const;
        [[nodiscard]] uint8_t* getBackBuffer() const;
        void swapBuffers();
        void refresh() const;

    private:
        enum class Controller : uint8_t
        {
            unknown,
            otm8009a,
            nt35510
        };

        static constexpr uint32_t width = 800U;
        static constexpr uint32_t height = 480U;

        uint8_t* frontBuffer_ = nullptr;
        uint8_t* backBuffer_ = nullptr;
        Controller controller_ = Controller::unknown;

        [[nodiscard]] bool configureDsi();
        void configureVideoTimings() const;
        void configureLtdc() const;
        void resetController() const;
        [[nodiscard]] Controller detectController() const;
        [[nodiscard]] bool initializeController() const;
        [[nodiscard]] bool initializeOtm8009a() const;
        [[nodiscard]] bool initializeNt35510() const;
        [[nodiscard]] bool readDcs(uint8_t command, uint8_t& value) const;
        [[nodiscard]] bool writeDcs(uint8_t command) const;
        [[nodiscard]] bool writeDcs(uint8_t command, uint8_t value) const;
        [[nodiscard]] bool writeDcs(uint8_t command, const uint8_t* data, uint32_t size) const;
        [[nodiscard]] bool waitCommandFifo() const;
        static void delayMilliseconds(uint32_t milliseconds);
};


#endif //DISPLAYLOWLEVEL_HPP
