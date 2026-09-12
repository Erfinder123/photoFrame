
CC := arm-none-eabi-gcc
CXX := arm-none-eabi-g++
OBJCOPY := arm-none-eabi-objcopy

SHAREDFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
INCLUDE_DIRS := $(shell find . -type d)
INC_FLAGS := $(foreach dir, $(INCLUDE_DIRS), -I$(dir))

BASEFLAGS := -c $(SHAREDFLAGS) -Wall -Wextra -MMD -MP $(INC_FLAGS)
CFLAGS := $(BASEFLAGS) -std=gnu17
CXXFLAGS := $(BASEFLAGS) -std=gnu++20 -fno-exceptions -fno-rtti

LDFLAGS := $(SHAREDFLAGS) -nostartfiles -specs=nosys.specs -T stm32_linker.ld

BUILDDIR := build/$(BUILD)
ELFFILE := $(BUILDDIR)/photoFrame.elf

ifeq ($(BUILD),debug)
    CFLAGS += -DDEBUG -Og -g3
    CXXFLAGS += -DDEBUG -Og -g3
    LDFLAGS += -Wl,-Map=$(BUILDDIR)/photoFrame.map
else
    CFLAGS += -DNDEBUG -Os
    CXXFLAGS += -DNDEBUG -Os
endif

C_SOURCES := $(shell find . -name '*.c' | sed 's|^\./||')
CPP_SOURCES := $(shell find . -name '*.cpp' | sed 's|^\./||')
OBJECTS := $(patsubst %.c,objects/$(BUILD)/%.o,$(C_SOURCES)) \
		   $(patsubst %.cpp,objects/$(BUILD)/%.o,$(CPP_SOURCES))

.PHONY: all release clean
.DEFAULT_GOAL := all

-include $(OBJECTS:.o=.d)

all:
	$(MAKE) BUILD=debug build/debug/photoFrame.elf
release:
	$(MAKE) BUILD=release build/release/photoFrame.hex build/release/photoFrame.bin
	rm build/release/photoFrame.elf

$(BUILDDIR)/photoFrame.hex: $(ELFFILE)
	$(OBJCOPY) -O ihex $< $@

$(BUILDDIR)/%.bin: $(ELFFILE)
	$(OBJCOPY) -O binary $< $@

$(ELFFILE): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

objects/$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

objects/$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	find . -type f \( -name "*.o" -o -name "*.d" -o -name "*.elf" -o -name "*.hex" -o -name "*.bin" \) -delete
