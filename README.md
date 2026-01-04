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

### Phase 5: User Mode and System Calls
- **SYSCALL/SYSRET**: Fast system call mechanism via x86_64 MSRs (STAR, LSTAR, SFMASK)
- **System Calls**: 6 core syscalls (exit, write, read, yield, getpid, sleep)
- **User Processes**: Separate address space per process via PML4 page tables
- **User Stack**: 64KB per-process stack at `0x7FFFFFFFE000` (grows down)
- **User Code**: Loaded at `0x400000` with read-only permissions
- **Ring 3 Execution**: User mode entry via IRETQ with proper GDT segments
- **User Library**: Minimal libc with syscall wrappers (`puts()`, `print_int()`, etc.)

### Phase 6: File System and Shell
- **Virtual File System (VFS)**: Abstraction layer for filesystem operations
- **RAMFS**: In-memory filesystem (4MB capacity, 256 max files)
  - Superblock, inode table, data blocks
  - Direct block allocation (12 blocks per file, 48KB max)
  - Directory support with `.` and `..` entries
- **File Descriptors**: Per-process FD table (16 per process)
- **Path Resolution**: Absolute and relative path handling with normalization
- **Interactive Shell**: Command-line interface with 8 built-in commands
- **File Syscalls**: open, close, lseek, stat, fstat, readdir, getcwd, chdir

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

# Debug builds (verbose logging)
make DEBUG=1              # Enable all debug output
make DEBUG_SCHED=1        # Scheduler debug only
make DEBUG_VMM=1          # VMM debug only
make DEBUG_USER=1         # User mode debug only
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
│   │   ├── gdt.c                # GDT with TSS and user segments
│   │   ├── idt.asm              # ISR/IRQ stubs, IDT loading
│   │   ├── context.asm          # Context switch assembly
│   │   ├── syscall.asm          # SYSCALL/SYSRET entry point
│   │   └── user_entry.asm       # User mode entry (IRETQ)
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
│   ├── fs/                      # File system
│   │   ├── vfs.c                # Virtual File System layer
│   │   ├── ramfs.c              # RAM filesystem implementation
│   │   ├── file.c               # File descriptor management
│   │   └── path.c               # Path utilities
│   ├── syscall/
│   │   ├── syscall.c            # System call dispatcher
│   │   ├── sys_process.c        # Process control syscalls
│   │   ├── sys_io.c             # I/O syscalls (read/write)
│   │   └── sys_fs.c             # File system syscalls
│   ├── user/
│   │   └── user_process.c       # User process creation
│   ├── lib/
│   │   └── string.c             # String utilities (memset, memcpy, etc.)
│   ├── include/                 # Kernel headers
│   │   └── fs/                  # VFS, RAMFS, file headers
│   └── kernel.c                 # Main kernel entry
├── user/                        # User-space programs
│   ├── include/syscall.h        # User syscall API
│   ├── lib/
│   │   ├── syscall.asm          # Raw SYSCALL invocation
│   │   └── libc.c               # Minimal C library
│   ├── init/
│   │   ├── start.asm            # User program entry point
│   │   └── init.c               # First user program (demo)
│   ├── shell/                   # Interactive shell
│   │   ├── start.asm            # Shell entry point
│   │   └── shell.c              # Shell implementation
│   └── linker.ld                # User program linker script
├── scripts/
│   └── linker.ld                # Kernel linker script
└── Makefile                     # Build system
```

## Development Phases

- [x] **Phase 1**: Bootloader and minimal kernel
- [x] **Phase 2**: Memory management (PMM, VMM, heap)
- [x] **Phase 3**: Interrupts and I/O (IDT, GDT with TSS, PIC, PIT, keyboard)
- [x] **Phase 4**: Process management (PCB, round-robin scheduler, context switching)
- [x] **Phase 5**: User mode and system calls (SYSCALL/SYSRET, 6 syscalls, user processes)
- [x] **Phase 6**: File system and shell (VFS, RAMFS, interactive shell)
- [ ] **Phase 7**: Extended filesystem (file deletion, mkdir, permissions)

## Architecture

### Boot Sequence

```
BIOS → MBR (Stage 1) → Stage 2 → Protected Mode → Long Mode → kernel_main()
       512 bytes        16KB      32-bit          64-bit

kernel_main():
  └→ VGA init → Memory init → GDT/TSS → IDT → PIC/PIT → SYSCALL init → RAMFS init → Shell load → sched_start()
                (PMM/VMM/Heap)                          (MSRs)         (create /bin) (from RAMFS) (never returns)
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
Virtual Address Space:

  User Space (Ring 3):
    0x0000000000400000  User code (USER_CODE_BASE)
    0x00007FFFFFFFE000  User stack top (64KB, grows down)

  Kernel Space (Ring 0, Higher-Half):
    0xFFFFFFFF80000000 - 0xFFFFFFFF80FFFFFF  Kernel code/data (16MB)
    0xFFFFFFFF81000000 - 0xFFFFFFFF81FFFFFF  Kernel heap (16MB)

Physical Memory:
  0x00000000 - 0x000FFFFF  Real mode area (1MB)
  0x00100000 - 0x001FFFFF  Kernel physical location (1MB+)
```

### System Call Interface

| Number | Name    | Signature                                     |
|--------|---------|-----------------------------------------------|
| 0      | exit    | `void exit(int code)`                         |
| 1      | write   | `ssize_t write(int fd, const void* buf, len)` |
| 2      | read    | `ssize_t read(int fd, void* buf, len)`        |
| 3      | yield   | `int yield(void)`                             |
| 4      | getpid  | `pid_t getpid(void)`                          |
| 5      | sleep   | `int sleep(uint64_t ms)`                      |
| 6      | open    | `int open(const char* path, int flags)`       |
| 7      | close   | `int close(int fd)`                           |
| 8      | lseek   | `off_t lseek(int fd, off_t offset, int whence)` |
| 9      | stat    | `int stat(const char* path, struct stat* buf)` |
| 10     | fstat   | `int fstat(int fd, struct stat* buf)`         |
| 11     | readdir | `int readdir(int fd, struct dirent* ent, int idx)` |
| 12     | getcwd  | `int getcwd(char* buf, size_t size)`          |
| 13     | chdir   | `int chdir(const char* path)`                 |

```
Calling Convention: RAX=syscall#, RDI/RSI/RDX/R10/R8/R9=args
Return Value: RAX (negative = error)
```

### Shell Commands

| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `echo [args]` | Print arguments to stdout |
| `cat <file>` | Display file contents |
| `ls [dir]` | List directory contents |
| `pwd` | Print working directory |
| `cd <dir>` | Change current directory |
| `clear` | Clear the screen |
| `exit` | Exit shell (halt system) |

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
4. Loads GDT with TSS and user segments (Ring 0 + Ring 3)
5. Sets up IDT with exception handlers
6. Remaps PIC and enables timer/keyboard IRQs
7. Initializes SYSCALL/SYSRET mechanism (MSR configuration)
8. Initializes RAMFS and creates filesystem structure
9. Creates `/bin` directory and demo files (`/hello.txt`, `/README`)
10. Loads interactive shell from embedded binary
11. Starts round-robin scheduler (preemptive multitasking)

### Interactive Shell

The OS boots into an interactive shell where users can navigate the filesystem:

```
================================================================================
                            Welcome to Chanux OS!
                        A simple educational OS kernel
================================================================================
chanux:/$ help
Available commands:
  help       - Show this help message
  echo       - Print arguments
  cat        - Display file contents
  ls         - List directory
  pwd        - Print working directory
  cd         - Change directory
  clear      - Clear screen
  exit       - Exit shell
chanux:/$ ls
.
..
bin
hello.txt
README
chanux:/$ cat hello.txt
Hello from Chanux OS!
chanux:/$ cd bin
chanux:/bin$ pwd
/bin
chanux:/bin$
```

The shell demonstrates:
- File system navigation (ls, cd, pwd)
- File reading (cat)
- Path resolution (absolute and relative paths)
- Per-process current working directory
- All 14 system calls working together

### File System Architecture

```
RAMFS Layout (4MB total):
  Block 0:        Superblock (magic, counts, allocation bitmaps)
  Blocks 1-8:     Inode table (256 inodes × 128 bytes each)
  Blocks 9-1023:  Data blocks (4KB each, ~4MB storage)

Inode Structure (128 bytes):
  ├── Type (file=1, directory=2)
  ├── Size, permissions (rwx), link count
  ├── Timestamps (created, modified, accessed)
  ├── 12 direct block pointers (48KB max per file)
  └── Owner UID/GID

VFS Layer:
  ├── Vnode table (cached inodes with reference counting)
  ├── File table (open files with position tracking)
  └── Per-process FD table (16 descriptors per process)
```

## License

Educational project - feel free to learn from and modify this code.
