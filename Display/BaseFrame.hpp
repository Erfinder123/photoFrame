#ifndef BASEFRAME_HPP
#define BASEFRAME_HPP


#include "Layout.hpp"

#include <cstdint>


class BaseFrame
{
    public:
        static constexpr uint32_t width = 800U;
        static constexpr uint32_t height = 480U;

        void capture(const uint8_t* frameBuffer);
        void restore(uint8_t* frameBuffer, const Rect& area) const;
        void restoreAll(uint8_t* frameBuffer) const;
        [[nodiscard]] bool isValid() const;
        [[nodiscard]] uint32_t getVersion() const;

    private:
        static constexpr uint32_t pixelCount = width * height;
        static uint16_t pixels_[pixelCount];
        bool valid_ = false;
        uint32_t version_ = 0U;
};


#endif // BASEFRAME_HPP
