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
- **Keyboard Driver**: PS/2 keyboard support with scancode translation and buffering
- **Timer Driver**: PIT-based system timer with timing and delay functions
- **System Call Interface**: Linux-compatible int 0x80 mechanism with basic syscalls
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
- At least 1GB RAM
- 1GB total disk space

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
│   │   ├── vmm.c      # Virtual memory manager implementation
│   │   ├── paging_asm.asm # Paging assembly routines
│   │   ├── isr.asm    # Interrupt service routines
│   │   ├── heap.c     # Heap allocator implementation
│   │   ├── pic.c      # Programmable Interrupt Controller implementation
│   │   ├── irq.asm    # IRQ handler assembly stubs
│   │   ├── keyboard.c # PS/2 keyboard driver implementation
│   │   ├── timer.c    # PIT timer driver implementation
│   │   ├── syscall.c  # System call handler implementation
│   │   ├── syscall_asm.asm # System call interrupt handler
│   │   ├── scheduler.c # Process scheduler implementation
│   │   ├── tss.c      # Task State Segment management
│   │   ├── tss_flush.asm # TSS loading assembly routine
│   │   ├── switch.asm # Context switching assembly code
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
│       ├── pic.h      # PIC constants and function declarations
│       ├── keyboard.h # Keyboard driver interface and scancodes
│       ├── timer.h    # Timer driver interface and PIT constants
│       ├── syscall.h  # System call numbers and interfaces
│       ├── scheduler.h # Process scheduler interface and PCB structure
│       └── tss.h      # Task State Segment structure
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

## Current Status

ChanUX has completed Phase 1, Phase 2, and Phase 3. The kernel successfully boots in protected mode, manages both physical and virtual memory with paging enabled, handles hardware interrupts, provides system timing, accepts keyboard input, supports system calls via interrupt 0x80, and includes a functional round-robin process scheduler with context switching capabilities.

## Features Roadmap

### Phase 1: Basic Boot (Completed) ✅
- [x] Basic bootloader - Multiboot-compliant bootloader that loads the kernel
- [x] Protected mode setup - 32-bit protected mode with flat memory model
- [x] Basic VGA text mode output - Terminal driver with scrolling support
- [x] Global Descriptor Table (GDT) - Memory segmentation for kernel/user space
- [x] Interrupt Descriptor Table (IDT) - Basic IDT structure (handlers not yet implemented)

### Phase 2: Core Kernel (Completed) ✅
- [x] Physical memory manager - Bitmap-based page allocator with memory detection
- [x] Virtual memory (paging) - x86 paging with page tables and address translation
- [x] Heap allocator - Dynamic memory allocation with malloc/free
- [x] Basic interrupt handlers - Page fault handler implemented
- [x] PIC configuration - 8259A PIC initialized with IRQ remapping

### Phase 3: Essential Features (Completed) ✅
- [x] Keyboard driver - PS/2 keyboard with scancode translation and buffering
- [x] Timer/PIT driver - System timer with frequency control and time measurement
- [x] System calls interface - Linux-compatible int 0x80 with basic syscalls
- [x] Basic scheduler - Round-robin process scheduling with context switching
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
- Heap Start: 64MB (0x04000000)
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
9. Virtual Memory Manager initializes:
   - Creates kernel page directory
   - Identity maps first 4MB for kernel
   - Enables paging (CR0.PG = 1)
   - Installs page fault handler
10. Heap Allocator initializes:
   - Maps 256 pages (1MB) at 64MB mark
   - Sets up linked list of memory blocks
11. Timer driver initializes:
   - Configures PIT for 100Hz operation
   - Sets up tick counting
   - Enables timer interrupt (IRQ0)
12. Keyboard driver initializes:
   - Configures PS/2 keyboard controller
   - Sets up keyboard buffer
   - Enables keyboard interrupt (IRQ1)
13. System call interface initializes:
   - Installs interrupt handler for int 0x80
   - Sets up system call table
   - Configures IDT entry with DPL=3 for user access
14. Scheduler initializes:
   - Sets up Task State Segment (TSS)
   - Creates idle process (PID 0)
   - Initializes process list and ready queue
   - Enables timer-based preemption
15. Test processes are created
16. Kernel enables interrupts and scheduler takes over

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
  - 0x00000000-0x07FFFFFF: Kernel space (128MB)
  - 0x08000000-0x3FFFFFFF: User space (~896MB)
  - 0x40000000-0x3FFFFFFF: Total addressable (1GB)
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
  - Start Address: 0x04000000 (64MB)
  - Initial Size: 1MB
  - Maximum Size: 64MB
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

### PS/2 Keyboard Driver
- **Controller**: 8042 PS/2 controller at ports 0x60/0x64
- **Scancode Set**: Set 1 (PC/XT compatible)
- **Features**:
  - Scancode to ASCII translation (US QWERTY layout)
  - Modifier key support (Shift, Ctrl, Alt, Caps Lock)
  - Circular buffer for input queueing (256 key events)
  - LED control (Caps, Num, Scroll Lock)
  - Key press/release event tracking
  - Blocking and non-blocking input modes
- **Key Event Structure**:
  - Raw scancode
  - ASCII character (if printable)
  - Modifier state
  - Press/release flag
- **API**:
  - `keyboard_init()` - Initialize keyboard driver
  - `keyboard_getchar()` - Read character (blocking)
  - `keyboard_has_key()` - Check if input available
  - `keyboard_read_key()` - Read full key event
  - `keyboard_set_leds()` - Control keyboard LEDs
  - `keyboard_flush()` - Clear input buffer

### Programmable Interval Timer (8253/8254 PIT)
- **Base Frequency**: 1.193182 MHz input clock
- **Default Configuration**: 100 Hz (10ms per tick)
- **Channel Usage**: Channel 0 for system timer
- **Features**:
  - Configurable frequency (19 Hz to 1.193 MHz)
  - Tick counting for timekeeping
  - Millisecond and microsecond sleep functions
  - Time measurement with microsecond precision
  - Timer callbacks for periodic tasks
  - Uptime tracking
  - Dynamic frequency changes
- **Time Units**:
  - System ticks (frequency dependent)
  - Milliseconds (ms)
  - Microseconds (μs)
  - Seconds
- **API**:
  - `timer_init(freq)` - Initialize timer with frequency
  - `timer_get_ticks()` - Get current tick count
  - `timer_sleep(ms)` - Sleep for milliseconds
  - `timer_usleep(us)` - Sleep for microseconds
  - `timer_get_uptime_ms()` - Get system uptime in ms
  - `timer_set_frequency()` - Change timer frequency
  - `timer_measure_start/end()` - Measure elapsed time

### System Call Interface
- **Interrupt Vector**: 0x80 (Linux-compatible)
- **Calling Convention**: 
  - System call number in EAX
  - Arguments in EBX, ECX, EDX, ESI, EDI
  - Return value in EAX
- **Implemented System Calls**:
  - `SYS_EXIT` (1) - Terminate process
  - `SYS_WRITE` (2) - Write to file descriptor
  - `SYS_READ` (3) - Read from file descriptor (placeholder)
  - `SYS_OPEN` (4) - Open file (placeholder)
  - `SYS_CLOSE` (5) - Close file (placeholder)
  - `SYS_GETPID` (6) - Get process ID
  - `SYS_SLEEP` (7) - Sleep for milliseconds
- **User-Space Interface**:
  - Inline assembly wrappers (syscall0, syscall1, syscall2, syscall3)
  - Convenient macros (exit, write, read, getpid, sleep)
- **Features**:
  - IDT entry configured with DPL=3 for user-space access
  - System call table for easy extension
  - Register preservation across calls
  - Error handling with negative return values

### Process Scheduler
- **Algorithm**: Round-robin with fixed time slices
- **Time Slice**: 50ms (5 timer ticks at 100Hz)
- **Process States**:
  - READY: Process is ready to run
  - RUNNING: Process is currently executing
  - BLOCKED: Process is waiting for I/O or event
  - TERMINATED: Process has finished execution
- **Process Control Block (PCB)**:
  - Process ID (PID) and name
  - CPU context (registers)
  - Kernel and user stack pointers
  - Page directory for virtual memory
  - Scheduling information (time slice, priority)
  - Process statistics (CPU time, start time)
- **Features**:
  - Context switching with full register preservation
  - Task State Segment (TSS) for privilege level switching
  - Process creation with automatic stack allocation
  - Process termination with resource cleanup
  - Idle process for CPU power saving
  - Scheduler statistics tracking
- **API**:
  - `process_create()` - Create new process
  - `process_exit()` - Terminate current process
  - `process_yield()` - Voluntarily give up CPU
  - `process_get_current_pid()` - Get current process ID
  - `scheduler_tick()` - Called on timer interrupt
- **Implementation Details**:
  - Maximum 64 processes
  - 4KB kernel stack per process
  - Ready queue for scheduling
  - Automatic page directory cloning
  - Integration with timer interrupt (IRQ0)
  - Process sleep mechanism with automatic wake-up

## Recent Updates

### Bug Fixes
- **IRQ Handler**: Fixed incorrect stack offset calculation that was causing invalid IRQ numbers (e.g., 75664 instead of 0-15)
- **Process Sleep**: Replaced busy-wait sleep with proper blocking sleep that allows CPU to schedule other processes
- **Scheduler Preemption**: Fixed idle process preemption to allow switching to ready processes

### New Features
- **Process Sleep/Wake**: Processes can now sleep for specified milliseconds and automatically wake up
- **String Library**: Added `strcpy()`, `strncpy()`, and `strlen()` functions
- **Test Processes**: Three demonstration processes showing different scheduling behaviors

## Contributing

Feel free to contribute to ChanUX! Please ensure your code follows the existing style and includes appropriate comments.

## License

This project is open source and available under the MIT License.