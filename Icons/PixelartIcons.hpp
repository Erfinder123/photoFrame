#pragma once

#include <cstdint>

enum class PixelartIcon : uint8_t
{
    NONE,
    FOLDER,
    FILE,
    IMAGE,
    VIDEO,
    UP
};

struct PixelartIcons
{
    static constexpr uint8_t width = 24U;
    static constexpr uint8_t height = 24U;
    static constexpr uint8_t bytesPerRow = width / 8U;

    [[nodiscard]] static const uint8_t* getBitmap(PixelartIcon icon);
};
