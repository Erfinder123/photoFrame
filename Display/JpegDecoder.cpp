#include "JpegDecoder.hpp"
#include "AreaResampler.hpp"

#include <cstring>


JpegDecodeResult JpegDecoder::draw(
    const char* const path,
    uint8_t* const frameBuffer,
    const uint32_t frameWidth,
    const uint32_t frameHeight,
    const uint32_t frameStride,
    uint16_t* const intermediateBuffer,
    const uint32_t intermediatePixelCapacity,
    void (*const readFinished)())
{
    if (path == nullptr || frameBuffer == nullptr || intermediateBuffer == nullptr)
        return JpegDecodeResult::DECOMPRESSION_FAILED;

    fileResult_ = f_open(&file_, path, FA_READ);
    if (fileResult_ != FR_OK)
        return JpegDecodeResult::FILE_OPEN_FAILED;

    JDEC decoder{};
    JRESULT jpegResult = jd_prepare(
        &decoder,
        readInput,
        workBuffer_,
        sizeof(workBuffer_),
        this);
    if (jpegResult != JDR_OK)
    {
        static_cast<void>(f_close(&file_));
        if (fileResult_ != FR_OK)
            return JpegDecodeResult::FILE_READ_FAILED;
        return convertResult(jpegResult);
    }

    uint8_t scale = 0U;
    uint32_t outputWidth = decoder.width;
    uint32_t outputHeight = decoder.height;
    bool fittingScaleFound = false;

    /* Select the smallest intermediate image that still covers the complete
       display.  Since TJpgDec supports only powers of two, search from 1:8
       towards 1:1 and stop at the first usable resolution. */
    for (int32_t candidateScale = 3; candidateScale >= 0; --candidateScale)
    {
        const uint32_t divisor = 1UL << candidateScale;
        const uint32_t candidateWidth =
            (decoder.width + divisor - 1U) / divisor;
        const uint32_t candidateHeight =
            (decoder.height + divisor - 1U) / divisor;
        const uint64_t candidatePixelCount =
            static_cast<uint64_t>(candidateWidth) * candidateHeight;
        if (candidateWidth >= frameWidth
            && candidateHeight >= frameHeight
            && candidatePixelCount <= intermediatePixelCapacity)
        {
            scale = static_cast<uint8_t>(candidateScale);
            outputWidth = candidateWidth;
            outputHeight = candidateHeight;
            fittingScaleFound = true;
            break;
        }
    }

    /* A small source image cannot cover the display without enlargement.
       Preserve all of its available detail.  For an unusually large image
       whose aspect ratio prevents a fitting scale, use the largest resolution
       that fits the bounded intermediate buffer. */
    if (!fittingScaleFound)
    {
        for (uint8_t candidateScale = 0U;
             candidateScale <= 3U;
             ++candidateScale)
        {
            const uint32_t divisor = 1UL << candidateScale;
            const uint32_t candidateWidth =
                (decoder.width + divisor - 1U) / divisor;
            const uint32_t candidateHeight =
                (decoder.height + divisor - 1U) / divisor;
            if (static_cast<uint64_t>(candidateWidth) * candidateHeight
                <= intermediatePixelCapacity)
            {
                scale = candidateScale;
                outputWidth = candidateWidth;
                outputHeight = candidateHeight;
                fittingScaleFound = true;
                break;
            }
        }
    }

    if (!fittingScaleFound)
    {
        static_cast<void>(f_close(&file_));
        return JpegDecodeResult::IMAGE_TOO_LARGE;
    }

    frameBuffer_ = reinterpret_cast<uint8_t*>(intermediateBuffer);
    frameWidth_ = outputWidth;
    frameHeight_ = outputHeight;

    jpegResult = jd_decomp(&decoder, writeOutput, scale);
    const FRESULT closeResult = f_close(&file_);
    if (fileResult_ != FR_OK || closeResult != FR_OK)
        return JpegDecodeResult::FILE_READ_FAILED;
    if (jpegResult != JDR_OK)
        return convertResult(jpegResult);

    if (readFinished != nullptr)
        readFinished();

    AreaResampler::fillCropRgb565(
        intermediateBuffer,
        outputWidth,
        outputHeight,
        reinterpret_cast<uint16_t*>(frameBuffer),
        frameWidth,
        frameHeight,
        frameStride);
    return JpegDecodeResult::OK;
}

std::size_t JpegDecoder::readInput(
    JDEC* const decoder,
    uint8_t* const buffer,
    const std::size_t byteCount)
{
    auto* const instance = static_cast<JpegDecoder*>(decoder->device);
    if (buffer != nullptr)
    {
        UINT bytesRead = 0U;
        instance->fileResult_ = f_read(
            &instance->file_,
            buffer,
            static_cast<UINT>(byteCount),
            &bytesRead);
        return instance->fileResult_ == FR_OK ? bytesRead : 0U;
    }

    const FSIZE_t currentPosition = f_tell(&instance->file_);
    FSIZE_t targetPosition = currentPosition + byteCount;
    if (targetPosition > f_size(&instance->file_))
        targetPosition = f_size(&instance->file_);
    instance->fileResult_ = f_lseek(&instance->file_, targetPosition);
    return instance->fileResult_ == FR_OK
         ? static_cast<std::size_t>(targetPosition - currentPosition)
         : 0U;
}

int JpegDecoder::writeOutput(
    JDEC* const decoder,
    void* const bitmap,
    JRECT* const rectangle)
{
    auto* const instance = static_cast<JpegDecoder*>(decoder->device);
    const uint32_t sourceWidth = rectangle->right - rectangle->left + 1U;
    const uint32_t sourceHeight = rectangle->bottom - rectangle->top + 1U;
    const uint32_t destinationX = rectangle->left;
    const uint32_t destinationY = rectangle->top;
    if (destinationX + sourceWidth > instance->frameWidth_
        || destinationY + sourceHeight > instance->frameHeight_)
        return 0;

    const auto* source = static_cast<const uint8_t*>(bitmap);
    for (uint32_t row = 0U; row < sourceHeight; ++row)
    {
        uint8_t* const destination = instance->frameBuffer_
                                   + ((destinationY + row) * instance->frameWidth_
                                      + destinationX) * sizeof(uint16_t);
        std::memcpy(
            destination,
            source + row * sourceWidth * sizeof(uint16_t),
            sourceWidth * sizeof(uint16_t));
    }
    return 1;
}

JpegDecodeResult JpegDecoder::convertResult(const JRESULT result)
{
    switch (result)
    {
        case JDR_OK:
            return JpegDecodeResult::OK;
        case JDR_MEM1:
        case JDR_MEM2:
            return JpegDecodeResult::INSUFFICIENT_MEMORY;
        case JDR_FMT1:
            return JpegDecodeResult::INVALID_FORMAT;
        case JDR_FMT2:
        case JDR_FMT3:
            return JpegDecodeResult::UNSUPPORTED_FORMAT;
        case JDR_INP:
            return JpegDecodeResult::FILE_READ_FAILED;
        case JDR_INTR:
        case JDR_PAR:
        default:
            return JpegDecodeResult::DECOMPRESSION_FAILED;
    }
}
