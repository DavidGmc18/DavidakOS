ENTRY(entry)

STACK_SIZE = 4K;

SECTIONS {
    . = 0x80200000;
    __kernel = .;

    .text :{
        KEEP(*(.text.entry));
        *(.text .text.*);
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    }

    .data : ALIGN(4) {
        *(.data .data.*);
    }

    .bss : ALIGN(4) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    . = ALIGN(4);
    __stack = .;
    . += STACK_SIZE;
    __stack_top = .;
    
    __kernel_end = .;
}