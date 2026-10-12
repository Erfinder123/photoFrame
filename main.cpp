
#include "main.hpp"
#include "SystemCard.hpp"
#include "CardLowLevel.hpp"
#include "Indicate.hpp"
#include "DisplayLowLevel.hpp"
#include "ImageLoader.hpp"
#include "BaseFrame.hpp"
#include "Layout.hpp"
#include "TextLayoutCalculator.hpp"
#include "RtcClock.hpp"
#include "BitmapFont.hpp"
#include "NothingFont.hpp"
#include "MatrixSansFont.hpp"
#include "PixelartIcons.hpp"
#include "ff.h"
#include "diskio.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <optional>
#include <cstring>


volatile CardInitError cardInitError = CardInitError::UNSUPPORTED_CARD;
volatile uint8_t failedCardCommand = 0xFFU;
volatile FRESULT fileSystemMountResult = FR_NOT_READY;
volatile bool displayInitializationSucceeded = false;


namespace
{
    std::optional<SystemCard> card;
    CardLowLevel cardLowLevel;
    DisplayLowLevel displayLowLevel;
    ImageLoader imageLoader;
    BaseFrame baseFrame;
    TaskHandle_t taskDispatcherHandle = nullptr;
    TaskHandle_t cardInitTaskHandle = nullptr;
    TaskHandle_t cardDeinitTaskHandle = nullptr;
    TaskHandle_t rootDirectoryTaskHandle = nullptr;
    TaskHandle_t clockTaskHandle = nullptr;
    QueueHandle_t taskDispatcherEventQueue = nullptr;

    constexpr uint32_t displayWidth = 800U;
    constexpr uint32_t displayHeight = 480U;
    constexpr uint32_t displayBytesPerPixel = 2U;
    constexpr uint32_t textScale = 2U;
    constexpr uint32_t textMargin = 8U;
    constexpr uint16_t clockRequestedHeight = 46U;
    constexpr ContainerId imageContainerId = 1U;
    constexpr ContainerId clockContainerId = 2U;
    constexpr uint16_t backgroundColor = 0x0000U;
    constexpr uint16_t textColor = 0xFFFFU;
    constexpr uint16_t clockShadowColor = 0x0000U;

    enum class DisplayContainerIndex : uint8_t
    {
        IMAGE,
        CLOCK,
        COUNT
    };

    Container displayContainers[] =
    {
        {
            .id = imageContainerId,
            .parentId = rootContainerId,
            .anchorTargetId = rootContainerId,
            .name = "image",
            .type = ContainerType::IMAGE,
            .size = {displayWidth, displayHeight},
            .anchor = {
                HorizontalAnchor::CENTER,
                VerticalAnchor::CENTER
            },
            .offset = {},
            .priority = 0U,
            .zIndex = 0U,
            .order = 0U,
            .displacement = DisplacementDirection::HORIZONTAL,
            .visible = true,
            .calculatedBounds = {},
            .visibleBounds = {}
        },
        {
            .id = clockContainerId,
            .parentId = rootContainerId,
            .anchorTargetId = rootContainerId,
            .name = "clock",
            .type = ContainerType::TEXT,
            .size = {},
            .anchor = {
                HorizontalAnchor::RIGHT,
                VerticalAnchor::TOP
            },
            .offset = {
                static_cast<int32_t>(textMargin),
                static_cast<int32_t>(textMargin)
            },
            .priority = 1U,
            .zIndex = 1U,
            .order = 0U,
            .displacement = DisplacementDirection::HORIZONTAL,
            .visible = true,
            .calculatedBounds = {},
            .visibleBounds = {}
        }
    };
    LayoutCalculator layoutCalculator;
    const Rect screenBounds{{0, 0}, {displayWidth, displayHeight}};

    Container& getDisplayContainer(const DisplayContainerIndex index)
    {
        return displayContainers[static_cast<uint8_t>(index)];
    }

    bool calculateDisplayLayout()
    {
        return layoutCalculator.calculate(
            displayContainers,
            static_cast<uint32_t>(DisplayContainerIndex::COUNT),
            screenBounds);
    }

    uint8_t* textFrameBuffer = nullptr;
    uint32_t textCursorY = textMargin;

    void finishImageRead()
    {
        if (card)
            card->finishDataExchange();
    }

    void setPixel(
        uint8_t* const frameBuffer,
        const uint32_t x,
        const uint32_t y,
        const uint16_t color)
    {
        if (frameBuffer == nullptr || x >= displayWidth || y >= displayHeight)
            return;

        const uint32_t offset = (y * displayWidth + x) * displayBytesPerPixel;
        frameBuffer[offset] = static_cast<uint8_t>(color);
        frameBuffer[offset + 1U] = static_cast<uint8_t>(color >> 8U);
    }

    void clearFrameBuffer(uint8_t* const frameBuffer, const uint16_t color)
    {
        for (uint32_t y = 0U; y < displayHeight; ++y)
            for (uint32_t x = 0U; x < displayWidth; ++x)
                setPixel(frameBuffer, x, y, color);
    }

    char32_t decodeUtf8(const char*& text)
    {
        const auto first = static_cast<uint8_t>(*text++);
        if (first < 0x80U)
            return first;

        uint32_t codePoint = 0U;
        uint8_t continuationCount = 0U;
        if ((first & 0xE0U) == 0xC0U)
        {
            codePoint = first & 0x1FU;
            continuationCount = 1U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            codePoint = first & 0x0FU;
            continuationCount = 2U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            codePoint = first & 0x07U;
            continuationCount = 3U;
        }
        else
            return U'?';

        for (uint8_t index = 0U; index < continuationCount; ++index)
        {
            const auto continuation = static_cast<uint8_t>(*text);
            if ((continuation & 0xC0U) != 0x80U)
                return U'?';
            ++text;
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }
        return static_cast<char32_t>(codePoint);
    }

    void drawGlyph(
        uint8_t* const frameBuffer,
        const BitmapGlyph& glyph,
        const uint8_t* const bitmap,
        const uint32_t fontHeight,
        const uint32_t scale,
        const uint32_t originX,
        const uint32_t originY,
        const uint16_t color,
        const Rect& clip)
    {
        const uint32_t bytesPerRow = glyph.width / 8U;
        for (uint32_t row = 0U; row < fontHeight; ++row)
        {
            for (uint32_t column = 0U; column < glyph.width; ++column)
            {
                const uint8_t bits = bitmap[row * bytesPerRow + column / 8U];
                if ((bits & (0x80U >> (column % 8U))) == 0U)
                    continue;

                for (uint32_t scaleY = 0U; scaleY < scale; ++scaleY)
                {
                    for (uint32_t scaleX = 0U; scaleX < scale; ++scaleX)
                    {
                        const uint32_t x = originX + column * scale + scaleX;
                        const uint32_t y = originY + row * scale + scaleY;
                        if (x >= static_cast<uint32_t>(clip.position.x)
                            && y >= static_cast<uint32_t>(clip.position.y)
                            && x < static_cast<uint32_t>(clip.position.x)
                                   + clip.size.width
                            && y < static_cast<uint32_t>(clip.position.y)
                                   + clip.size.height)
                            setPixel(frameBuffer, x, y, color);
                    }
                }
            }
        }
    }

    const BitmapGlyph* findTextGlyph(
        const char32_t codePoint,
        const uint8_t*& bitmap)
    {
        const BitmapGlyph* glyph = NothingFont::findGlyph(codePoint);
        bitmap = glyph == nullptr ? nullptr : NothingFont::getBitmap(*glyph);
        if (glyph == nullptr)
        {
            glyph = MatrixSansFont::findGlyph(codePoint);
            if (glyph != nullptr)
                bitmap = MatrixSansFont::getBitmap(*glyph);
        }
        if (glyph == nullptr)
        {
            glyph = BitmapFont::findGlyph(codePoint);
            if (glyph != nullptr)
                bitmap = BitmapFont::getBitmap(*glyph);
        }
        if (glyph == nullptr)
        {
            glyph = BitmapFont::findGlyph(U'?');
            if (glyph != nullptr)
                bitmap = BitmapFont::getBitmap(*glyph);
        }
        return glyph;
    }

    void drawClock(
        uint8_t* const frameBuffer,
        const Rect& bounds,
        const char* text,
        const uint8_t scale)
    {
        uint32_t cursorX = static_cast<uint32_t>(bounds.position.x);
        const uint32_t cursorY = static_cast<uint32_t>(bounds.position.y);
        while (*text != '\0')
        {
            const char32_t codePoint = decodeUtf8(text);
            const uint8_t* bitmap = nullptr;
            const BitmapGlyph* const glyph = findTextGlyph(codePoint, bitmap);
            if (glyph == nullptr)
                return;

            drawGlyph(
                frameBuffer,
                *glyph,
                bitmap,
                NothingFont::height,
                scale,
                cursorX + 1U,
                cursorY + 1U,
                clockShadowColor,
                bounds);
            drawGlyph(
                frameBuffer,
                *glyph,
                bitmap,
                NothingFont::height,
                scale,
                cursorX,
                cursorY,
                textColor,
                bounds);
            cursorX += static_cast<uint32_t>(glyph->width) * scale + scale;
        }
    }

    void drawIcon(
        uint8_t* const frameBuffer,
        const PixelartIcon icon,
        const uint32_t originX,
        const uint32_t originY)
    {
        const uint8_t* const bitmap = PixelartIcons::getBitmap(icon);
        if (bitmap == nullptr)
            return;

        for (uint32_t row = 0U; row < PixelartIcons::height; ++row)
        {
            for (uint32_t column = 0U; column < PixelartIcons::width; ++column)
            {
                const uint8_t bits = bitmap[
                    row * PixelartIcons::bytesPerRow + column / 8U];
                if ((bits & (0x80U >> (column % 8U))) != 0U)
                    setPixel(
                        frameBuffer,
                        originX + column,
                        originY + row,
                        textColor);
            }
        }
    }

    bool extensionEquals(const char* extension, const char* expected)
    {
        while (*extension != '\0' && *expected != '\0')
        {
            char left = *extension++;
            char right = *expected++;
            if (left >= 'A' && left <= 'Z')
                left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z')
                right = static_cast<char>(right - 'A' + 'a');
            if (left != right)
                return false;
        }
        return *extension == '\0' && *expected == '\0';
    }

    PixelartIcon getFileIcon(const FILINFO& fileInformation)
    {
        if ((fileInformation.fattrib & AM_DIR) != 0U)
            return PixelartIcon::FOLDER;

        const char* const extension = std::strrchr(fileInformation.fname, '.');
        if (extension == nullptr)
            return PixelartIcon::FILE;

        if (extensionEquals(extension, ".bmp")
            || extensionEquals(extension, ".jpg")
            || extensionEquals(extension, ".jpeg")
            || extensionEquals(extension, ".png"))
            return PixelartIcon::IMAGE;

        if (extensionEquals(extension, ".avi")
            || extensionEquals(extension, ".mov")
            || extensionEquals(extension, ".mp4"))
            return PixelartIcon::VIDEO;

        return PixelartIcon::FILE;
    }

    bool beginTextFrame()
    {
        if (!displayInitializationSucceeded)
            return false;

        /* Draw directly into the active buffer until the display path is
           verified on hardware. LTDC reads this memory continuously. */
        textFrameBuffer = displayLowLevel.getFrontBuffer();
        clearFrameBuffer(textFrameBuffer, backgroundColor);
        textCursorY = textMargin;
        return true;
    }

    void showTextFrame()
    {
        if (textFrameBuffer == nullptr)
            return;

        baseFrame.capture(textFrameBuffer);
        baseFrame.restoreAll(displayLowLevel.getBackBuffer());
        textFrameBuffer = nullptr;
        displayLowLevel.refresh();
    }

    void showNoCardMessage()
    {
        if (!beginTextFrame())
            return;

        outputFileName("Вставьте SD карту");
        showTextFrame();
    }

    const char* getSdStatusText(const CardInitError error)
    {
        switch (error)
        {
            case CardInitError::NONE:
                return "SD: OK";
            case CardInitError::COMMAND_FAILED:
                return "SD: command failed";
            case CardInitError::INVALID_INTERFACE_CONDITION:
                return "SD: invalid interface condition";
            case CardInitError::POWER_UP_TIMEOUT:
                return "SD: power-up timeout";
            case CardInitError::UNSUPPORTED_CARD:
                return "SD: unsupported card";
            default:
                return "SD: unknown error";
        }
    }

    const char* getFailedCommandText(const uint8_t command)
    {
        switch (command)
        {
            case 0U:
                return "SD: CMD0 failed";
            case 2U:
                return "SD: CMD2 failed";
            case 3U:
                return "SD: CMD3 failed";
            case 6U:
                return "SD: ACMD6 failed";
            case 7U:
                return "SD: CMD7 failed";
            case 8U:
                return "SD: CMD8 failed";
            case 9U:
                return "SD: CMD9 failed";
            case 16U:
                return "SD: CMD16 failed";
            case 41U:
                return "SD: ACMD41 failed";
            case 55U:
                return "SD: CMD55 failed";
            default:
                return "SD: command failed";
        }
    }

    const char* getFatFsStatusText(const FRESULT result)
    {
        switch (result)
        {
            case FR_OK:
                return "FatFs: OK";
            case FR_DISK_ERR:
                return "FatFs: disk error";
            case FR_NOT_READY:
                return "FatFs: card not ready";
            case FR_NO_FILESYSTEM:
                return "FatFs: no filesystem";
            case FR_NOT_ENOUGH_CORE:
                return "FatFs: not enough memory";
            default:
                return "FatFs: mount error";
        }
    }

    const char* getImageStatusText(const ImageDecodeResult result)
    {
        switch (result)
        {
            case ImageDecodeResult::OK:
                return "Image: OK";
            case ImageDecodeResult::FILE_OPEN_FAILED:
                return "Image: file not found";
            case ImageDecodeResult::FILE_READ_FAILED:
                return "Image: read error";
            case ImageDecodeResult::INVALID_FORMAT:
                return "Image: invalid file";
            case ImageDecodeResult::UNSUPPORTED_FORMAT:
                return "Image: unsupported format";
            case ImageDecodeResult::INSUFFICIENT_MEMORY:
                return "Image: not enough memory";
            case ImageDecodeResult::IMAGE_TOO_LARGE:
                return "Image: image too large";
            case ImageDecodeResult::DECOMPRESSION_FAILED:
            default:
                return "Image: decode error";
        }
    }

}


int main()
{
    taskDispatcherEventQueue = xQueueCreate(
        8U,
        sizeof(TaskDispatcherEvent));
    if (taskDispatcherEventQueue == nullptr)
        return 1;

    xTaskCreate(
        taskDispatcher,
        "dispatcher",
        configMINIMAL_STACK_SIZE,
        nullptr,
        1U,
        &taskDispatcherHandle);

    vTaskStartScheduler();
    return 0;
}

void taskDispatcher(void* parameters)
{
    static_cast<void>(parameters);

    cardLowLevel.configureCardDetectInterrupt(
        configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    displayInitializationSucceeded = displayLowLevel.enable();
    if (displayInitializationSucceeded)
    {
        const BaseType_t clockTaskCreated = xTaskCreate(
            clockTask,
            "clock",
            configMINIMAL_STACK_SIZE,
            nullptr,
            0U,
            &clockTaskHandle);
        if (clockTaskCreated == pdPASS)
            RtcClock::configureSecondInterrupt(
                configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    }

    TaskDispatcherEvent event = cardLowLevel.isCardDetected()
                              ? TaskDispatcherEvent::CARD_DETECTED
                              : TaskDispatcherEvent::CARD_REMOVED;

    while (true)
    {
        switch (event)
        {
            case TaskDispatcherEvent::CARD_DETECTED:
            {
                if (!card && cardInitTaskHandle == nullptr)
                {
                    if (!card)
                        card.emplace(
                            IndicateColor::GREEN,
                            IndicateColor::ORANGE);
                    xTaskCreate(
                        cardInitTask,
                        "cardInit",
                        configMINIMAL_STACK_SIZE * 2U,
                        nullptr,
                        1U,
                        &cardInitTaskHandle);
                }
                break;
            }
            case TaskDispatcherEvent::CARD_REMOVED:
                if (cardDeinitTaskHandle == nullptr)
                    xTaskCreate(
                        cardDeinitTask,
                        "cardDeinit",
                        configMINIMAL_STACK_SIZE,
                        nullptr,
                        1U,
                        &cardDeinitTaskHandle);
                break;
            case TaskDispatcherEvent::USB_ON:
            case TaskDispatcherEvent::USB_OFF:
                break;
            case TaskDispatcherEvent::DISPLAY_ON:
                static_cast<void>(displayLowLevel.enable());
                break;
            case TaskDispatcherEvent::TOUCHSCREEN_ON:
                // Touchscreen initialization will be added with its driver.
                break;
            case TaskDispatcherEvent::CLOCK_TICK:
                if (clockTaskHandle != nullptr)
                    xTaskNotifyGive(clockTaskHandle);
                break;
        }

        xQueueReceive(taskDispatcherEventQueue, &event, portMAX_DELAY);
    }
}

void cardDeinitTask(void* parameters)
{
    static_cast<void>(parameters);

    vTaskDelay(pdMS_TO_TICKS(50U));
    if (cardLowLevel.isCardDetected())
    {
        cardDeinitTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    while (cardInitTaskHandle != nullptr
           || rootDirectoryTaskHandle != nullptr)
    {
        vTaskDelay(pdMS_TO_TICKS(10U));
        if (cardLowLevel.isCardDetected())
        {
            cardDeinitTaskHandle = nullptr;
            vTaskDelete(nullptr);
            return;
        }
    }

    card.reset();
    fileSystemMountResult = FR_NOT_READY;
    cardInitError = CardInitError::UNSUPPORTED_CARD;
    failedCardCommand = 0xFFU;
    showNoCardMessage();

    cardDeinitTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void cardInitTask(void* parameters)
{
    static_cast<void>(parameters);

    /* Debounce Card Detect and allow the card power/clock domain to settle.
       Previously this interval occurred accidentally while display refresh
       was polled synchronously. */
    vTaskDelay(pdMS_TO_TICKS(100U));

    if (!card)
    {
        cardInitTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    const bool initialized = card->initialize(cardLowLevel);
    cardInitError = card->getInitializationError();
    failedCardCommand = card->getFailedCommand();

    if (!cardLowLevel.isCardDetected())
        card.reset();

    const bool cardReady = initialized && card.has_value();
    fileSystemMountResult = cardReady
                          ? card->getFileSystemResult()
                          : FR_NOT_READY;

    if (!cardReady && beginTextFrame())
    {
        if (cardInitError != CardInitError::NONE)
            outputFileName(
                cardInitError == CardInitError::COMMAND_FAILED
                    ? getFailedCommandText(failedCardCommand)
                    : getSdStatusText(cardInitError));
        if (fileSystemMountResult != FR_OK)
            outputFileName(getFatFsStatusText(fileSystemMountResult));
        showTextFrame();
    }

    if (cardReady && rootDirectoryTaskHandle == nullptr)
    {
        const BaseType_t taskCreated = xTaskCreate(
            rootDirectoryTask,
            "rootDirectory",
            configMINIMAL_STACK_SIZE * 2U,
            nullptr,
            1U,
            &rootDirectoryTaskHandle);

        if (taskCreated != pdPASS)
        {
            rootDirectoryTaskHandle = nullptr;
            if (beginTextFrame())
            {
                outputFileName(getFatFsStatusText(FR_NOT_ENOUGH_CORE));
                showTextFrame();
            }
        }
    }

    cardInitTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void rootDirectoryTask(void* parameters)
{
    static_cast<void>(parameters);

    ImageDecodeResult imageResult = ImageDecodeResult::FILE_OPEN_FAILED;
    if (fileSystemMountResult == FR_OK && displayInitializationSucceeded)
    {
        if (!calculateDisplayLayout())
            imageResult = ImageDecodeResult::DECOMPRESSION_FAILED;

        const Container& imageContainer = getDisplayContainer(
            DisplayContainerIndex::IMAGE);
        uint8_t* const completeFrameBuffer = displayLowLevel.getBackBuffer();
        const uint32_t imageOffset =
            (static_cast<uint32_t>(imageContainer.visibleBounds.position.y)
             * displayWidth
             + static_cast<uint32_t>(imageContainer.visibleBounds.position.x))
            * displayBytesPerPixel;
        uint8_t* const imageFrameBuffer = completeFrameBuffer + imageOffset;
        if (imageResult != ImageDecodeResult::DECOMPRESSION_FAILED)
            imageResult = imageLoader.draw(
                ImageFileType::JPEG,
                "0:/cat.jpg",
                imageFrameBuffer,
                imageContainer.visibleBounds.size.width,
                imageContainer.visibleBounds.size.height,
                displayWidth,
                finishImageRead);
        if (imageResult == ImageDecodeResult::OK)
        {
            baseFrame.capture(completeFrameBuffer);
            displayLowLevel.swapBuffers();
            baseFrame.restoreAll(displayLowLevel.getBackBuffer());
            rootDirectoryTaskHandle = nullptr;
            vTaskDelete(nullptr);
            return;
        }
    }

    if (beginTextFrame())
    {
        if (cardInitError != CardInitError::NONE)
            outputFileName(
                cardInitError == CardInitError::COMMAND_FAILED
                    ? getFailedCommandText(failedCardCommand)
                    : getSdStatusText(cardInitError));
        if (fileSystemMountResult != FR_OK)
            outputFileName(getFatFsStatusText(fileSystemMountResult));
        else
            outputFileName(getImageStatusText(imageResult));
    }

    DIR directory;
    FILINFO fileInfo;
    FRESULT directoryResult = fileSystemMountResult == FR_OK
                            ? card->openCurrentDirectory(directory)
                            : fileSystemMountResult;

    if (directoryResult == FR_OK
        && std::strcmp(card->getCurrentDirectoryPath(), "0:/") != 0)
        outputFileName("..", PixelartIcon::UP);

    while (directoryResult == FR_OK)
    {
        directoryResult = card->readDirectoryEntry(directory, fileInfo);
        if (directoryResult != FR_OK || fileInfo.fname[0] == '\0')
            break;

        outputFileName(fileInfo.fname, getFileIcon(fileInfo));
    }

    if (directoryResult == FR_OK)
        directoryResult = card->closeDirectory(directory);

    if (card && directoryResult == FR_OK)
        card->finishDataExchange();

    if (directoryResult != FR_OK
        && directoryResult != fileSystemMountResult)
        outputFileName(getFatFsStatusText(directoryResult));

    showTextFrame();

    if (!cardLowLevel.isCardDetected())
        card.reset();

    rootDirectoryTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void clockTask(void* parameters)
{
    static_cast<void>(parameters);

    TextLayoutCalculator textCalculator(
        NothingFont::height,
        NothingFont::findGlyph,
        MatrixSansFont::findGlyph);
    const TextMeasurement measurement = textCalculator.measure(
        "00:00",
        clockRequestedHeight);
    Container& clockContainer = getDisplayContainer(DisplayContainerIndex::CLOCK);
    clockContainer.size = measurement.size;
    static_cast<void>(calculateDisplayLayout());

    Rect previousBounds{};
    uint8_t previousHour = 0xFFU;
    uint8_t previousMinute = 0xFFU;
    uint32_t previousBaseVersion = 0U;
    while (true)
    {
        static_cast<void>(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
        if (baseFrame.isValid())
        {
            const RtcClock::Time time = RtcClock::readTime();
            const uint32_t baseVersion = baseFrame.getVersion();
            if (time.hour != previousHour
                || time.minute != previousMinute
                || baseVersion != previousBaseVersion)
            {
                char timeText[] =
                {
                    static_cast<char>('0' + time.hour / 10U),
                    static_cast<char>('0' + time.hour % 10U),
                    ':',
                    static_cast<char>('0' + time.minute / 10U),
                    static_cast<char>('0' + time.minute % 10U),
                    '\0'
                };
                const Rect dirtyArea = LayoutCalculator::unite(
                    previousBounds,
                    clockContainer.visibleBounds);

                vTaskSuspendAll();
                uint8_t* const frameBuffer = displayLowLevel.getBackBuffer();
                baseFrame.restore(frameBuffer, dirtyArea);
                drawClock(
                    frameBuffer,
                    clockContainer.visibleBounds,
                    timeText,
                    measurement.scale);
                displayLowLevel.swapBuffers();
                static_cast<void>(xTaskResumeAll());

                previousBounds = clockContainer.visibleBounds;
                previousHour = time.hour;
                previousMinute = time.minute;
                previousBaseVersion = baseVersion;
            }
        }

    }
}

void outputFileName(
    const char* fileName,
    const PixelartIcon icon)
{
    if (fileName == nullptr
        || textFrameBuffer == nullptr
        || textCursorY + NothingFont::height * textScale > displayHeight)
        return;

    uint32_t cursorX = textMargin;
    if (icon != PixelartIcon::NONE)
    {
        constexpr uint32_t iconSpacing = 8U;
        const uint32_t iconY = textCursorY
                             + (NothingFont::height * textScale
                                - PixelartIcons::height) / 2U;
        drawIcon(textFrameBuffer, icon, cursorX, iconY);
        cursorX += PixelartIcons::width + iconSpacing;
    }
    const char* current = fileName;
    while (*current != '\0')
    {
        const char32_t codePoint = decodeUtf8(current);
        const uint8_t* bitmap = nullptr;
        const BitmapGlyph* const glyph = findTextGlyph(codePoint, bitmap);
        if (glyph == nullptr)
            break;

        const uint32_t glyphWidth = glyph->width * textScale;
        if (cursorX + glyphWidth > displayWidth - textMargin)
            break;

        drawGlyph(
            textFrameBuffer,
            *glyph,
            bitmap,
            NothingFont::height,
            textScale,
            cursorX,
            textCursorY,
            textColor,
            {{0, 0}, {displayWidth, displayHeight}});
        cursorX += glyphWidth;
    }
    textCursorY += NothingFont::height * textScale;
}

extern "C" void EXTI2_IRQHandler()
{
    cardLowLevel.clearCardDetectInterrupt();

    if (taskDispatcherHandle == nullptr
        || taskDispatcherEventQueue == nullptr)
        return;

    const TaskDispatcherEvent event = cardLowLevel.isCardDetected()
                                    ? TaskDispatcherEvent::CARD_DETECTED
                                    : TaskDispatcherEvent::CARD_REMOVED;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(
        taskDispatcherEventQueue,
        &event,
        &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

extern "C" void RTC_Alarm_IRQHandler()
{
    RtcClock::clearSecondInterrupt();

    if (taskDispatcherHandle == nullptr
        || taskDispatcherEventQueue == nullptr)
        return;

    const TaskDispatcherEvent event = TaskDispatcherEvent::CLOCK_TICK;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(
        taskDispatcherEventQueue,
        &event,
        &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
