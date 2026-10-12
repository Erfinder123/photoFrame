#ifndef TEXTLAYOUTCALCULATOR_HPP
#define TEXTLAYOUTCALCULATOR_HPP


#include "BitmapFont.hpp"
#include "Layout.hpp"

#include <cstdint>


struct TextMeasurement
{
    Size size;
    uint8_t scale = 1U;
};

class TextLayoutCalculator
{
    public:
        using FindGlyph = const BitmapGlyph* (*)(char32_t codePoint);

        TextLayoutCalculator(
            uint8_t fontHeight,
            FindGlyph primaryFindGlyph,
            FindGlyph fallbackFindGlyph = nullptr);

        [[nodiscard]] TextMeasurement measure(
            const char* text,
            uint16_t requestedHeight) const;

    private:
        [[nodiscard]] const BitmapGlyph* findGlyph(char32_t codePoint) const;
        [[nodiscard]] static char32_t decodeUtf8(const char*& text);

        uint8_t fontHeight_;
        FindGlyph primaryFindGlyph_;
        FindGlyph fallbackFindGlyph_;
};


#endif // TEXTLAYOUTCALCULATOR_HPP
