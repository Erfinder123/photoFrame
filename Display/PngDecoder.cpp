#include "PngDecoder.hpp"

#include "AreaResampler.hpp"

#include <cstring>


namespace
{
    constexpr uint32_t maximumPngWidth = 4096U;
    __attribute__((section(".external_ram.png"), aligned(8)))
    PNG png;
    __attribute__((section(".external_ram.png"), aligned(4)))
    uint16_t pngLine[maximumPngWidth];
}

PngDecoder* PngDecoder::active_ = nullptr;

PngDecodeResult PngDecoder::draw(
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
        return PngDecodeResult::DECOMPRESSION_FAILED;

    active_ = this;
    intermediateBuffer_ = intermediateBuffer;
    fileResult_ = FR_OK;
    if (!png.open(path, openFile, closeFile, readFile, seekFile, drawLine))
    {
        active_ = nullptr;
        return fileResult_ == FR_OK
             ? PngDecodeResult::INVALID_FORMAT
             : PngDecodeResult::FILE_OPEN_FAILED;
    }

    imageWidth_ = static_cast<uint32_t>(png.getWidth());
    const uint32_t imageHeight = static_cast<uint32_t>(png.getHeight());
    if (imageWidth_ > maximumPngWidth
        || static_cast<uint64_t>(imageWidth_) * imageHeight
           > intermediatePixelCapacity)
    {
        png.close();
        active_ = nullptr;
        return PngDecodeResult::IMAGE_TOO_LARGE;
    }
    if (png.isInterlaced())
    {
        png.close();
        active_ = nullptr;
        return PngDecodeResult::UNSUPPORTED_FORMAT;
    }

    const int result = png.decode(this, PNG_CHECK_CRC);
    png.close();
    active_ = nullptr;
    if (fileResult_ != FR_OK)
        return PngDecodeResult::FILE_READ_FAILED;
    if (result != PNG_SUCCESS)
        return result == PNG_UNSUPPORTED_FEATURE
             ? PngDecodeResult::UNSUPPORTED_FORMAT
             : PngDecodeResult::DECOMPRESSION_FAILED;

    if (readFinished != nullptr)
        readFinished();

    AreaResampler::fillCropRgb565(
        intermediateBuffer_,
        imageWidth_,
        imageHeight,
        reinterpret_cast<uint16_t*>(frameBuffer),
        frameWidth,
        frameHeight,
        frameStride);
    return PngDecodeResult::OK;
}

void* PngDecoder::openFile(const char* const path, int32_t* const fileSize)
{
    if (active_ == nullptr)
        return nullptr;
    active_->fileResult_ = f_open(&active_->file_, path, FA_READ);
    if (active_->fileResult_ != FR_OK)
        return nullptr;
    if (f_size(&active_->file_) > INT32_MAX)
    {
        static_cast<void>(f_close(&active_->file_));
        active_->fileResult_ = FR_INVALID_OBJECT;
        return nullptr;
    }
    *fileSize = static_cast<int32_t>(f_size(&active_->file_));
    return &active_->file_;
}

void PngDecoder::closeFile(void* const handle)
{
    if (active_ != nullptr && handle != nullptr)
        active_->fileResult_ = f_close(static_cast<FIL*>(handle));
}

int32_t PngDecoder::readFile(
    PNGFILE* const file,
    uint8_t* const buffer,
    const int32_t length)
{
    if (active_ == nullptr || file == nullptr || file->fHandle == nullptr)
        return 0;
    UINT bytesRead = 0U;
    active_->fileResult_ = f_read(
        static_cast<FIL*>(file->fHandle),
        buffer,
        static_cast<UINT>(length),
        &bytesRead);
    return active_->fileResult_ == FR_OK ? static_cast<int32_t>(bytesRead) : 0;
}

int32_t PngDecoder::seekFile(PNGFILE* const file, const int32_t position)
{
    if (active_ == nullptr || file == nullptr || file->fHandle == nullptr)
        return 0;
    active_->fileResult_ = f_lseek(
        static_cast<FIL*>(file->fHandle),
        static_cast<FSIZE_t>(position));
    return active_->fileResult_ == FR_OK ? position : 0;
}

int PngDecoder::drawLine(PNGDRAW* const line)
{
    auto* const instance = static_cast<PngDecoder*>(line->pUser);
    if (instance == nullptr || line->iWidth < 0
        || static_cast<uint32_t>(line->iWidth) > maximumPngWidth)
        return 0;
    png.getLineAsRGB565(
        line,
        pngLine,
        PNG_RGB565_LITTLE_ENDIAN,
        0U);
    std::memcpy(
        instance->intermediateBuffer_
            + static_cast<uint32_t>(line->y) * instance->imageWidth_,
        pngLine,
        static_cast<uint32_t>(line->iWidth) * sizeof(uint16_t));
    return 1;
}
