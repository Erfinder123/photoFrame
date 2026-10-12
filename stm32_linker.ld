
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2M
    SRAM (rxw) : ORIGIN = 0x20000000, LENGTH = 192K
    SDRAM (rw) : ORIGIN = 0xC0000000, LENGTH = 16M
}

_sstack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    .preinit_array (READONLY) :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array (READONLY) :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array (READONLY) :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _ldata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    .heap :
    {
        . = ALIGN(4);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = ALIGN(4);
    } > SRAM

    .framebuffer (NOLOAD) :
    {
        . = ALIGN(4);
        _sframebuffer_front = .;
        KEEP(*(.framebuffer))
        . = _sframebuffer_front + (800 * 480 * 2);
        _eframebuffer_front = .;
        _sframebuffer_back = .;
        . = _sframebuffer_back + (800 * 480 * 2);
        _eframebuffer_back = .;
        _eframebuffer = .;
    } > SDRAM

    .external_ram (NOLOAD) :
    {
        . = ALIGN(4);
        _sexternal_ram = .;
        *(.external_ram*)
        . = ALIGN(4);
        _eexternal_ram = .;
    } > SDRAM
}
