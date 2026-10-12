#include "BmpReader.hpp"

#include "AreaResampler.hpp"
#include "ff.h"


namespace
{
    constexpr uint32_t maximumBmpWidth = 4096U;
    __attribute__((section(".external_ram.bmp"), aligned(4)))
    uint8_t bmpRow[maximumBmpWidth * 4U];

    uint16_t read16(const uint8_t* const data)
    {
        return static_cast<uint16_t>(data[0])
             | static_cast<uint16_t>(data[1] << 8U);
    }

    uint32_t read32(const uint8_t* const data)
    {
        return static_cast<uint32_t>(data[0])
             | (static_cast<uint32_t>(data[1]) << 8U)
             | (static_cast<uint32_t>(data[2]) << 16U)
             | (static_cast<uint32_t>(data[3]) << 24U);
    }

    bool readExact(FIL& file, uint8_t* const buffer, const UINT size)
    {
        UINT bytesRead = 0U;
        return f_read(&file, buffer, size, &bytesRead) == FR_OK
            && bytesRead == size;
    }
}

BmpReadResult BmpReader::draw(
    const char* const path,
    uint8_t* const frameBuffer,
    const uint32_t frameWidth,
    const uint32_t frameHeight,
    const uint32_t frameStride,
    uint16_t* const intermediateBuffer,
    const uint32_t intermediatePixelCapacity,
    void (*const readFinished)())
{
    if (intermediateBuffer == nullptr)
        return BmpReadResult::IMAGE_TOO_LARGE;

    FIL file{};
    if (f_open(&file, path, FA_READ) != FR_OK)
        return BmpReadResult::FILE_OPEN_FAILED;

    uint8_t header[54]{};
    if (!readExact(file, header, sizeof(header)))
    {
        static_cast<void>(f_close(&file));
        return BmpReadResult::FILE_READ_FAILED;
    }
    const int32_t signedWidth = static_cast<int32_t>(read32(header + 18U));
    const int32_t signedHeight = static_cast<int32_t>(read32(header + 22U));
    const uint16_t bitsPerPixel = read16(header + 28U);
    if (header[0] != 'B' || header[1] != 'M'
        || read32(header + 14U) < 40U || signedWidth <= 0
        || signedHeight == 0 || read16(header + 26U) != 1U)
    {
        static_cast<void>(f_close(&file));
        return BmpReadResult::INVALID_FORMAT;
    }
    if (read32(header + 30U) != 0U
        || (bitsPerPixel != 24U && bitsPerPixel != 32U))
    {
        static_cast<void>(f_close(&file));
        return BmpReadResult::UNSUPPORTED_FORMAT;
    }

    const uint32_t width = static_cast<uint32_t>(signedWidth);
    const uint32_t height = signedHeight < 0
                          ? static_cast<uint32_t>(-static_cast<int64_t>(signedHeight))
                          : static_cast<uint32_t>(signedHeight);
    const uint32_t bytesPerPixel = bitsPerPixel / 8U;
    const uint32_t rowSize = (width * bytesPerPixel + 3U) & ~3UL;
    if (width > maximumBmpWidth || rowSize > sizeof(bmpRow)
        || static_cast<uint64_t>(width) * height
           > intermediatePixelCapacity)
    {
        static_cast<void>(f_close(&file));
        return BmpReadResult::IMAGE_TOO_LARGE;
    }

    const FSIZE_t pixelOffset = read32(header + 10U);
    for (uint32_t fileRow = 0U; fileRow < height; ++fileRow)
    {
        if (f_lseek(&file, pixelOffset + static_cast<FSIZE_t>(fileRow) * rowSize)
                != FR_OK
            || !readExact(file, bmpRow, rowSize))
        {
            static_cast<void>(f_close(&file));
            return BmpReadResult::FILE_READ_FAILED;
        }
        const uint32_t destinationRow = signedHeight < 0
                                      ? fileRow
                                      : height - fileRow - 1U;
        uint16_t* const destination = intermediateBuffer
                                    + destinationRow * width;
        for (uint32_t x = 0U; x < width; ++x)
        {
            const uint8_t* const pixel = bmpRow + x * bytesPerPixel;
            destination[x] = static_cast<uint16_t>(
                ((pixel[2] >> 3U) << 11U)
                | ((pixel[1] >> 2U) << 5U)
                | (pixel[0] >> 3U));
        }
    }
    if (f_close(&file) != FR_OK)
        return BmpReadResult::FILE_READ_FAILED;

    if (readFinished != nullptr)
        readFinished();

    AreaResampler::fillCropRgb565(
        intermediateBuffer,
        width,
        height,
        reinterpret_cast<uint16_t*>(frameBuffer),
        frameWidth,
        frameHeight,
        frameStride);
    return BmpReadResult::OK;
}
