#ifndef JPEGDECODER_HPP
#define JPEGDECODER_HPP


#include <cstddef>
#include <cstdint>

#include "ff.h"
#include "tjpgd.h"


enum class JpegDecodeResult : uint8_t
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

class JpegDecoder
{
    public:
        JpegDecodeResult draw(
            const char* path,
            uint8_t* frameBuffer,
            uint32_t frameWidth,
            uint32_t frameHeight,
            uint32_t frameStride,
            uint16_t* intermediateBuffer,
            uint32_t intermediatePixelCapacity,
            void (*readFinished)());

    private:
        static constexpr std::size_t workBufferSize = 4096U;

        static std::size_t readInput(
            JDEC* decoder,
            uint8_t* buffer,
            std::size_t byteCount);
        static int writeOutput(
            JDEC* decoder,
            void* bitmap,
            JRECT* rectangle);
        static JpegDecodeResult convertResult(JRESULT result);

        alignas(uint32_t) uint8_t workBuffer_[workBufferSize]{};
        FIL file_{};
        uint8_t* frameBuffer_ = nullptr;
        uint32_t frameWidth_ = 0U;
        uint32_t frameHeight_ = 0U;
        FRESULT fileResult_ = FR_OK;
};


#endif //JPEGDECODER_HPP
