AS_FLAGS=
AS=i686-elf-as $(AS_FLAGS)

CC_INCLUDE=-Icstdlib
CC_ARCH_FLAG=-D CPU_I386
CC_BIT_FLAG=-D CPU_32BIT
CC_FLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -fms-extensions $(CC_ARCH_FLAG) $(CC_BIT_FLAG)
CC=i686-elf-gcc $(CC_INCLUDE) $(CC_FLAGS)
LD=i686-elf-ld

# OS core (module loader) compilation

iso/boot/myos.iso: iso/boot/myos.bin
	-rm iso/boot/myos.iso
	grub-file --is-x86-multiboot iso/boot/myos.bin
	grub-mkrescue -o $@ iso

iso/boot/myos.bin: boot.o kernel.o bios/bios_io.o kernlib/kernmem.o cstdlib/string.o cpu/pci.o cpu/cpu_int.o cpu/x86/gdt.o cpu/x86/idt.o cpu/x86/pic.o dev/ata.o dev/pio.o fs/fs.o fs/ext2.o bin/elf.o bin/module.o multiboot/mbt.o
	$(CC) -nostdlib -T kernel.ld -o $@ $^ -lgcc

boot.o: boot.s
	$(AS) -o $@ -c $<
kernel.o: kernel.c bios/bios_io.h kernlib/kernmem.h kernlib/kerndefs.h cpu/pci.h cpu/cpu_mode.h dev/pio.h dev/ata.h
	$(CC) -o $@ -c $<

bios/bios_io.o: bios/bios_io.c bios/bios_io.h
	$(CC) -o $@ -c $<
kernlib/kernmem.o: kernlib/kernmem.c kernlib/kernmem.h kernlib/kerndefs.h
	$(CC) -o $@ -c $<

cstdlib/string.o: cstdlib/string.c
	$(CC) -o $@ -c $<

cpu/pci.o: cpu/pci.c cpu/pci.h cpu/cpu_io.h
	$(CC) -o $@ -c $<
cpu/cpu_int.o: cpu/cpu_int.c cpu/cpu_int.h
	$(CC) -o $@ -c $<

cpu/x86/gdt.o: cpu/x86/gdt.s cpu/x86/gdt.h
	$(AS) -o $@ -c $<
cpu/x86/idt.o: cpu/x86/idt.c cpu/x86/idt.h
	$(CC) -o $@ -c $<
cpu/x86/pic.o: cpu/x86/pic.c cpu/x86/pic.h
	$(CC) -o $@ -c $<

dev/ata.o: dev/ata.c dev/ata.h
	$(CC) -o $@ -c $<
dev/pio.o: dev/pio.c dev/pio.h dev/ata.h cpu/pci.h
	$(CC) -o $@ -c $<

fs/fs.o: fs/fs.c fs/fs.h dev/ata.h
	$(CC) -o $@ -c $<
fs/ext2.o: fs/ext2.c fs/ext2.h dev/ata.h fs/fs.h
	$(CC) -o $@ -c $<

bin/elf.o: bin/elf.c bin/elf.h fs/fs.h
	$(CC) -o $@ -c $<
bin/module.o: bin/module.c bin/module.h bin/elf.h fs/fs.h
	$(CC) -o $@ -c $<

multiboot/mbt.o: multiboot/mbt.c multiboot/mbt.h
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
			 -s

# test image
img_refresh:
	rm atest.img
	cp ../atest.img .

img_mount:
	sudo mount -o loop atest.img ../mnt

img_umount:
	sudo umount ../mnt


# virtual memory module
modules/vmemory/vmemory.so: modules/vmemory/vmemory.o
	$(LD) -shared -fPIC -nostdlib $^ -o $@
	-sudo umount ../mnt
	sudo mount -o loop modules.img ../mnt
	sudo cp vmemory.so ../mnt
	sudo umount ../mnt
modules/vmemory/vmemory.o: modules/vmemory/vmemory.c modules/vmemory/vmemory.h
	$(CC) -c $< -o $@ -fPIC

# test module
test_module.so: test_module.o
	$(LD) -shared -fPIC -nostdlib $^ -o $@
	-sudo umount ../mnt
	sudo mount -o loop atest.img ../mnt
	sudo cp test_module.so ../mnt
	sudo umount ../mnt
test_module.o: test_module.c
	$(CC) -c $< -o $@ -fPIC
