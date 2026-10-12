#include "BaseFrame.hpp"

#include <algorithm>
#include <cstring>


__attribute__((section(".external_ram.base_frame"), aligned(4)))
uint16_t BaseFrame::pixels_[pixelCount];


void BaseFrame::capture(const uint8_t* const frameBuffer)
{
    if (frameBuffer == nullptr)
        return;

    std::memcpy(pixels_, frameBuffer, sizeof(pixels_));
    valid_ = true;
    ++version_;
}

void BaseFrame::restore(uint8_t* const frameBuffer, const Rect& area) const
{
    if (!valid_ || frameBuffer == nullptr)
        return;

    const Rect screen{{0, 0}, {width, height}};
    const Rect clipped = LayoutCalculator::intersection(area, screen);
    if (clipped.size.width == 0U || clipped.size.height == 0U)
        return;

    auto* const destination = reinterpret_cast<uint16_t*>(frameBuffer);
    for (uint32_t row = 0U; row < clipped.size.height; ++row)
    {
        const uint32_t y = static_cast<uint32_t>(clipped.position.y) + row;
        const uint32_t x = static_cast<uint32_t>(clipped.position.x);
        std::memcpy(
            destination + y * width + x,
            pixels_ + y * width + x,
            clipped.size.width * sizeof(uint16_t));
    }
}

void BaseFrame::restoreAll(uint8_t* const frameBuffer) const
{
    if (!valid_ || frameBuffer == nullptr)
        return;
    std::memcpy(frameBuffer, pixels_, sizeof(pixels_));
}

bool BaseFrame::isValid() const
{
    return valid_;
}

uint32_t BaseFrame::getVersion() const
{
    return version_;
}
