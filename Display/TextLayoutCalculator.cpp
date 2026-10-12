#include "TextLayoutCalculator.hpp"

#include <algorithm>


TextLayoutCalculator::TextLayoutCalculator(
    const uint8_t fontHeight,
    const FindGlyph primaryFindGlyph,
    const FindGlyph fallbackFindGlyph)
    : fontHeight_(fontHeight),
      primaryFindGlyph_(primaryFindGlyph),
      fallbackFindGlyph_(fallbackFindGlyph)
{
}

TextMeasurement TextLayoutCalculator::measure(
    const char* text,
    const uint16_t requestedHeight) const
{
    if (text == nullptr || fontHeight_ == 0U || primaryFindGlyph_ == nullptr)
        return {};

    const uint32_t roundedScale =
        (static_cast<uint32_t>(requestedHeight) + fontHeight_ / 2U)
        / fontHeight_;
    const uint8_t scale = static_cast<uint8_t>(
        std::clamp<uint32_t>(roundedScale, 1U, UINT8_MAX));

    uint32_t width = 0U;
    uint32_t characterCount = 0U;
    while (*text != '\0')
    {
        const BitmapGlyph* const glyph = findGlyph(decodeUtf8(text));
        if (glyph == nullptr)
            continue;
        width += static_cast<uint32_t>(glyph->width) * scale;
        ++characterCount;
    }
    if (characterCount > 1U)
        width += (characterCount - 1U) * scale;

    return {
        {
            width,
            static_cast<uint32_t>(fontHeight_) * scale
        },
        scale
    };
}

const BitmapGlyph* TextLayoutCalculator::findGlyph(
    const char32_t codePoint) const
{
    const BitmapGlyph* glyph = primaryFindGlyph_(codePoint);
    if (glyph == nullptr && fallbackFindGlyph_ != nullptr)
        glyph = fallbackFindGlyph_(codePoint);
    return glyph;
}

char32_t TextLayoutCalculator::decodeUtf8(const char*& text)
{
    const auto first = static_cast<uint8_t>(*text++);
    if (first < 0x80U)
        return first;

    uint32_t codePoint = 0U;
    uint8_t continuationCount = 0U;
    if ((first & 0xE0U) == 0xC0U)
    {
        codePoint = first & 0x1FU;
        continuationCount = 1U;
    }
    else if ((first & 0xF0U) == 0xE0U)
    {
        codePoint = first & 0x0FU;
        continuationCount = 2U;
    }
    else if ((first & 0xF8U) == 0xF0U)
    {
        codePoint = first & 0x07U;
        continuationCount = 3U;
    }
    else
        return U'?';

    for (uint8_t index = 0U; index < continuationCount; ++index)
    {
        const auto continuation = static_cast<uint8_t>(*text);
        if ((continuation & 0xC0U) != 0x80U)
            return U'?';
        ++text;
        codePoint = (codePoint << 6U) | (continuation & 0x3FU);
    }
    return static_cast<char32_t>(codePoint);
}
