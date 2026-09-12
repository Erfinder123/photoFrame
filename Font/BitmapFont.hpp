#ifndef BITMAPFONT_HPP
#define BITMAPFONT_HPP


#include <cstdint>


struct BitmapGlyph
{
    char32_t codePoint;
    uint16_t bitmapOffset;
    uint8_t width;
};

struct BitmapFont
{
    static constexpr uint8_t height = 16U;

    [[nodiscard]] static const BitmapGlyph* findGlyph(char32_t codePoint);
    [[nodiscard]] static const uint8_t* getBitmap(const BitmapGlyph& glyph);
};


#endif //BITMAPFONT_HPP
