#ifndef PNGDECODER_HPP
#define PNGDECODER_HPP


#include <cstdint>

#include "PNGdec.h"
#include "ff.h"


enum class PngDecodeResult : uint8_t
{
    OK,
    FILE_OPEN_FAILED,
    FILE_READ_FAILED,
    INVALID_FORMAT,
    UNSUPPORTED_FORMAT,
    IMAGE_TOO_LARGE,
    DECOMPRESSION_FAILED
};

class PngDecoder
{
    public:
        PngDecodeResult draw(
            const char* path,
            uint8_t* frameBuffer,
            uint32_t frameWidth,
            uint32_t frameHeight,
            uint32_t frameStride,
            uint16_t* intermediateBuffer,
            uint32_t intermediatePixelCapacity,
            void (*readFinished)());

    private:
        static void* openFile(const char* path, int32_t* fileSize);
        static void closeFile(void* handle);
        static int32_t readFile(PNGFILE* file, uint8_t* buffer, int32_t length);
        static int32_t seekFile(PNGFILE* file, int32_t position);
        static int drawLine(PNGDRAW* line);

        static PngDecoder* active_;
        FIL file_{};
        FRESULT fileResult_ = FR_OK;
        uint32_t imageWidth_ = 0U;
        uint16_t* intermediateBuffer_ = nullptr;
};


#endif //PNGDECODER_HPP
