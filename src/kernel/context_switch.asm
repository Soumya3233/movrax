# ============================================================================
# MOVRAX Pure Assembly Context Switch
# Deterministic, compiler-independent process switching
# ============================================================================
# 
# void switch_context(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3)
#
# This function:
#   1. Saves callee-saved registers on current stack
#   2. Saves current ESP to *old_esp
#   3. Switches CR3 if different (address space isolation)
#   4. Loads new ESP
#   5. Restores callee-saved registers from new stack
#   6. Returns (ret pops EIP from new stack → resumes new process)
#
# This is the STANDARD approach used by VxWorks, INTEGRITY, and Linux.
# It is deterministic: bounded cycle count, no compiler interference.
# ============================================================================

.section .text
.global switch_context
.type switch_context, @function

switch_context:
    # ---- Save current process state ----
    # Save callee-saved registers (System V i386 ABI)
    push %ebp
    push %ebx
    push %esi
    push %edi
    
    # Save current ESP into *old_esp
    # Stack layout at this point:
    #   [EDI] [ESI] [EBX] [EBP] [RET_ADDR] [old_esp_ptr] [new_esp] [new_cr3]
    #   ESP+0  +4    +8    +12   +16         +20            +24       +28
    mov 20(%esp), %eax      # eax = old_esp_ptr (first argument)
    mov %esp, (%eax)        # *old_esp_ptr = current ESP
    
    # ---- Switch address space (CR3) if different ----
    mov 28(%esp), %ecx      # ecx = new_cr3 (third argument)
    mov %cr3, %edx          # edx = current CR3
    cmp %ecx, %edx
    je .skip_cr3_switch     # Skip TLB flush if same address space
    mov %ecx, %cr3          # Load new page directory
.skip_cr3_switch:
    
    # ---- Switch to new process ----
    mov 24(%esp), %esp      # ESP = new_esp (second argument)
    
    # Restore callee-saved registers from new stack
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    
    # Return to new process
    # 'ret' pops EIP from new stack, jumping to where the new process
    # was suspended (its previous call to switch_context)
    ret

.size switch_context, . - switch_context

# ============================================================================
# First-time process entry trampoline
# 
# When a NEW process is scheduled for the first time, its stack has been
# set up by setup_kernel_stack() with a fake frame. The 'ret' in 
# switch_context will pop the entry_point address and jump to it.
#
# For kernel processes: entry_point runs directly.
# For user processes: we need to switch to Ring 3 via IRET.
# ============================================================================

.global process_entry_trampoline
.type process_entry_trampoline, @function

process_entry_trampoline:
    # Re-enable interrupts (they were disabled during context switch)
    sti
    
    # The entry point address was pushed onto the stack by setup_kernel_stack
    # It's now at the top of the stack
    ret     # pops entry point and jumps to it

.size process_entry_trampoline, . - process_entry_trampoline

# ============================================================================
# User-mode entry via IRET
# Used when creating Ring 3 processes
# Stack must be set up with: [SS] [User ESP] [EFLAGS] [CS] [EIP]
# ============================================================================

.global enter_usermode
.type enter_usermode, @function

enter_usermode:
    # Arguments on stack: entry_point, user_esp
    # We build an IRET frame
    
    mov 4(%esp), %ecx       # entry_point
    mov 8(%esp), %edx       # user_esp
    
    # Set data segments to User Data (0x20 | 3 = 0x23)
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    # Build IRET frame on stack
    push $0x23              # SS (User Data | RPL 3)
    push %edx               # User ESP
    pushf                   # EFLAGS
    orl $0x200, (%esp)      # Ensure IF (interrupt flag) is set
    push $0x1B              # CS (User Code | RPL 3 = 0x18 | 3)
    push %ecx               # EIP (entry point)
    
    iret                    # Switch to Ring 3

.size enter_usermode, . - enter_usermode
