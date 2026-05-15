# minios

A minimal UNIX-like x86 operating system with a graphical window manager, filesystem, networking stack, and onboard applications — all running in 32-bit protected mode.

![Boot animation](https://img.shields.io/badge/arch-x86%20%2832--bit%29-blue)
![Build](https://img.shields.io/badge/build-i686--elf--gcc-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Features

### Core OS
- **Multiboot-compliant** — boots via GRUB or directly with `qemu -kernel`
- **Protected mode** (32-bit x86) with custom linker script
- **VGA text mode** (80×25) and **VESA framebuffer** (800×600×32)
- **PS/2 keyboard** with Shift/CapsLock, E0-prefix arrow keys (Home, End, Delete)
- **PS/2 mouse** driver with three cursor styles
- **Serial port** output for debug logging
- **RTC driver** for system clock

### Graphical Shell (GUI)
- **Multi-window manager** — drag, resize, minimize, raise, close
- **Taskbar** with window list and live clock
- **Apps menu** with 9 built-in applications
- **File drag & drop** between File Manager, Terminal, and Notepad
- **Terminal emulator** with 100-row scrollback buffer
- **Desktop settings** — background color, pattern, cursor style, boot animation
- **Pixel DE** — full-screen screensaver with 3D rotating cube, starfield, and gradient background

### Built-in Applications
| App | Description |
|-----|-------------|
| **Terminal** | Shell with command history, tab completion, arrow key navigation |
| **Paint** | Drawing canvas (C to clear) |
| **Snake** | Classic game with live score in title bar |
| **Browser** | Basic web browser |
| **Settings** | GUI system settings (persisted to `/settings.cfg`) |
| **Files** | File manager with drag initiation |
| **Calculator** | GUI RPN calculator with keyboard input |
| **Notepad** | Text editor with line numbers, syntax highlighting, auto-save on ESC |
| **Winver** | About screen |

### Shell & Commands
Over 40 built-in commands: `ls`, `cat`, `echo`, `clear`, `date`, `ps`, `help`, `shutdown`, `reboot`, `neofetch`, `meminfo`, `calc`, `edit`, `rm`, `mv`, `head`, `hexdump`, `wc`, `cal`, `seq`, `expr`, `which`, `touch`, `basename`, `dirname`, `rev`, `uniq`, `sort`, `hostname`, and more.

Features command history (16 entries, up/down arrows), mid-line editing (left/right, Home, End, Delete), and tab completion.

### File System
- **In-memory filesystem** — up to 64 files, 32-char names, 4 KB per file, 64 KB pool
- **Persistent settings** — `/settings.cfg` writes to FS image
- Commands: `create`, `ls`, `cat`, `rm`, `mv`, `touch`, `edit`

### Networking
- **RTL8139** NIC driver with PCI discovery and ring-buffer DMA
- **TCP/IP stack** — ARP, IP, TCP (SYN/ACK with `io_wait` polling), DNS, HTTP GET
- Static IP `10.0.2.15` on QEMU user-mode slirp network

### Language Runtimes
- **Python interpreter** — run `.py` scripts with `python` command
- **C compiler (TinyCC)** — compile and run `.c` source with `tcc` command
- **Two-pass assembler** — `asm` command for x86 assembly
- **Package manager** — `apt` command

### Graphics Stack
- **GLES software renderer** — points, Bresenham lines, scanline triangles
- **Window compositing** with z-ordering, shadows, drag indicators
- **Syntax highlighting** in Notepad for C, Python, and shell files

## Screenshots

*(Add screenshots of GUI desktop, terminal, notepad, and Pixel DE here)*

## Build Requirements

- `i686-elf-gcc` (cross-compiler targeting i686-elf)
- `i686-elf-ld`
- `nasm`
- `make`
- `qemu-system-i386` (for testing)

On macOS:
```bash
brew install i686-elf-gcc nasm qemu
```

On Debian/Ubuntu:
```bash
apt install build-essential nasm qemu-system-x86
# Install i686-elf-gcc from your distro or build from source
```

## Building

```bash
git clone <repo-url> minios
cd minios
make
```

This produces `minios.bin` — a Multiboot-compliant kernel image.

## Running

### Direct boot (QEMU)
```bash
qemu-system-i386 -kernel minios.bin -m 64 -vga std -nic user,model=rtl8139
```

### With a filesystem image
```bash
qemu-system-i386 -kernel minios.bin -m 64 -vga std \
  -nic user,model=rtl8139 \
  -drive file=disk.img,format=raw,if=ide
```

### With GRUB (ISO)
```bash
mkdir -p iso/boot/grub
cp minios.bin iso/boot/
cat > iso/boot/grub/grub.cfg << 'EOF'
menuentry "minios" {
    multiboot /boot/minios.bin
}
EOF
grub-mkrescue -o minios.iso iso
qemu-system-i386 -cdrom minios.iso -m 64 -vga std
```

## Project Structure

```
├── boot.asm           # Multiboot header + entry point (NASM)
├── link.ld            # Linker script
├── Makefile           # Build system
├── kernel.c           # Main kernel: shell, commands, main loop
├── vga.c / vga.h      # VGA text-mode driver (80×25)
├── keyboard.c / .h    # PS/2 keyboard driver
├── mouse.c / .h       # PS/2 mouse driver
├── serial.c / .h      # Serial port (COM1) driver
├── ports.h            # x86 I/O port utilities
├── framebuffer.c / .h # VESA framebuffer (800×600×32)
├── rtc.c / .h         # CMOS RTC driver
├── gui.c / gui.h      # Window manager + terminal emulator
├── gles.c / gles.h    # Software GLES renderer
├── de_pixel.c / .h    # Pixel Desktop Environment
├── fs.c / fs.h        # In-memory filesystem
├── rtl8139.c / .h     # RTL8139 NIC driver
├── net.c / net.h      # TCP/IP stack
├── settings_store.c   # Persistent settings
├── calcapp.c / .h     # GUI calculator
├── notepad.c / .h     # GUI text editor
├── paint.c / .h       # Drawing application
├── snake.c / .h       # Snake game
├── browser.c / .h     # Web browser
├── files.c / .h       # File manager
├── settings.c / .h    # Settings panel
├── winver.c / .h      # About screen
├── pylang.c / .h      # Python interpreter
├── tinycc.c / .h      # C compiler
├── asm.c / .h         # Assembler
├── apt.c / .h         # Package manager
└── ...
```

## Architecture

```
                    ┌─────────────────────────┐
                    │   GUI Window Manager     │
                    │  (gui.c, 9 apps, taskbar)│
                    ├─────────────────────────┤
                    │      Shell (kernel.c)     │
                    │   ~40 commands, history   │
                    ├─────────────────────────┤
                    │   Language Runtimes       │
                    │  (Python, C, Asm, APT)    │
                    ├─────────────────────────┤
                    │   TCP/IP Stack (net.c)    │
                    │  ARP/IP/TCP/DNS/HTTP      │
                    ├─────────────────────────┤
                    │   RTL8139 NIC Driver      │
                    ├─────────────────────────┤
                    │   In-Memory Filesystem    │
                    │  (fs.c, 64 files, 64KB)   │
                    ├─────────────────────────┤
                    │   Hardware Drivers        │
                    │  VGA/FB, Keyboard, Mouse, │
                    │  RTC, Serial, PCI         │
                    ├─────────────────────────┤
                    │   x86 Protected Mode     │
                    │  Multiboot, IDT, Port I/O │
                    └─────────────────────────┘
```

## QEMU Test Helper

The OS auto-detects the RTL8139 NIC on QEMU's `-nic user,model=rtl8139` and performs an HTTP GET test against the host on `10.0.2.2:8080` during the GUI main loop.

## Configuration

Settings are persisted to `/settings.cfg` in the filesystem:

- `mouse_icon` — 0=Crosshair, 1=Arrow, 2=Hand
- `bg_color` — desktop background color (hex RGB)
- `bg_pattern` — 0=Checkerboard, 1=Solid, 2=Stripes
- `boot_anim` — 0=skip, 1=show boot animation
- `anim_speed` — 1=slow, 2=medium, 3=fast
- `desktop_env` — 0=Default DE, 1=Pixel DE
