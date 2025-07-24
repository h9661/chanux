# ChanUX - A Custom Operating System

A hobby operating system built from scratch in C, designed for learning low-level system programming and OS development concepts.

## Overview

ChanUX is a simple operating system kernel written in C and x86 assembly. This project aims to explore fundamental OS concepts including:

- Bootloading and system initialization ✅
- Memory management (paging, virtual memory)
- Process management and scheduling
- Interrupt handling and system calls
- Basic device drivers
- File system implementation

### Key Features Implemented

- **Multiboot Bootloader**: Compatible with GRUB and other multiboot loaders
- **Protected Mode**: Full 32-bit protected mode with flat memory model
- **GDT (Global Descriptor Table)**: Proper segmentation for kernel and user space
- **IDT (Interrupt Descriptor Table)**: Framework for interrupt handling
- **VGA Terminal**: Text mode display driver with color support and scrolling
- **Build System**: Complete Makefile with support for cross-compilation

## Prerequisites

### Required Tools
- **GCC Cross-Compiler** (i686-elf-gcc recommended)
- **NASM** - Netwide Assembler for x86 assembly
- **GNU Make** - Build automation
- **QEMU** or **Bochs** - Virtual machine for testing
- **GNU Binutils** (i686-elf)
- **GRUB** - Bootloader (optional, for GRUB-based boot)

### Development Environment
- Linux or macOS (WSL2 for Windows)
- At least 4GB RAM
- 1GB free disk space

## Project Structure

```
chanux/
├── README.md           # This file
├── Makefile           # Build configuration with cross-compiler support
├── src/               # Source code
│   ├── boot/          # Bootloader and early initialization
│   │   └── boot.asm   # Multiboot-compliant bootloader
│   ├── kernel/        # Kernel core code
│   │   ├── kernel.c   # Main kernel entry point
│   │   ├── gdt.c      # Global Descriptor Table implementation
│   │   ├── gdt_flush.asm # GDT loading assembly routine
│   │   ├── idt.c      # Interrupt Descriptor Table implementation
│   │   ├── idt_load.asm  # IDT loading assembly routine
│   │   ├── terminal.c # VGA text mode terminal driver
│   │   └── linker.ld  # Linker script for kernel memory layout
│   ├── drivers/       # Device drivers (future)
│   ├── lib/           # Utility libraries
│   │   └── string.c   # Memory manipulation functions
│   └── include/       # Header files
│       └── string.h   # String/memory function declarations
├── build/             # Build output directory (generated)
├── iso/               # ISO creation directory (generated)
└── docs/              # Documentation

```

## Getting Started

### 1. Setting Up the Development Environment

#### Install Required Tools (Ubuntu/Debian)
```bash
# Install basic development tools
sudo apt-get update
sudo apt-get install build-essential nasm qemu-system-x86 grub-pc-bin xorriso

# Install cross-compiler dependencies
sudo apt-get install bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
```

#### Build Cross-Compiler
Building a cross-compiler is crucial for OS development. Follow the [OSDev Cross-Compiler Tutorial](https://wiki.osdev.org/GCC_Cross-Compiler).

### 2. Building the OS

```bash
# Clone the repository (when available)
git clone https://github.com/yourusername/chanux.git
cd chanux

# Build the kernel
make

# Create bootable ISO
make iso

# Run in QEMU
make run
```

### 3. Development Workflow

1. **Write code** in appropriate directories
2. **Build** using `make`
3. **Test** in QEMU/Bochs
4. **Debug** using GDB with QEMU: `make debug`
5. **Clean** build artifacts: `make clean`
6. **Iterate** and improve

### 4. Testing the Current Build

After building, you should see the following when running the kernel:
```
ChanUX kernel booting...
GDT installed
IDT installed
Welcome to ChanUX!
```

## Current Status

ChanUX has completed Phase 1 of development with a functional bootloader and basic system initialization. The kernel successfully boots in protected mode and displays output to the screen.

## Features Roadmap

### Phase 1: Basic Boot (Completed) ✅
- [x] Basic bootloader - Multiboot-compliant bootloader that loads the kernel
- [x] Protected mode setup - 32-bit protected mode with flat memory model
- [x] Basic VGA text mode output - Terminal driver with scrolling support
- [x] Global Descriptor Table (GDT) - Memory segmentation for kernel/user space
- [x] Interrupt Descriptor Table (IDT) - Basic IDT structure (handlers not yet implemented)

### Phase 2: Core Kernel
- [ ] Physical memory manager
- [ ] Virtual memory (paging)
- [ ] Heap allocator
- [ ] Basic interrupt handlers
- [ ] PIC configuration

### Phase 3: Essential Features
- [ ] Keyboard driver
- [ ] Timer/PIT driver
- [ ] Basic scheduler
- [ ] System calls interface
- [ ] User mode support

### Phase 4: Advanced Features
- [ ] File system (ext2 or custom)
- [ ] Process management
- [ ] Inter-process communication
- [ ] Basic shell
- [ ] Network stack (optional)

## Learning Resources

### Essential Reading
- [OSDev Wiki](https://wiki.osdev.org) - Comprehensive OS development resource
- [Intel x86 Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) - CPU architecture reference
- "Operating Systems: Three Easy Pieces" - Free online textbook
- "The Little Book About OS Development" - Practical guide

### Tutorials
- [Bran's Kernel Development Tutorial](http://www.osdever.net/bkerndev/Docs/title.htm)
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/)
- [os-tutorial](https://github.com/cfenollosa/os-tutorial) - GitHub tutorial series

### Communities
- [OSDev Forum](https://forum.osdev.org/) - Active community
- [r/osdev](https://www.reddit.com/r/osdev/) - Reddit community
- `##osdev` on Libera.Chat IRC

## Contributing

This is a personal learning project, but suggestions and discussions are welcome! Feel free to:
- Open issues for bugs or suggestions
- Submit pull requests for improvements
- Share your own OS development experiences

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- The OSDev community for invaluable resources
- Authors of various OS development tutorials
- QEMU and Bochs developers for excellent emulation tools

---

**Note**: Operating system development is a complex topic. Be prepared for challenges, lots of debugging, and deep dives into computer architecture. The journey is rewarding!

## Technical Details

### Memory Layout
- Kernel loads at 1MB (0x100000) as per Multiboot specification
- Stack: 16KB allocated in BSS section
- VGA Text Buffer: Direct memory access at 0xB8000

### Boot Process
1. GRUB loads the multiboot header and kernel
2. Boot assembly code sets up initial stack
3. Control transfers to kernel_main()
4. GDT is installed for proper segmentation
5. IDT is installed for interrupt handling
6. Terminal is initialized for output
7. Kernel enters idle loop

### Code Organization
- Assembly files use NASM syntax
- C code follows C99 standard
- All structures are packed to match x86 requirements
- Comprehensive comments explain low-level details