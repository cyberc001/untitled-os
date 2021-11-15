AS_FLAGS=
AS=i686-elf-as $(AS_FLAGS)

CC_INCLUDE=-Icstdlib
CC_ARCH_FLAG=-D CPU_I386
CC_FLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -fms-extensions $(CC_ARCH_FLAG)
CC=i686-elf-gcc $(CC_INCLUDE) $(CC_FLAGS)

iso/boot/myos.iso: iso/boot/myos.bin
	-rm iso/boot/myos.iso
	grub-file --is-x86-multiboot iso/boot/myos.bin
	grub-mkrescue -o $@ iso

iso/boot/myos.bin: boot.o kernel.o bios/bios_io.o kernlib/kernmem.o cstdlib/string.o cpu/pci.o dev/ata.o dev/pio.o fs/fs.o fs/ext2.o
	$(CC) -nostdlib -T kernel.ld -o $@ $^ -lgcc

boot.o: boot.s
	$(AS) -o $@ -c $<
kernel.o: kernel.c bios/bios_io.h kernlib/kernmem.h kernlib/kerndefs.h cpu/pci.h dev/pio.h dev/ata.h
	$(CC) -o $@ -c $<

bios/bios_io.o: bios/bios_io.c bios/bios_io.h cstdlib/string.h
	$(CC) -o $@ -c $<
kernlib/kernmem.o: kernlib/kernmem.c kernlib/kernmem.h kernlib/kerndefs.h
	$(CC) -o $@ -c $<

cstdlib/string.o: cstdlib/string.c cstdlib/string.h
	$(CC) -o $@ -c $<

cpu/pci.o: cpu/pci.c cpu/pci.h cpu/cpu_io.h
	$(CC) -o $@ -c $<

dev/ata.o: dev/ata.c dev/ata.h
	$(CC) -o $@ -c $<
dev/pio.o: dev/pio.c dev/pio.h dev/ata.h cpu/pci.h
	$(CC) -o $@ -c $<

fs/fs.o: fs/fs.c fs/fs.h dev/ata.h
	$(CC) -o $@ -c $<
fs/ext2.o: fs/ext2.c fs/ext2.h dev/ata.h fs/fs.h
	$(CC) -o $@ -c $<

# clean
clean:
	-rm *.o
	-rm ./*/*.o
	-rm iso/boot/myos.bin
	-rm iso/boot/myos.iso

# run emulator
run:
	qemu-system-i386 -cdrom iso/boot/myos.iso\
			 -drive id=disk,file=atest.img,if=ide,cache=none,format=raw \
