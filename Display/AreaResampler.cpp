#include "AreaResampler.hpp"

#include <cstring>


namespace
{
    constexpr uint32_t maximumOutputWidth = 800U;
    constexpr uint32_t channelFractionBits = 8U;
    constexpr uint64_t fixedPointOne = 1ULL << 16U;

    __attribute__((section(".external_ram.resize"), aligned(4)))
    uint16_t horizontalRed[maximumOutputWidth];
    __attribute__((section(".external_ram.resize"), aligned(4)))
    uint16_t horizontalGreen[maximumOutputWidth];
    __attribute__((section(".external_ram.resize"), aligned(4)))
    uint16_t horizontalBlue[maximumOutputWidth];
    __attribute__((section(".external_ram.resize"), aligned(8)))
    uint64_t verticalRed[maximumOutputWidth];
    __attribute__((section(".external_ram.resize"), aligned(8)))
    uint64_t verticalGreen[maximumOutputWidth];
    __attribute__((section(".external_ram.resize"), aligned(8)))
    uint64_t verticalBlue[maximumOutputWidth];

    uint64_t overlap(
        const uint64_t areaStart,
        const uint64_t areaEnd,
        const uint32_t pixel)
    {
        const uint64_t pixelStart = static_cast<uint64_t>(pixel) << 16U;
        const uint64_t pixelEnd = pixelStart + fixedPointOne;
        const uint64_t start = areaStart > pixelStart ? areaStart : pixelStart;
        const uint64_t end = areaEnd < pixelEnd ? areaEnd : pixelEnd;
        return end > start ? end - start : 0U;
    }

    void resampleHorizontalRow(
        const uint16_t* const source,
        const uint64_t cropX,
        const uint64_t cropWidth,
        const uint32_t destinationWidth)
    {
        for (uint32_t destinationX = 0U;
             destinationX < destinationWidth;
             ++destinationX)
        {
            const uint64_t sourceLeft = cropX
                + cropWidth * destinationX / destinationWidth;
            const uint64_t sourceRight = cropX
                + cropWidth * (destinationX + 1U) / destinationWidth;
            const uint32_t firstSourceX = sourceLeft >> 16U;
            const uint32_t lastSourceX = (sourceRight - 1U) >> 16U;
            uint64_t red = 0U;
            uint64_t green = 0U;
            uint64_t blue = 0U;
            uint64_t totalWeight = 0U;

            for (uint32_t sourceX = firstSourceX;
                 sourceX <= lastSourceX;
                 ++sourceX)
            {
                const uint64_t weight = overlap(sourceLeft, sourceRight, sourceX);
                const uint16_t pixel = source[sourceX];
                red += ((pixel >> 11U) & 0x1FU) * weight;
                green += ((pixel >> 5U) & 0x3FU) * weight;
                blue += (pixel & 0x1FU) * weight;
                totalWeight += weight;
            }

            horizontalRed[destinationX] = static_cast<uint16_t>(
                (red << channelFractionBits) / totalWeight);
            horizontalGreen[destinationX] = static_cast<uint16_t>(
                (green << channelFractionBits) / totalWeight);
            horizontalBlue[destinationX] = static_cast<uint16_t>(
                (blue << channelFractionBits) / totalWeight);
        }
    }
}

void AreaResampler::fillCropRgb565(
    const uint16_t* const source,
    const uint32_t sourceWidth,
    const uint32_t sourceHeight,
    uint16_t* const destination,
    const uint32_t destinationWidth,
    const uint32_t destinationHeight,
    const uint32_t destinationStride)
{
    if (source == nullptr || destination == nullptr
        || sourceWidth == 0U || sourceHeight == 0U
        || destinationWidth == 0U || destinationHeight == 0U
        || destinationWidth > maximumOutputWidth
        || destinationStride < destinationWidth)
        return;

    uint64_t cropX = 0U;
    uint64_t cropY = 0U;
    uint64_t cropWidth = static_cast<uint64_t>(sourceWidth) << 16U;
    uint64_t cropHeight = static_cast<uint64_t>(sourceHeight) << 16U;
    if (static_cast<uint64_t>(sourceWidth) * destinationHeight
        > static_cast<uint64_t>(sourceHeight) * destinationWidth)
    {
        cropWidth = static_cast<uint64_t>(sourceHeight)
                  * destinationWidth * fixedPointOne / destinationHeight;
        cropX = ((static_cast<uint64_t>(sourceWidth) << 16U) - cropWidth) / 2U;
    }
    else
    {
        cropHeight = static_cast<uint64_t>(sourceWidth)
                   * destinationHeight * fixedPointOne / destinationWidth;
        cropY = ((static_cast<uint64_t>(sourceHeight) << 16U) - cropHeight) / 2U;
    }

    for (uint32_t destinationY = 0U;
         destinationY < destinationHeight;
         ++destinationY)
    {
        std::memset(verticalRed, 0, destinationWidth * sizeof(uint64_t));
        std::memset(verticalGreen, 0, destinationWidth * sizeof(uint64_t));
        std::memset(verticalBlue, 0, destinationWidth * sizeof(uint64_t));

        const uint64_t sourceTop = cropY
            + cropHeight * destinationY / destinationHeight;
        const uint64_t sourceBottom = cropY
            + cropHeight * (destinationY + 1U) / destinationHeight;
        const uint32_t firstSourceY = sourceTop >> 16U;
        const uint32_t lastSourceY = (sourceBottom - 1U) >> 16U;
        uint64_t totalVerticalWeight = 0U;

        for (uint32_t sourceY = firstSourceY;
             sourceY <= lastSourceY;
             ++sourceY)
        {
            resampleHorizontalRow(
                source + sourceY * sourceWidth,
                cropX,
                cropWidth,
                destinationWidth);
            const uint64_t verticalWeight = overlap(
                sourceTop,
                sourceBottom,
                sourceY);
            totalVerticalWeight += verticalWeight;

            for (uint32_t destinationX = 0U;
                 destinationX < destinationWidth;
                 ++destinationX)
            {
                verticalRed[destinationX]
                    += horizontalRed[destinationX] * verticalWeight;
                verticalGreen[destinationX]
                    += horizontalGreen[destinationX] * verticalWeight;
                verticalBlue[destinationX]
                    += horizontalBlue[destinationX] * verticalWeight;
            }
        }

        for (uint32_t destinationX = 0U;
             destinationX < destinationWidth;
             ++destinationX)
        {
            const uint16_t red = static_cast<uint16_t>(
                ((verticalRed[destinationX] / totalVerticalWeight) + 128U)
                >> channelFractionBits);
            const uint16_t green = static_cast<uint16_t>(
                ((verticalGreen[destinationX] / totalVerticalWeight) + 128U)
                >> channelFractionBits);
            const uint16_t blue = static_cast<uint16_t>(
                ((verticalBlue[destinationX] / totalVerticalWeight) + 128U)
                >> channelFractionBits);
            destination[destinationY * destinationStride + destinationX]
                = static_cast<uint16_t>((red << 11U) | (green << 5U) | blue);
        }
    }
}
