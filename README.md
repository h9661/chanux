# Chanux OS

An educational x86_64 operating system built from scratch.

## Features

### Phase 1: Bootloader
- **Custom Two-Stage Bootloader** (no GRUB dependency)
  - Stage 1: 512-byte MBR that loads Stage 2
  - Stage 2: Enables A20, detects memory via E820, transitions to 64-bit long mode
- **64-bit Kernel**: Written in C with x86_64 assembly
- **Higher-Half Kernel**: Runs at virtual address `0xFFFFFFFF80000000`
- **VGA Text Mode**: 80x25 text output with 16 colors and `kprintf()`

### Phase 2: Memory Management
- **Physical Memory Manager (PMM)**: Bitmap-based page allocator
- **Virtual Memory Manager (VMM)**: 4-level paging (PML4), higher-half kernel mapping
- **Kernel Heap**: `kmalloc()`/`kfree()` with block coalescing

### Phase 3: Interrupts and I/O
- **Interrupt Descriptor Table (IDT)**: 256 64-bit interrupt gates
- **GDT with TSS**: Task State Segment with IST1 for double fault protection
- **Exception Handlers**: Divide error, invalid opcode, double fault, GPF, page fault
- **8259A PIC Driver**: Remaps IRQs 0-15 to vectors 32-47, spurious IRQ handling
- **PIT Timer**: 100Hz system clock (10ms resolution)
- **PS/2 Keyboard Driver**: Scancode set 1, circular input buffer, modifier key tracking

### Phase 4: Process Management
- **Process Control Block (PCB)**: Full process state tracking (PID, state, stack, scheduling info)
- **Process States**: UNUSED, READY, RUNNING, BLOCKED, TERMINATED
- **Kernel Stack**: 8KB per-process kernel stack with 16-byte alignment
- **Context Switching**: Assembly-based register save/restore with TSS.RSP0 updates
- **Round-Robin Scheduler**: Preemptive scheduling with 100ms time slices
- **Process API**: `process_create()`, `process_exit()`, `process_yield()`, `process_block()`/`unblock()`
- **Timer Integration**: PIT IRQ0 triggers scheduler tick for preemption

## Building

### Requirements

- **Cross-compiler**: `x86_64-elf-gcc`, `x86_64-elf-ld`
- **Assembler**: NASM
- **Emulator**: QEMU (`qemu-system-x86_64`)

### Build Commands

```bash
# Build the OS image
make

# Run in QEMU
make run

# Debug with GDB
make debug

# Run with QEMU monitor
make monitor

# Clean build artifacts
make clean

# Show build configuration
make info
```

## Project Structure

```
chanux/
├── boot/
│   ├── stage1/mbr.asm           # Stage 1 MBR bootloader (512 bytes)
│   ├── stage2/loader.asm        # Stage 2 bootloader (A20, E820, long mode)
│   └── include/boot.inc         # Shared boot constants
├── kernel/
│   ├── arch/x86_64/
│   │   ├── boot.asm             # Kernel entry point
│   │   ├── gdt.c                # GDT with TSS
│   │   ├── idt.asm              # ISR/IRQ stubs, IDT loading
│   │   └── context.asm          # Context switch assembly
│   ├── interrupts/
│   │   ├── idt.c                # IDT initialization
│   │   ├── isr.c                # Exception handlers
│   │   └── irq.c                # Hardware IRQ dispatcher
│   ├── drivers/
│   │   ├── vga/vga.c            # VGA text mode driver
│   │   ├── pic/pic.c            # 8259A PIC driver
│   │   ├── pit/pit.c            # 8254 PIT timer
│   │   └── keyboard/keyboard.c  # PS/2 keyboard driver
│   ├── mm/
│   │   ├── pmm.c                # Physical memory manager
│   │   ├── vmm.c                # Virtual memory manager
│   │   └── heap.c               # Kernel heap allocator
│   ├── proc/
│   │   ├── process.c            # Process management (PCB, create/exit)
│   │   └── sched.c              # Round-robin scheduler
│   ├── lib/
│   │   └── string.c             # String utilities (memset, memcpy, etc.)
│   ├── include/                 # Kernel headers
│   └── kernel.c                 # Main kernel entry
├── scripts/
│   └── linker.ld                # Kernel linker script
└── Makefile                     # Build system
```

## Development Phases

- [x] **Phase 1**: Bootloader and minimal kernel
- [x] **Phase 2**: Memory management (PMM, VMM, heap)
- [x] **Phase 3**: Interrupts and I/O (IDT, GDT with TSS, PIC, PIT, keyboard)
- [x] **Phase 4**: Process management (PCB, round-robin scheduler, context switching)
- [ ] **Phase 5**: System calls and user mode
- [ ] **Phase 6**: File system and shell

## Architecture

### Boot Sequence

```
BIOS → MBR (Stage 1) → Stage 2 → Protected Mode → Long Mode → kernel_main()
       512 bytes        16KB      32-bit          64-bit

kernel_main():
  └→ VGA init → Memory init → Interrupt init → Process init → sched_start()
                (PMM/VMM/Heap)  (GDT/IDT/PIC/PIT)  (idle + demo)   (never returns)
```

### Scheduler Architecture

```
Timer Interrupt (IRQ0, 100Hz)
        │
        ▼
  sched_tick()
        │
        ├── Decrement time_slice
        │
        └── time_slice == 0?
               │
               ▼
          schedule()
               │
               ├── Pick next from run queue (round-robin)
               ├── Update TSS.RSP0 for new process
               └── context_switch() → Switch stacks → New process runs
```

### Memory Layout

```
Virtual Address Space (Higher-Half Kernel):
  0xFFFFFFFF80000000 - 0xFFFFFFFF80FFFFFF  Kernel code/data (16MB)
  0xFFFFFFFF81000000 - 0xFFFFFFFF81FFFFFF  Kernel heap (16MB)

Physical Memory:
  0x00000000 - 0x000FFFFF  Real mode area (1MB)
  0x00100000 - 0x001FFFFF  Kernel physical location (1MB+)
```

### Interrupt Vectors

```
Vectors 0-31:   CPU Exceptions (Divide error, Page fault, etc.)
Vectors 32-47:  Hardware IRQs (PIC remapped)
  IRQ0 (32):    PIT Timer (100Hz)
  IRQ1 (33):    PS/2 Keyboard
```

## Current Status

After boot, the kernel:
1. Initializes VGA driver and displays banner
2. Detects and displays memory map from E820
3. Initializes PMM, VMM, and kernel heap
4. Loads GDT with TSS (for double fault protection)
5. Sets up IDT with exception handlers
6. Remaps PIC and enables timer/keyboard IRQs
7. Initializes process management (creates idle process PID 0)
8. Creates demo processes and starts the round-robin scheduler
9. Demo processes run concurrently, printing tick messages

### Process Demo

When the OS boots, it creates two demo processes that demonstrate preemptive multitasking:

```
[Process 1] Started!
[Process 2] Started!
[P1] tick 1
[P2] tick 1
[P1] tick 2
[P2] tick 2
...
```

The processes are preempted by the timer interrupt every 100ms, showing the scheduler switching between them.

## License

Educational project - feel free to learn from and modify this code.
