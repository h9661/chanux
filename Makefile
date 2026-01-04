# =============================================================================
# Chanux OS - Main Makefile
# =============================================================================
# Build system for the Chanux educational operating system.
#
# Usage:
#   make          - Build everything
#   make run      - Build and run in QEMU
#   make debug    - Build and run with GDB debugging
#   make clean    - Clean build artifacts
# =============================================================================

# =============================================================================
# Toolchain Configuration
# =============================================================================

# Cross-compiler (must be x86_64-elf target)
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy

# Assembler (NASM)
AS = nasm

# QEMU
QEMU = qemu-system-x86_64

# =============================================================================
# Compiler Flags
# =============================================================================

# C Compiler flags for kernel
CFLAGS = -ffreestanding \
         -fno-builtin \
         -fno-stack-protector \
         -fno-pic \
         -fno-pie \
         -nostdlib \
         -nostdinc \
         -mno-red-zone \
         -mno-mmx \
         -mno-sse \
         -mno-sse2 \
         -mcmodel=kernel \
         -Wall \
         -Wextra \
         -Werror \
         -std=c11 \
         -g \
         -O2 \
         -I kernel/include \
         -I kernel

# Linker flags
LDFLAGS = -T scripts/linker.ld \
          -nostdlib \
          -z max-page-size=4096

# Assembler flags for boot code (16-bit real mode, raw binary)
ASFLAGS_BOOT = -f bin -I boot/include/

# Assembler flags for kernel code (64-bit, ELF format)
ASFLAGS_KERNEL = -f elf64 -g -F dwarf

# =============================================================================
# Debug Configuration
# =============================================================================
# Usage:
#   make DEBUG=1          - Enable all debug output
#   make DEBUG_SCHED=1    - Enable only scheduler debug
#   make DEBUG=1 DEBUG_VMM=0 - Enable all except VMM debug

# Master debug flag
ifdef DEBUG
    CFLAGS += -DDEBUG
    ASFLAGS_KERNEL += -DDEBUG_CONTEXT=1
endif

# Subsystem-specific debug flags
ifdef DEBUG_SCHED
    CFLAGS += -DDEBUG_SCHED=$(DEBUG_SCHED)
endif
ifdef DEBUG_VMM
    CFLAGS += -DDEBUG_VMM=$(DEBUG_VMM)
endif
ifdef DEBUG_USER
    CFLAGS += -DDEBUG_USER=$(DEBUG_USER)
endif
ifdef DEBUG_SYSCALL
    CFLAGS += -DDEBUG_SYSCALL=$(DEBUG_SYSCALL)
endif
ifdef DEBUG_CONTEXT
    ASFLAGS_KERNEL += -DDEBUG_CONTEXT=$(DEBUG_CONTEXT)
endif

# =============================================================================
# Directories
# =============================================================================

BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel
USER_DIR = user

# =============================================================================
# Source Files
# =============================================================================

# Boot sources
BOOT_STAGE1 = $(BOOT_DIR)/stage1/mbr.asm
BOOT_STAGE2 = $(BOOT_DIR)/stage2/loader.asm

# Kernel assembly sources
KERNEL_ASM_SRCS = $(KERNEL_DIR)/arch/x86_64/boot.asm \
                  $(KERNEL_DIR)/arch/x86_64/idt.asm \
                  $(KERNEL_DIR)/arch/x86_64/context.asm \
                  $(KERNEL_DIR)/arch/x86_64/syscall.asm \
                  $(KERNEL_DIR)/arch/x86_64/user_entry.asm

# Kernel C sources
KERNEL_C_SRCS = $(KERNEL_DIR)/kernel.c \
                $(KERNEL_DIR)/drivers/vga/vga.c \
                $(KERNEL_DIR)/lib/string.c \
                $(KERNEL_DIR)/mm/pmm.c \
                $(KERNEL_DIR)/mm/vmm.c \
                $(KERNEL_DIR)/mm/heap.c \
                $(KERNEL_DIR)/arch/x86_64/gdt.c \
                $(KERNEL_DIR)/interrupts/idt.c \
                $(KERNEL_DIR)/interrupts/isr.c \
                $(KERNEL_DIR)/interrupts/irq.c \
                $(KERNEL_DIR)/drivers/pic/pic.c \
                $(KERNEL_DIR)/drivers/pit/pit.c \
                $(KERNEL_DIR)/drivers/keyboard/keyboard.c \
                $(KERNEL_DIR)/proc/process.c \
                $(KERNEL_DIR)/proc/sched.c \
                $(KERNEL_DIR)/syscall/syscall.c \
                $(KERNEL_DIR)/syscall/sys_process.c \
                $(KERNEL_DIR)/syscall/sys_io.c \
                $(KERNEL_DIR)/syscall/sys_fs.c \
                $(KERNEL_DIR)/user/user_process.c \
                $(KERNEL_DIR)/fs/ramfs.c \
                $(KERNEL_DIR)/fs/vfs.c \
                $(KERNEL_DIR)/fs/path.c \
                $(KERNEL_DIR)/fs/file.c

# =============================================================================
# User Program Configuration
# =============================================================================

# C Compiler flags for user programs
USER_CFLAGS = -ffreestanding \
              -fno-builtin \
              -fno-stack-protector \
              -fno-pic \
              -fno-pie \
              -nostdlib \
              -nostdinc \
              -mno-red-zone \
              -mno-mmx \
              -mno-sse \
              -mno-sse2 \
              -Wall \
              -Wextra \
              -Werror \
              -std=c11 \
              -g \
              -O2 \
              -I $(USER_DIR)/include

# User program linker flags
USER_LDFLAGS = -T $(USER_DIR)/linker.ld \
               -nostdlib \
               -z max-page-size=4096

# User program source files
USER_ASM_SRCS = $(USER_DIR)/lib/syscall.asm \
                $(USER_DIR)/shell/start.asm
USER_C_SRCS = $(USER_DIR)/lib/libc.c \
              $(USER_DIR)/shell/shell.c

# User program object files
USER_ASM_OBJS = $(patsubst $(USER_DIR)/%.asm,$(BUILD_DIR)/user_prog/%.o,$(USER_ASM_SRCS))
USER_C_OBJS = $(patsubst $(USER_DIR)/%.c,$(BUILD_DIR)/user_prog/%.o,$(USER_C_SRCS))
USER_OBJS = $(USER_ASM_OBJS) $(USER_C_OBJS)

# User program output
USER_ELF = $(BUILD_DIR)/user_prog/shell.elf
USER_BIN = $(BUILD_DIR)/user_prog/shell.bin
USER_EMBED_OBJ = $(BUILD_DIR)/user_shell_embed.o

# =============================================================================
# Object Files
# =============================================================================

KERNEL_ASM_OBJS = $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))
KERNEL_C_OBJS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))
KERNEL_OBJS = $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS)

# =============================================================================
# Output Files
# =============================================================================

STAGE1_BIN = $(BUILD_DIR)/stage1.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = chanux.img

# =============================================================================
# Default Target
# =============================================================================

.PHONY: all
all: $(OS_IMAGE)

# =============================================================================
# Build Stage 1 Bootloader
# =============================================================================

$(STAGE1_BIN): $(BOOT_STAGE1) $(BOOT_DIR)/include/boot.inc | $(BUILD_DIR)
	@echo "[ASM] Building Stage 1 bootloader..."
	$(AS) $(ASFLAGS_BOOT) $< -o $@
	@echo "[OK] Stage 1: $@"

# =============================================================================
# Build Stage 2 Bootloader
# =============================================================================

$(STAGE2_BIN): $(BOOT_STAGE2) $(BOOT_DIR)/include/boot.inc | $(BUILD_DIR)
	@echo "[ASM] Building Stage 2 bootloader..."
	$(AS) $(ASFLAGS_BOOT) $< -o $@
	@echo "[OK] Stage 2: $@ ($(shell stat -f%z $@ 2>/dev/null || stat -c%s $@ 2>/dev/null) bytes)"

# =============================================================================
# Build Kernel Assembly Objects
# =============================================================================

$(BUILD_DIR)/arch/x86_64/%.o: $(KERNEL_DIR)/arch/x86_64/%.asm | $(BUILD_DIR)/arch/x86_64
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS_KERNEL) $< -o $@

# =============================================================================
# Build Kernel C Objects
# =============================================================================

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	@echo "[CC] $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# =============================================================================
# Link Kernel (depends on user program being embedded)
# =============================================================================

$(KERNEL_ELF): $(KERNEL_OBJS) $(USER_EMBED_OBJ) scripts/linker.ld | $(BUILD_DIR)
	@echo "[LD] Linking kernel..."
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) $(USER_EMBED_OBJ) -o $@
	@echo "[OK] Kernel ELF: $@"

# =============================================================================
# Create Kernel Binary
# =============================================================================

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD_DIR)
	@echo "[OBJCOPY] Creating kernel binary..."
	$(OBJCOPY) -O binary $< $@
	@echo "[OK] Kernel binary: $@ ($(shell stat -f%z $@ 2>/dev/null || stat -c%s $@ 2>/dev/null) bytes)"

# =============================================================================
# Build User Program Assembly Objects
# =============================================================================

# Assembler flags for user program (64-bit, ELF format)
ASFLAGS_USER = -f elf64 -g -F dwarf

$(BUILD_DIR)/user_prog/lib/%.o: $(USER_DIR)/lib/%.asm | $(BUILD_DIR)/user_prog/lib
	@echo "[ASM-USER] $<"
	$(AS) $(ASFLAGS_USER) $< -o $@

$(BUILD_DIR)/user_prog/shell/%.o: $(USER_DIR)/shell/%.asm | $(BUILD_DIR)/user_prog/shell
	@echo "[ASM-USER] $<"
	$(AS) $(ASFLAGS_USER) $< -o $@

# =============================================================================
# Build User Program C Objects
# =============================================================================

$(BUILD_DIR)/user_prog/%.o: $(USER_DIR)/%.c | $(BUILD_DIR)/user_prog
	@echo "[CC-USER] $<"
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

# =============================================================================
# Link User Program
# =============================================================================

$(USER_ELF): $(USER_OBJS) $(USER_DIR)/linker.ld | $(BUILD_DIR)/user_prog
	@echo "[LD-USER] Linking user program..."
	$(LD) $(USER_LDFLAGS) $(USER_OBJS) -o $@
	@echo "[OK] User ELF: $@"

# =============================================================================
# Create User Program Binary
# =============================================================================

$(USER_BIN): $(USER_ELF) | $(BUILD_DIR)/user_prog
	@echo "[OBJCOPY-USER] Creating user binary..."
	$(OBJCOPY) -O binary $< $@
	@echo "[OK] User binary: $@ ($(shell stat -f%z $@ 2>/dev/null || stat -c%s $@ 2>/dev/null) bytes)"

# =============================================================================
# Embed User Program into Kernel
# =============================================================================

$(USER_EMBED_OBJ): $(USER_BIN)
	@echo "[EMBED] Embedding shell program into kernel..."
	cd $(BUILD_DIR) && $(OBJCOPY) -I binary -O elf64-x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		-B i386:x86-64 \
		--redefine-sym _binary_user_prog_shell_bin_start=_user_shell_start \
		--redefine-sym _binary_user_prog_shell_bin_end=_user_shell_end \
		--redefine-sym _binary_user_prog_shell_bin_size=_user_shell_size \
		user_prog/shell.bin user_shell_embed.o
	@echo "[OK] Shell program embedded: $@"

# Note: USER_EMBED_OBJ is explicitly added in the KERNEL_ELF rule

# =============================================================================
# User Program Build Directories
# =============================================================================

$(BUILD_DIR)/user_prog:
	@mkdir -p $(BUILD_DIR)/user_prog

$(BUILD_DIR)/user_prog/lib:
	@mkdir -p $(BUILD_DIR)/user_prog/lib

$(BUILD_DIR)/user_prog/shell:
	@mkdir -p $(BUILD_DIR)/user_prog/shell

# =============================================================================
# Create Bootable Disk Image
# =============================================================================

$(OS_IMAGE): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "[IMG] Creating disk image..."
	@# Create empty 10MB disk image
	dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
	@# Write Stage 1 (MBR) at sector 0
	dd if=$(STAGE1_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	@# Write Stage 2 at sector 1 (skip MBR)
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@# Write kernel at sector 34 (after Stage 2)
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=34 conv=notrunc 2>/dev/null
	@echo "[OK] Disk image created: $@"
	@echo ""
	@echo "Disk layout:"
	@echo "  Sector 0:      Stage 1 (MBR)"
	@echo "  Sectors 1-33:  Stage 2 (bootloader)"
	@echo "  Sectors 34+:   Kernel"

# =============================================================================
# Create Build Directories
# =============================================================================

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/arch/x86_64:
	@mkdir -p $(BUILD_DIR)/arch/x86_64

$(BUILD_DIR)/drivers/vga:
	@mkdir -p $(BUILD_DIR)/drivers/vga

$(BUILD_DIR)/lib:
	@mkdir -p $(BUILD_DIR)/lib

$(BUILD_DIR)/mm:
	@mkdir -p $(BUILD_DIR)/mm

$(BUILD_DIR)/interrupts:
	@mkdir -p $(BUILD_DIR)/interrupts

$(BUILD_DIR)/drivers/pic:
	@mkdir -p $(BUILD_DIR)/drivers/pic

$(BUILD_DIR)/drivers/pit:
	@mkdir -p $(BUILD_DIR)/drivers/pit

$(BUILD_DIR)/drivers/keyboard:
	@mkdir -p $(BUILD_DIR)/drivers/keyboard

$(BUILD_DIR)/proc:
	@mkdir -p $(BUILD_DIR)/proc

$(BUILD_DIR)/syscall:
	@mkdir -p $(BUILD_DIR)/syscall

$(BUILD_DIR)/user:
	@mkdir -p $(BUILD_DIR)/user

$(BUILD_DIR)/fs:
	@mkdir -p $(BUILD_DIR)/fs

# =============================================================================
# Run in QEMU
# =============================================================================

.PHONY: run
run: $(OS_IMAGE)
	@echo ""
	@echo "=== Starting Chanux in QEMU ==="
	@echo ""
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -m 128M \
	        -serial stdio \
	        -no-reboot \
	        -no-shutdown

# =============================================================================
# Run with QEMU Monitor
# =============================================================================

.PHONY: monitor
monitor: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -m 128M \
	        -monitor stdio \
	        -no-reboot

# =============================================================================
# Debug with GDB
# =============================================================================

.PHONY: debug
debug: $(OS_IMAGE)
	@echo ""
	@echo "=== Starting Chanux in QEMU (Debug Mode) ==="
	@echo "Connect GDB with: target remote localhost:1234"
	@echo ""
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -m 128M \
	        -serial stdio \
	        -no-reboot \
	        -S -s &
	@echo "Waiting for QEMU to start..."
	@sleep 1
	gdb -ex "target remote localhost:1234" \
	    -ex "symbol-file $(KERNEL_ELF)" \
	    -ex "break kernel_main" \
	    -ex "continue"

# =============================================================================
# Debug without GDB auto-connect
# =============================================================================

.PHONY: debug-wait
debug-wait: $(OS_IMAGE)
	@echo ""
	@echo "=== Starting Chanux in QEMU (Waiting for GDB) ==="
	@echo "Connect GDB with: target remote localhost:1234"
	@echo ""
	$(QEMU) -drive format=raw,file=$(OS_IMAGE) \
	        -m 128M \
	        -serial stdio \
	        -no-reboot \
	        -S -s

# =============================================================================
# Clean
# =============================================================================

.PHONY: clean
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(OS_IMAGE)
	@echo "[OK] Clean complete"

# =============================================================================
# Show Configuration
# =============================================================================

.PHONY: info
info:
	@echo "Chanux OS Build Configuration"
	@echo "=============================="
	@echo "Cross Compiler: $(CC)"
	@echo "Assembler:      $(AS)"
	@echo "Linker:         $(LD)"
	@echo "QEMU:           $(QEMU)"
	@echo ""
	@echo "Kernel Sources:"
	@echo "  Assembly: $(KERNEL_ASM_SRCS)"
	@echo "  C:        $(KERNEL_C_SRCS)"
	@echo ""
	@echo "Output: $(OS_IMAGE)"

# =============================================================================
# Help
# =============================================================================

.PHONY: help
help:
	@echo "Chanux OS Build System"
	@echo "======================"
	@echo ""
	@echo "Usage:"
	@echo "  make          Build the OS image"
	@echo "  make run      Build and run in QEMU"
	@echo "  make debug    Build and run with GDB debugging"
	@echo "  make monitor  Build and run with QEMU monitor"
	@echo "  make clean    Remove all build artifacts"
	@echo "  make info     Show build configuration"
	@echo "  make help     Show this help message"
