# ChanUX Makefile
# 
# This Makefile automates the build process for the ChanUX operating system kernel.
# It handles compilation of C and assembly files, linking, and creation of a bootable ISO image.

# ===== TOOLCHAIN CONFIGURATION =====
# Cross-compiler toolchain for i686 (32-bit x86) target
# Using specific i686-elf tools ensures proper freestanding compilation without host OS dependencies

# C compiler - i686-elf-gcc is a cross-compiler targeting 32-bit x86 ELF format
CC = i686-elf-gcc

# Assembler - NASM (Netwide Assembler) for x86 assembly code
AS = nasm

# Linker - i686-elf-ld links object files into the final kernel binary
LD = i686-elf-ld

# ===== COMPILATION FLAGS =====

# C compiler flags:
# -std=gnu99: Use GNU C99 standard (C99 with GNU extensions)
# -ffreestanding: Compile for a freestanding environment (no standard library)
# -O2: Optimization level 2 (good balance of speed and size)
# -Wall: Enable all common warnings
# -Wextra: Enable extra warning flags
# -I$(SRC_DIR)/include: Add include directory to header search path
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(SRC_DIR)/include

# Linker flags:
# -T src/arch/x86/linker.ld: Use custom linker script for kernel memory layout
# -nostdlib: Don't link against standard system libraries
LDFLAGS = -T src/arch/x86/linker.ld -nostdlib

# Assembler flags:
# -f elf32: Output 32-bit ELF object files
ASFLAGS = -f elf32

# ===== DIRECTORY STRUCTURE =====
# Define the project directory layout

# Source code root directory
SRC_DIR = src

# Build output directory (for intermediate files)
BUILD_DIR = build

# Architecture-specific directories
ARCH_DIR = $(SRC_DIR)/arch/x86

# Kernel subsystem directories
KERNEL_CORE_DIR = $(SRC_DIR)/kernel/core
KERNEL_MEM_DIR = $(SRC_DIR)/kernel/memory
KERNEL_PROC_DIR = $(SRC_DIR)/kernel/process
KERNEL_INT_DIR = $(SRC_DIR)/kernel/interrupt
KERNEL_IO_DIR = $(SRC_DIR)/kernel/io

# Library directory
LIB_DIR = $(SRC_DIR)/lib

# ===== OUTPUT FILES =====

# Final kernel binary output path
KERNEL = $(BUILD_DIR)/kernel.bin

# Bootable ISO image filename
ISO = chanux.iso

# ===== SOURCE FILE DISCOVERY =====
# Automatically find all source files using wildcard patterns

# Find all C source files in the new structure
C_SOURCES = $(wildcard $(ARCH_DIR)/cpu/*.c) \
            $(wildcard $(KERNEL_CORE_DIR)/*.c) \
            $(wildcard $(KERNEL_MEM_DIR)/*.c) \
            $(wildcard $(KERNEL_PROC_DIR)/*.c) \
            $(wildcard $(KERNEL_INT_DIR)/*.c) \
            $(wildcard $(KERNEL_IO_DIR)/*.c) \
            $(wildcard $(LIB_DIR)/*.c)

# Find all assembly source files in the new structure
ASM_SOURCES = $(wildcard $(ARCH_DIR)/boot/*.asm) \
              $(wildcard $(ARCH_DIR)/asm/*.asm)

# ===== OBJECT FILE GENERATION =====
# Transform source file paths to object file paths in build directory

# Convert C source paths to object file paths
# Example: src/kernel/main.c -> build/kernel/main.o
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))

# Convert assembly source paths to object file paths
# Example: src/boot/boot.asm -> build/boot/boot.o
ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES))

# Combine all object files
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# ===== BUILD TARGETS =====

# Default target - builds the kernel binary
all: $(KERNEL)

# Create build directory structure
# The pipe (|) makes this an order-only prerequisite
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/arch/x86/boot $(BUILD_DIR)/arch/x86/cpu $(BUILD_DIR)/arch/x86/asm
	mkdir -p $(BUILD_DIR)/kernel/core $(BUILD_DIR)/kernel/memory $(BUILD_DIR)/kernel/process
	mkdir -p $(BUILD_DIR)/kernel/interrupt $(BUILD_DIR)/kernel/io
	mkdir -p $(BUILD_DIR)/lib

# Pattern rule for compiling C source files to object files
# $< is the first prerequisite (the .c file)
# $@ is the target (the .o file)
# | $(BUILD_DIR) ensures build directory exists but doesn't trigger rebuilds
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for assembling ASM source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Link all object files into the final kernel binary
# $^ expands to all prerequisites (all object files)
# $@ is the target (kernel.bin)
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Create bootable ISO image using GRUB
# This target creates a GRUB-bootable ISO with our kernel
iso: $(KERNEL)
	# Create ISO directory structure required by GRUB
	mkdir -p iso/boot/grub
	
	# Copy kernel binary to ISO boot directory
	cp $(KERNEL) iso/boot/kernel.bin
	
	# Create GRUB configuration file
	# This tells GRUB how to boot our kernel
	echo 'menuentry "ChanUX" {' > iso/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	
	# Use grub-mkrescue to create the final ISO image
	i686-elf-grub-mkrescue -o $(ISO) iso

# Run the OS in QEMU emulator
# Depends on ISO creation first
run: iso
	qemu-system-i386 -cdrom $(ISO)

# Run the OS in QEMU with debugging enabled
# -s: Enable GDB server on port 1234
# -S: Start CPU in stopped state (wait for GDB)
# & : Run QEMU in background
debug: iso
	qemu-system-i386 -cdrom $(ISO) -s -S &
	gdb -ex "target remote localhost:1234" -ex "symbol-file $(KERNEL)"

# Clean all build artifacts
# Removes build directory, ISO directory, and ISO file
clean:
	rm -rf $(BUILD_DIR) iso $(ISO)

# Declare phony targets (targets that don't create files with their names)
# This prevents conflicts if files named 'all', 'iso', etc. exist
.PHONY: all iso run debug clean