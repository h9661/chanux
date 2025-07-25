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
- **Physical Memory Manager**: Bitmap-based allocator with multiboot memory detection
- **Virtual Memory Manager**: x86 paging with dynamic page allocation and mapping
- **Heap Allocator**: Dynamic memory allocation with malloc/free/calloc/realloc
- **PIC Configuration**: 8259A PIC initialization with IRQ remapping and handling
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
│   │   ├── pmm.c      # Physical memory manager implementation
│   │   ├── pmm_test.c # PMM unit tests
│   │   ├── vmm.c      # Virtual memory manager implementation
│   │   ├── vmm_test.c # VMM unit tests
│   │   ├── paging_asm.asm # Paging assembly routines
│   │   ├── isr.asm    # Interrupt service routines
│   │   ├── heap.c     # Heap allocator implementation
│   │   ├── heap_test.c # Heap allocator unit tests
│   │   ├── pic.c      # Programmable Interrupt Controller implementation
│   │   ├── pic_test.c # PIC test code with timer and keyboard
│   │   ├── irq.asm    # IRQ handler assembly stubs
│   │   └── linker.ld  # Linker script for kernel memory layout
│   ├── drivers/       # Device drivers (future)
│   ├── lib/           # Utility libraries
│   │   └── string.c   # Memory manipulation functions
│   └── include/       # Header files
│       ├── string.h   # String/memory function declarations
│       ├── multiboot.h # Multiboot specification structures
│       ├── pmm.h      # Physical memory manager interface
│       ├── paging.h   # Paging structures and constants
│       ├── vmm.h      # Virtual memory manager interface
│       ├── heap.h     # Heap allocator interface
│       └── pic.h      # PIC constants and function declarations
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
PIC initialized: IRQs remapped to 0x20-0x2F
Initializing Physical Memory Manager...
Total memory: XX MB (XXXX pages)
Memory map:
  Region: 0x0 - 0xXXXXX (X MB) - Available
  [Additional memory regions...]
PMM initialized: XXXX free pages (XX MB)

Running PMM unit tests...
========================
[PASS] Single page allocation/free
[PASS] Multiple page allocation/free
[PASS] Allocation uniqueness
[PASS] Free and reallocate
[PASS] Memory statistics tracking
[PASS] Contiguous page allocation
[PASS] Region initialization

Test Results: 7 passed, 0 failed
All tests passed!

Final memory state:
Physical Memory Map:
Total pages: XXXX
Used pages: XXX
Free pages: XXXX
First 10 pages: UUUUUUUUFF

Welcome to ChanUX!

Initializing Virtual Memory Manager...
Identity mapping kernel memory (0-4MB)...
Enabling paging...
Virtual Memory Manager initialized

Running VMM unit tests...
========================
[PASS] Basic page mapping
[PASS] Page unmapping
[PASS] Range mapping
[PASS] Virtual page allocation
[PASS] Multiple virtual page allocation
[PASS] Page directory creation
[PASS] Address translation with offset

Test Results: 7 passed, 0 failed
All tests passed!

Testing virtual memory access...
Virtual memory write/read successful!

Final memory state:
Physical Memory Map:
Total pages: XXXX
Used pages: XXX
Free pages: XXXX
First 10 pages: UUUUUUUUFF

Welcome to ChanUX with Virtual Memory!

Initializing heap allocator...
Allocating 256 pages for heap at 0x10000000
Heap initialized: 1024 KB available

Running heap allocator tests...
==============================
[PASS] Basic allocation and free
[PASS] Zero size allocation
[PASS] Calloc zero initialization
[PASS] Realloc functionality
[PASS] Many small allocations
[PASS] Large allocation (64KB)
[PASS] Fragmentation and coalescing
[PASS] Heap statistics tracking
[PASS] Memory alignment
[PASS] Heap integrity check

Test Results: 10 passed, 0 failed
All tests passed!

Testing heap allocation...
Allocated strings:
  str1: Hello from heap!
  str2: Dynamic memory allocation works!
Memory freed successfully

Heap blocks:
Address     Size      Status
--------------------------------
0x10000000  XXXX bytes  FREE

Running PIC tests...
===================

PIC Status:
IRQ Mask: 0xFFFF
IRR: 0x0000
ISR: 0x0000
Enabled IRQs: None
Timer initialized at 100 Hz

Enabling timer interrupt (IRQ0)...
Enabling keyboard interrupt (IRQ1)...

PIC Status:
IRQ Mask: 0xFFFC
IRR: 0x0000
ISR: 0x0000
Enabled IRQs: 0, 1

Enabling CPU interrupts...
Waiting for timer interrupts...
Timer test complete: 10 ticks in ~100ms
Measured frequency: ~100 Hz

Press any key to test keyboard interrupt...
Keyboard interrupt! Scancode: 0xXX

Disabling timer and keyboard interrupts...

PIC Status:
IRQ Mask: 0xFFFF
IRR: 0x0000
ISR: 0x0000
Enabled IRQs: None

PIC tests completed successfully!
Total timer ticks: XX

Welcome to ChanUX with Virtual Memory, Heap, and Interrupts!
```

## Current Status

ChanUX has completed Phase 1 and significant portions of Phase 2. The kernel successfully boots in protected mode, manages both physical and virtual memory with paging enabled, and displays output to the screen.

## Features Roadmap

### Phase 1: Basic Boot (Completed) ✅
- [x] Basic bootloader - Multiboot-compliant bootloader that loads the kernel
- [x] Protected mode setup - 32-bit protected mode with flat memory model
- [x] Basic VGA text mode output - Terminal driver with scrolling support
- [x] Global Descriptor Table (GDT) - Memory segmentation for kernel/user space
- [x] Interrupt Descriptor Table (IDT) - Basic IDT structure (handlers not yet implemented)

### Phase 2: Core Kernel (In Progress)
- [x] Physical memory manager - Bitmap-based page allocator with memory detection
- [x] Virtual memory (paging) - x86 paging with page tables and address translation
- [x] Heap allocator - Dynamic memory allocation with malloc/free
- [x] Basic interrupt handlers - Page fault handler implemented
- [x] PIC configuration - 8259A PIC initialized with IRQ remapping

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
- PMM Bitmap: Located at 2MB (0x200000)
- Page Size: 4KB (4096 bytes)
- Heap Start: 256MB (0x10000000)
- Initial Heap Size: 1MB

### Boot Process
1. GRUB loads the multiboot header and kernel
2. Boot assembly code sets up initial stack
3. Control transfers to kernel_main()
4. GDT is installed for proper segmentation
5. IDT is installed for interrupt handling
6. PIC is initialized and IRQs remapped to 0x20-0x2F
7. Terminal is initialized for output
8. Physical Memory Manager initializes:
   - Detects available memory from multiboot info
   - Sets up bitmap allocator at 2MB
   - Marks kernel and bitmap memory as used
   - Runs unit tests to verify functionality
9. Virtual Memory Manager initializes:
   - Creates kernel page directory
   - Identity maps first 4MB for kernel
   - Enables paging (CR0.PG = 1)
   - Installs page fault handler
   - Runs unit tests to verify paging
10. Heap Allocator initializes:
   - Maps 256 pages (1MB) at 256MB mark
   - Sets up linked list of memory blocks
   - Runs unit tests to verify allocation
11. PIC test runs:
   - Enables timer (IRQ0) and keyboard (IRQ1) interrupts
   - Tests interrupt handling with timer ticks
   - Demonstrates keyboard interrupt on keypress
12. Kernel enters idle loop

### Code Organization
- Assembly files use NASM syntax
- C code follows C99 standard
- All structures are packed to match x86 requirements
- Comprehensive comments explain low-level details

### Physical Memory Manager
- **Algorithm**: Bitmap allocator (1 bit per 4KB page)
- **Memory Detection**: Uses multiboot memory map
- **Features**:
  - Single and multiple page allocation
  - Contiguous page allocation
  - Memory region marking (used/free)
  - Statistics tracking
  - Built-in unit tests
- **API**:
  - `pmm_alloc_page()` - Allocate single page
  - `pmm_alloc_pages(count)` - Allocate contiguous pages
  - `pmm_free_page(addr)` - Free single page
  - `pmm_free_pages(addr, count)` - Free multiple pages

### Virtual Memory Manager
- **Architecture**: x86 32-bit paging (4KB pages)
- **Page Tables**: Two-level hierarchy (Page Directory + Page Tables)
- **Features**:
  - Dynamic page table allocation
  - Virtual-to-physical address mapping
  - Page fault handling
  - Identity mapping for kernel space
  - Page directory cloning (for process creation)
  - TLB management
- **Memory Layout**:
  - 0x00000000-0x3FFFFFFF: Kernel space (1GB)
  - 0x40000000-0xBFFFFFFF: User space (2GB)
  - 0xC0000000-0xFFFFFFFF: Reserved (1GB)
- **API**:
  - `vmm_map_page()` - Map virtual to physical address
  - `vmm_unmap_page()` - Unmap virtual address
  - `vmm_alloc_page()` - Allocate and map virtual page
  - `vmm_create_page_directory()` - Create new address space

### Heap Allocator
- **Algorithm**: First-fit with block coalescing
- **Block Structure**: Linked list with headers containing size and status
- **Features**:
  - Dynamic memory allocation (malloc/free)
  - Memory reallocation (realloc)
  - Zero-initialized allocation (calloc)
  - Automatic heap expansion
  - Block splitting and coalescing
  - Heap integrity checking
  - Statistics tracking
- **Memory Layout**:
  - Start Address: 0x10000000 (256MB)
  - Initial Size: 1MB
  - Maximum Size: 256MB
  - Minimum Block: 16 bytes
  - Alignment: 8 bytes
- **API**:
  - `malloc(size)` - Allocate memory block
  - `free(ptr)` - Free memory block
  - `calloc(num, size)` - Allocate zero-initialized array
  - `realloc(ptr, new_size)` - Resize memory block
  - `heap_get_stats()` - Get heap usage statistics
  - `heap_check_integrity()` - Verify heap consistency

### Programmable Interrupt Controller (8259A PIC)
- **Purpose**: Manage hardware interrupts and deliver them to the CPU
- **Configuration**: Master-slave cascade (handles 16 IRQs total)
- **IRQ Remapping**: Remaps IRQs 0-15 to interrupt vectors 0x20-0x2F
- **Features**:
  - IRQ masking/unmasking
  - End of Interrupt (EOI) handling
  - Spurious interrupt detection
  - IRQ priority management
  - Interrupt status tracking
- **Supported IRQs**:
  - IRQ0: Timer (PIT)
  - IRQ1: Keyboard
  - IRQ2: Cascade (connects slave PIC)
  - IRQ3-15: Available for other devices
- **API**:
  - `pic_init()` - Initialize and remap PICs
  - `pic_enable_irq(irq)` - Enable specific IRQ
  - `pic_disable_irq(irq)` - Disable specific IRQ
  - `pic_send_eoi(irq)` - Send End of Interrupt signal
  - `pic_get_irq_mask()` - Get current IRQ mask