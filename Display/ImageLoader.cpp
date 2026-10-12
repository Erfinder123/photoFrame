#include "ImageLoader.hpp"

#include "BmpReader.hpp"


__attribute__((section(".external_ram.image"), aligned(4)))
uint16_t ImageLoader::intermediateBuffer_[intermediatePixelCapacity];


namespace
{
    ImageDecodeResult convertJpegResult(const JpegDecodeResult result)
    {
        return static_cast<ImageDecodeResult>(result);
    }

    ImageDecodeResult convertPngResult(const PngDecodeResult result)
    {
        switch (result)
        {
            case PngDecodeResult::OK:
                return ImageDecodeResult::OK;
            case PngDecodeResult::FILE_OPEN_FAILED:
                return ImageDecodeResult::FILE_OPEN_FAILED;
            case PngDecodeResult::FILE_READ_FAILED:
                return ImageDecodeResult::FILE_READ_FAILED;
            case PngDecodeResult::INVALID_FORMAT:
                return ImageDecodeResult::INVALID_FORMAT;
            case PngDecodeResult::UNSUPPORTED_FORMAT:
                return ImageDecodeResult::UNSUPPORTED_FORMAT;
            case PngDecodeResult::IMAGE_TOO_LARGE:
                return ImageDecodeResult::IMAGE_TOO_LARGE;
            case PngDecodeResult::DECOMPRESSION_FAILED:
            default:
                return ImageDecodeResult::DECOMPRESSION_FAILED;
        }
    }

    ImageDecodeResult convertBmpResult(const BmpReadResult result)
    {
        switch (result)
        {
            case BmpReadResult::OK:
                return ImageDecodeResult::OK;
            case BmpReadResult::FILE_OPEN_FAILED:
                return ImageDecodeResult::FILE_OPEN_FAILED;
            case BmpReadResult::FILE_READ_FAILED:
                return ImageDecodeResult::FILE_READ_FAILED;
            case BmpReadResult::INVALID_FORMAT:
                return ImageDecodeResult::INVALID_FORMAT;
            case BmpReadResult::UNSUPPORTED_FORMAT:
                return ImageDecodeResult::UNSUPPORTED_FORMAT;
            case BmpReadResult::IMAGE_TOO_LARGE:
            default:
                return ImageDecodeResult::IMAGE_TOO_LARGE;
        }
    }
}

ImageDecodeResult ImageLoader::draw(
    const ImageFileType type,
    const char* const path,
    uint8_t* const frameBuffer,
    const uint32_t frameWidth,
    const uint32_t frameHeight,
    const uint32_t frameStride,
    void (*const readFinished)())
{
    switch (type)
    {
        case ImageFileType::JPEG:
            return convertJpegResult(jpegDecoder_.draw(
                path,
                frameBuffer,
                frameWidth,
                frameHeight,
                frameStride,
                intermediateBuffer_,
                intermediatePixelCapacity,
                readFinished));
        case ImageFileType::PNG:
            return convertPngResult(pngDecoder_.draw(
                path,
                frameBuffer,
                frameWidth,
                frameHeight,
                frameStride,
                intermediateBuffer_,
                intermediatePixelCapacity,
                readFinished));
        case ImageFileType::BMP:
            return convertBmpResult(BmpReader::draw(
                path,
                frameBuffer,
                frameWidth,
                frameHeight,
                frameStride,
                intermediateBuffer_,
                intermediatePixelCapacity,
                readFinished));
    }
    return ImageDecodeResult::UNSUPPORTED_FORMAT;
}
