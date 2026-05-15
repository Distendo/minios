CC = i686-elf-gcc
LD = i686-elf-ld
CFLAGS = -std=c99 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -m32 -Wall -Wextra -O2 -I.
LDFLAGS = -T link.ld

OBJS = boot.o vga.o keyboard.o serial.o framebuffer.o mouse.o gui.o kernel.o paint.o snake.o browser.o rtc.o settings.o fs.o files.o winver.o rtl8139.o net.o asm.o pylang.o tinycc.o apt.o calcapp.o notepad.o settings_store.o gles.o de_pixel.o

all: minios.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	nasm -f elf32 $< -o $@

minios.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

run: minios.bin
	qemu-system-i386 -kernel minios.bin -m 64 -vga std

clean:
	rm -f *.o minios.bin

.PHONY: all run clean
