/* boot.asm - Multiboot header and entry point */

/* Multiboot header constants */
.set ALIGN,    1<<0                 /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1                 /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO      /* Multiboot 'flag' field */
.set MAGIC,    0x1BADB002           /* magic number for bootloader to find header */
.set CHECKSUM, -(MAGIC + FLAGS)     /* checksum to prove we are multiboot */

/* Entry point - multiboot header at very start of .text */
.section .text
.align 4

/* Multiboot header - MUST be in first 8KB of kernel file */
multiboot_header:
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Global entry point */
.global _start
.type _start, @function
_start:
    /* Set up stack pointer */
    mov $stack_top, %esp
    
    /* Reset EFLAGS */
    pushl $0
    popf
    
    /* Push Multiboot info pointer (EBX) as argument to kernel_main */
    /* EAX = magic number, EBX = multiboot info pointer */
    push %ebx
    
    /* Call kernel main function (C++) */
    call kernel_main
    
    /* Clean up stack */
    add $4, %esp
    
    /* If kernel_main returns, halt the CPU */
    cli
1:  hlt
    jmp 1b

/* Set size of _start symbol for debugging */
.size _start, . - _start

/* Reserve stack space */
.section .bss
.align 16
stack_bottom:
.skip 32768  /* 32 KB stack */
stack_top:
