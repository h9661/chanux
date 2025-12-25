# Chanux OS

An educational x86_64 operating system built from scratch.

## Features

- **Custom Bootloader**: Two-stage bootloader (no GRUB)
  - Stage 1: 512-byte MBR that loads Stage 2
  - Stage 2: Enables A20, detects memory, transitions to 64-bit long mode
- **64-bit Kernel**: Written in C with x86_64 assembly
- **Higher-Half Kernel**: Runs at virtual address 0xFFFFFFFF80000000
- **VGA Text Mode**: 80x25 text output with colors

## Building

### Requirements

- **Cross-compiler**: x86_64-elf-gcc, x86_64-elf-ld
- **Assembler**: NASM
- **Emulator**: QEMU (qemu-system-x86_64)

### Build Commands

```bash
# Build the OS image
make

# Run in QEMU
make run

# Debug with GDB
make debug

# Clean build artifacts
make clean
```

## Project Structure

```
chanux/
├── boot/
│   ├── stage1/mbr.asm       # Stage 1 MBR bootloader
│   ├── stage2/loader.asm    # Stage 2 extended bootloader
│   └── include/boot.inc     # Shared boot constants
├── kernel/
│   ├── arch/x86_64/         # Architecture-specific code
│   ├── drivers/             # Device drivers (VGA, keyboard, etc.)
│   ├── include/             # Kernel headers
│   └── kernel.c             # Main kernel
├── scripts/
│   └── linker.ld            # Kernel linker script
└── Makefile                 # Build system
```

## Development Phases

- [x] **Phase 1**: Bootloader and minimal kernel
- [ ] **Phase 2**: Memory management (PMM, VMM, heap)
- [ ] **Phase 3**: Interrupts and I/O (IDT, keyboard, timer)
- [ ] **Phase 4**: Process management (scheduler)
- [ ] **Phase 5**: System calls and user mode
- [ ] **Phase 6**: File system and shell

## Boot Sequence

```
BIOS → MBR (Stage 1) → Stage 2 → Protected Mode → Long Mode → Kernel
       512 bytes        16KB      32-bit          64-bit      kernel_main()
```

## License

Educational project - feel free to learn from and modify this code.
