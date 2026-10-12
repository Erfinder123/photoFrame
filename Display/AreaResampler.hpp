#ifndef AREARESAMPLER_HPP
#define AREARESAMPLER_HPP


#include <cstdint>


namespace AreaResampler
{
    void fillCropRgb565(
        const uint16_t* source,
        uint32_t sourceWidth,
        uint32_t sourceHeight,
        uint16_t* destination,
        uint32_t destinationWidth,
        uint32_t destinationHeight,
        uint32_t destinationStride);
};


#endif //AREARESAMPLER_HPP
