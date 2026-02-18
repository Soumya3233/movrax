# Makefile for MOVRAX - Military Persistent Memory OS

# Compiler and tools
AS = i686-elf-as
CXX = i686-elf-g++

# Directories
SRC_DIR = src
BUILD_DIR = build
BOOT_DIR = $(SRC_DIR)/boot
KERNEL_DIR = $(SRC_DIR)/kernel
INCLUDE_DIR = $(SRC_DIR)/include

# Flags (hardened for military-grade)
CXXFLAGS = -ffreestanding -O2 -Wall -Wextra \
           -fno-exceptions -fno-rtti \
           -fno-delete-null-pointer-checks \
           -fno-strict-aliasing \
           -I$(INCLUDE_DIR) -I$(KERNEL_DIR)
LDFLAGS = -T linker.d -nostdlib -ffreestanding -lgcc

# Source files
BOOT_ASM = $(BOOT_DIR)/boot.asm
ISR_ASM = $(KERNEL_DIR)/isr.asm
CTX_SWITCH_ASM = $(KERNEL_DIR)/context_switch.asm
CPP_SRC = $(wildcard $(KERNEL_DIR)/*.cpp)

# Object files
BOOT_OBJ = $(BUILD_DIR)/boot.o
ISR_OBJ = $(BUILD_DIR)/isr.o
CTX_SWITCH_OBJ = $(BUILD_DIR)/context_switch.o
CPP_OBJ = $(patsubst $(KERNEL_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CPP_SRC))
OBJ = $(BOOT_OBJ) $(ISR_OBJ) $(CTX_SWITCH_OBJ) $(CPP_OBJ)

# Output
KERNEL_BIN = kernel.bin
ISO_FILE = mini-os.iso

# Default target
all: $(ISO_FILE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble boot code (GAS format)
$(BUILD_DIR)/boot.o: $(BOOT_ASM) | $(BUILD_DIR)
	$(AS) $< -o $@

# Assemble ISR stubs (GAS format)
$(BUILD_DIR)/isr.o: $(ISR_ASM) | $(BUILD_DIR)
	$(AS) $< -o $@

# Assemble context switch (GAS format)
$(BUILD_DIR)/context_switch.o: $(CTX_SWITCH_ASM) | $(BUILD_DIR)
	$(AS) $< -o $@

# Compile C++ files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link kernel (using g++ as linker driver to find libgcc)
$(KERNEL_BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ)

# Create bootable ISO
$(ISO_FILE): $(KERNEL_BIN)
	mkdir -p isodir/boot/grub
	cp $(KERNEL_BIN) isodir/boot/
	cp $(BOOT_DIR)/grub.cfg isodir/boot/grub/
	grub-mkrescue -o $(ISO_FILE) isodir

# Run in QEMU with 128MB RAM (copy to /tmp to avoid WSL path issues)
run: $(ISO_FILE)
	cp $(ISO_FILE) /tmp/mini-os.iso
	qemu-system-i386 -cdrom /tmp/mini-os.iso -m 128 -accel tcg,thread=single

# Run in QEMU with debugging enabled
debug: $(ISO_FILE)
	cp $(ISO_FILE) /tmp/mini-os.iso
	qemu-system-i386 -cdrom /tmp/mini-os.iso -m 128 -s -S

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) isodir $(KERNEL_BIN) $(ISO_FILE)

.PHONY: all run debug clean