
#ifndef MAIN_HPP
#define MAIN_HPP

#include <cstdint>
#include "PixelartIcons.hpp"


enum class TaskDispatcherEvent : uint8_t
{
    CARD_DETECTED,
    CARD_REMOVED,
    USB_ON,
    USB_OFF,
    DISPLAY_ON,
    TOUCHSCREEN_ON,
    CLOCK_TICK
};

int main();
void taskDispatcher(void* parameters);
void cardInitTask(void* parameters);
void cardDeinitTask(void* parameters);
void rootDirectoryTask(void* parameters);
void clockTask(void* parameters);
void outputFileName(
    const char* fileName,
    PixelartIcon icon = PixelartIcon::NONE);


#endif //MAIN_HPP
