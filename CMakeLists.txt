cmake_minimum_required(VERSION 4.1)

project(photoFrame LANGUAGES C CXX)

set(
    CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_EXTENSIONS ON)

file(
    GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "*.c"
    "*.cpp"
)

file(
    GLOB_RECURSE TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/*.c"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
)

list(REMOVE_ITEM SOURCES ${TEST_SOURCES})
list(FILTER SOURCES EXCLUDE REGEX "(.*/build/.*|.*/objects/.*|.*/.idea/.*|.*/.git/.*)")


add_executable(
    photoFrame.elf
    ${SOURCES}
)

target_compile_options(
    photoFrame.elf
    PRIVATE

    ${CPU_FLAGS}

    -Wall
    -Wextra

    $<$<COMPILE_LANGUAGE:CXX>:
    -fno-exceptions
    -fno-rtti
    -fno-use-cxa-atexit
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    >
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(
        photoFrame.elf
        PRIVATE
        DEBUG
    )

    target_compile_options(
        photoFrame.elf
        PRIVATE
        -Og
        -g3
    )
else()
    target_compile_definitions(
        photoFrame.elf
        PRIVATE
        NDEBUG
    )

    target_compile_options(
        photoFrame.elf
        PRIVATE
        -Os
    )
endif()


file(
    GLOB_RECURSE INCLUDE_DIRS
    LIST_DIRECTORIES true
    ${CMAKE_SOURCE_DIR}/*
)

list(
    FILTER INCLUDE_DIRS EXCLUDE REGEX
    "(.*/build(/.*)?|.*/objects(/.*)?|.*/tests(/.*)?|.*/.idea(/.*)?|.*/.git(/.*)?)"
)

foreach(dir ${INCLUDE_DIRS})
    if(IS_DIRECTORY ${dir})
        target_include_directories(
            photoFrame.elf
            PRIVATE
            ${dir}
        )
    endif()
endforeach()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(
        photoFrame.elf
        PRIVATE
        -Wl,-Map=${CMAKE_BINARY_DIR}/photoFrame.map
    )
endif()


set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/stm32_linker.ld)

target_link_options(
    photoFrame.elf
    PRIVATE

    ${CPU_FLAGS}

    -nostartfiles
    -specs=nosys.specs

    -T${LINKER_SCRIPT}
)

set_target_properties(
        photoFrame.elf
        PROPERTIES
        LINK_DEPENDS ${LINKER_SCRIPT}
)


if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")

    add_custom_command(
        TARGET photoFrame.elf
        POST_BUILD

        COMMAND
        arm-none-eabi-objcopy
        -O ihex

        $<TARGET_FILE:photoFrame.elf>

        ${CMAKE_BINARY_DIR}/photoFrame.hex
    )


    add_custom_command(
        TARGET photoFrame.elf
        POST_BUILD

        COMMAND
        arm-none-eabi-objcopy
        -O binary

        $<TARGET_FILE:photoFrame.elf>

        ${CMAKE_BINARY_DIR}/photoFrame.bin
    )
endif()
