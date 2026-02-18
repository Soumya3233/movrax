# ISR (Interrupt Service Routine) stubs
# GAS syntax version

.section .text
.extern exception_handler
.extern timer_callback
.extern pic_eoi_helper

# Common ISR stub
# Logic: Save all registers, call C handler, restore registers
isr_common_stub:
    pusha                   # Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax
    
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    movw $0x10, %ax         # Load Kernel Data Segment
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    movl %esp, %eax         # Push pointer to stack (Context*)
    pushl %eax
    
    # Call handler (address is on stack from specific stub logic if needed, 
    # but for now we dispatch based on interrupt number)
    
    # Clean up (this part is specific to how we want to dispatch)
    # We'll use specific stubs for now to keep it simple
    
    popl %eax
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    
    popa
    addl $8, %esp           # clean up error code and ISR number
    iret

# --- EXCEPTION HANDLERS ---

.global isr_page_fault
isr_page_fault:
    pushl $14               # Exception Number
    jmp exception_common_stub

exception_common_stub:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Pass pointer to stack as argument
    # We need to construct arguments: (exception_num, error_code, eip)
    # Stack layout at this point:
    # [gs, fs, es, ds] (16 bytes)
    # [edi, esi, ebp, esp, ebx, edx, ecx, eax] (32 bytes)
    # [exception_num] (4 bytes)
    # [error_code] (4 bytes)  <-- Pushed by CPU
    # [eip] (4 bytes)         <-- Pushed by CPU
    
    # Calculate offsets relative to ESP
    movl 48(%esp), %ecx     # exception_num
    movl 52(%esp), %eax     # error_code
    movl 56(%esp), %edx     # eip
    
    pushl %edx
    pushl %eax
    pushl %ecx
    
    call exception_handler
    
    addl $12, %esp          # clean up args
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp           # pop error code and exception number
    iret

# --- IRQ HANDLERS ---

.global irq_timer
irq_timer:
    pushl $0                # Error code (dummy)
    pushl $32               # INT number (IRQ 0 + 32)
    jmp irq_common_stub

irq_common_stub:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    movl %esp, %eax         # Push Context* 
    pushl %eax
    
    call timer_callback     # We verify if it is timer inside
    
    popl %eax               # Pop Context*
    
    # Send EOI (End of Interrupt) to PIC
    # Check if we need to send to secondary PIC (IRQ >= 8)
    # But since we only have wrapper for Timer (IRQ 0), just send to Primary
    
    movb $0x20, %al
    outb %al, $0x20
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp           # pop error code and int number
    iret

.global isr_syscall
isr_syscall:
    pushl $0                # Dummy error code
    pushl $0x80             # Interrupt number
    
    # Use common irq stub logic BUT call different handler
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    movl %esp, %eax
    pushl %eax
    
    call syscall_callback
    
    popl %eax
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

# --- GDT Helper Functions ---

.global gdt_flush
gdt_flush:
    mov 4(%esp), %eax      # Get GDT pointer
    lgdt (%eax)            # Load GDT
    
    mov $0x10, %ax         # Load Kernel Data Segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    # Far jump to flush CS
    ljmp $0x08, $.flush
.flush:
    ret

.global tss_flush
tss_flush:
    mov $0x28, %ax         # Load TSS Segment (Index 5 * 8 = 40 = 0x28)
    ltr %ax                # Load Task Register
    ret

