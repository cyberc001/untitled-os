AS_FLAGS=-g
#-alm for assembly macro output
AS=x86_64-elf-as $(AS_FLAGS)

NASM=nasm $(NASM_FLAGS)
NASM_FLAGS=-felf64 -g -F dwarf

CC_INCLUDE=-Icstdlib -I.
CC_ARCH_FLAG=-D CPU_I386
CC_BIT_FLAG=-D CPU_64BIT
CC_INTERNAL_FLAGS=-std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -mabi=sysv -mno-80387 -mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -MMD
CC_FLAGS= -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -fms-extensions $(CC_ARCH_FLAG) $(CC_BIT_FLAG)
CC=x86_64-elf-gcc $(CC_INCLUDE) $(CC_FLAGS) $(CC_INTERNAL_FLAGS)
CC_MODULE=x86_64-elf-gcc $(CC_INCLUDE) $(CC_FLAGS) -ffreestanding
LD_INTERNAL_FLAGS=-nostdlib -static
LD=x86_64-elf-ld $(LD_INTERNAL_FLAGS)

# OS core (module loader) compilation

iso/myos.iso: iso/myos.bin
	xorriso -as mkisofs -b limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-eltorito-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso -o iso/myos.iso
iso/myos.bin: kernel.o kernlib/kernmem.o cstdlib/string.o cpu/pci.o cpu/cpu_int.o cpu/cpu_init.o cpu/x86/gdt.o cpu/x86/gdt_s.o cpu/x86/idt.o cpu/x86/isr.o cpu/x86/isr_s.o cpu/x86/pic.o dev/ata.o dev/pio.o dev/uart.o fs/fs.o fs/ext2.o bin/elf.o bin/module.o
	$(LD) -T kernel.ld -o $@ $^

kernel.o: kernel.c kernlib/kernmem.h kernlib/kerndefs.h cpu/pci.h cpu/cpu_mode.h dev/pio.h dev/ata.h
	$(CC) -o $@ -c $<

kernlib/kernmem.o: kernlib/kernmem.c kernlib/kernmem.h kernlib/kerndefs.h
	$(CC) -o $@ -c $<

cstdlib/string.o: cstdlib/string.c
	$(CC) -o $@ -c $<

cpu/pci.o: cpu/pci.c cpu/pci.h cpu/cpu_io.h
	$(CC) -o $@ -c $<
cpu/cpu_int.o: cpu/cpu_int.c cpu/cpu_int.h
	$(CC) -o $@ -c $<
cpu/cpu_init.o: cpu/cpu_init.c cpu/cpu_init.h cpu/x86/gdt.h cpu/x86/pic.h
	$(CC) -o $@ -c $<

cpu/x86/gdt.o: cpu/x86/gdt.c cpu/x86/gdt.h
	$(CC) -o $@ -c $<
cpu/x86/gdt_s.o: cpu/x86/gdt.s cpu/x86/gdt.h
	$(NASM) -o $@ $<
cpu/x86/idt.o: cpu/x86/idt.c cpu/x86/idt.h
	$(CC) -o $@ -c $<
cpu/x86/isr.o: cpu/x86/isr.c
	$(CC) -o $@ -c $<
cpu/x86/isr_s.o: cpu/x86/isr.s
	$(AS) -o $@ -c $<
cpu/x86/pic.o: cpu/x86/pic.c cpu/x86/pic.h
	$(CC) -o $@ -c $<

dev/ata.o: dev/ata.c dev/ata.h
	$(CC) -o $@ -c $<
dev/pio.o: dev/pio.c dev/pio.h dev/ata.h cpu/pci.h
	$(CC) -o $@ -c $<
dev/uart.o: dev/uart.c dev/uart.h cpu/cpu_io.h
	$(CC) -o $@ -c $<

fs/fs.o: fs/fs.c fs/fs.h dev/ata.h
	$(CC) -o $@ -c $<
fs/ext2.o: fs/ext2.c fs/ext2.h dev/ata.h fs/fs.h
	$(CC) -o $@ -c $<

bin/elf.o: bin/elf.c bin/elf.h fs/fs.h bin/module.h
	$(CC) -o $@ -c $<
bin/module.o: bin/module.c bin/module.h bin/elf.h fs/fs.h
	$(CC) -o $@ -c $<

# clean
clean:
	-rm *.o
	-rm ./*/*.o
	-rm ./*/*/*.o
	-rm iso/myos.bin
	-rm iso/myos.iso

# run emulator
run:
	qemu-system-x86_64 -cdrom iso/myos.iso \
			 -drive id=disk,file=atest.img,if=ide,cache=none,format=raw \
			 -d int \
			 #-S -gdb tcp::1234

# test image
img_refresh:
	rm atest.img
	cp ../atest.img .

img_mount:
	sudo mount -o loop atest.img ../mnt

img_umount:
	sudo umount ../mnt


clean_modules:
	rm modules/*/*.so
modules: modules/vmemory/vmemory.so

# virtual memory module
modules/vmemory/vmemory.so: modules/vmemory/vmemory.o modules/vmemory/allocator.o
	$(LD) -shared -fPIC -nostdlib $^ -o $@
	-sudo umount ../mnt
	sudo mount -o loop atest.img ../mnt
	sudo cp $@ ../mnt
	sudo umount ../mnt
modules/vmemory/vmemory.o: modules/vmemory/vmemory.c modules/vmemory/vmemory.h modules/vmemory/allocator.h
	$(CC_MODULE) -c $< -o $@ -fPIC
modules/vmemory/allocator.o: modules/vmemory/allocator.c modules/vmemory/allocator.h
	$(CC_MODULE) -c $< -o $@ -fPIC

# test module
test_module.so: test_module.o
	$(LD) -shared -fPIC -nostdlib $^ -o $@
	-sudo umount ../mnt
	sudo mount -o loop atest.img ../mnt
	sudo cp test_module.so ../mnt
	sudo umount ../mnt
test_module.o: test_module.c
	$(CC_MODULE) -c $< -o $@ -fPIC
