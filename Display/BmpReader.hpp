#ifndef BMPREADER_HPP
#define BMPREADER_HPP


#include <cstdint>


enum class BmpReadResult : uint8_t
{
    OK,
    FILE_OPEN_FAILED,
    FILE_READ_FAILED,
    INVALID_FORMAT,
    UNSUPPORTED_FORMAT,
    IMAGE_TOO_LARGE
};

struct BmpReader
{
    static BmpReadResult draw(
        const char* path,
        uint8_t* frameBuffer,
        uint32_t frameWidth,
        uint32_t frameHeight,
        uint32_t frameStride,
        uint16_t* intermediateBuffer,
        uint32_t intermediatePixelCapacity,
        void (*readFinished)());
};


#endif //BMPREADER_HPP
