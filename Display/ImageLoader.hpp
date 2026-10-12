#ifndef IMAGELOADER_HPP
#define IMAGELOADER_HPP


#include <cstdint>

#include "JpegDecoder.hpp"
#include "PngDecoder.hpp"


enum class ImageFileType : uint8_t
{
    JPEG,
    PNG,
    BMP
};

enum class ImageDecodeResult : uint8_t
{
    OK,
    FILE_OPEN_FAILED,
    FILE_READ_FAILED,
    INVALID_FORMAT,
    UNSUPPORTED_FORMAT,
    INSUFFICIENT_MEMORY,
    IMAGE_TOO_LARGE,
    DECOMPRESSION_FAILED
};

class ImageLoader
{
    public:
        ImageLoader() = default;
        ImageLoader(const ImageLoader&) = delete;
        ImageLoader& operator=(const ImageLoader&) = delete;

        ImageDecodeResult draw(
            ImageFileType type,
            const char* path,
            uint8_t* frameBuffer,
            uint32_t frameWidth,
            uint32_t frameHeight,
            uint32_t frameStride,
            void (*readFinished)());

    private:
        static constexpr uint32_t intermediateBufferSize = 3U * 1024U * 1024U;
        static constexpr uint32_t intermediatePixelCapacity =
            intermediateBufferSize / sizeof(uint16_t);
        static uint16_t intermediateBuffer_[intermediatePixelCapacity];

        JpegDecoder jpegDecoder_;
        PngDecoder pngDecoder_;
};


#endif //IMAGELOADER_HPP
