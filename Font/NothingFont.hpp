#pragma once

#include "BitmapFont.hpp"

#include <cstdint>

struct NothingFont
{
    static constexpr uint8_t height = 16U;

    [[nodiscard]] static const BitmapGlyph* findGlyph(char32_t codePoint);
    [[nodiscard]] static const uint8_t* getBitmap(const BitmapGlyph& glyph);
};
