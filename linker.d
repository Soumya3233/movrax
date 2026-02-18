/* Linker script for the kernel */

ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 1MB physical address */
    . = 1M;

    /* Text section must come first with multiboot header inside */
    .text ALIGN(4) : {
        /* Force boot.o to be first - contains multiboot header */
        *boot.o(.text)
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }

    /* C++ static constructors and destructors */
    .init_array ALIGN(4) : {
        __init_array_start = .;
        *(.init_array)
        __init_array_end = .;
    }

    .fini_array ALIGN(4) : {
        __fini_array_start = .;
        *(.fini_array)
        __fini_array_end = .;
    }

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
