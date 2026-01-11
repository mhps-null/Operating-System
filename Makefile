# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025
DISK_NAME     = storage

# Flags
WARNING_CFLAG = -Wall -Wextra 
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386


run: all disk
	@qemu-system-i386 -s -S -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso -display sdl
all: build
build: iso disk
clean:
	@rm -rf *.o *.iso $(OUTPUT_FOLDER)/*.o $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/$(ISO_NAME).iso $(OUTPUT_FOLDER)/iso $(OUTPUT_FOLDER)/$(DISK_NAME).bin

disk:
	@if [ ! -f $(OUTPUT_FOLDER)/$(DISK_NAME).bin ]; then \
		echo "Creating empty disk image: $(OUTPUT_FOLDER)/$(DISK_NAME).bin"; \
		qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M; \
	fi

kernel:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/cpu/intsetup.s -o $(OUTPUT_FOLDER)/intsetup.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/context-switch.s -o $(OUTPUT_FOLDER)/context-switch.o

# TODO: Compile C file with CFLAGS
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/gdt.c -o $(OUTPUT_FOLDER)/gdt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/portio.c -o $(OUTPUT_FOLDER)/portio.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/idt.c -o $(OUTPUT_FOLDER)/idt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/interrupt.c -o $(OUTPUT_FOLDER)/interrupt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/text/framebuffer.c -o $(OUTPUT_FOLDER)/framebuffer.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/driver/keyboard.c -o $(OUTPUT_FOLDER)/keyboard.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/driver/disk.c -o $(OUTPUT_FOLDER)/disk.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/filesystem/ext2.c -o $(OUTPUT_FOLDER)/ext2.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/memory/paging.c -o $(OUTPUT_FOLDER)/paging.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/process/process.c -o $(OUTPUT_FOLDER)/process.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/process/scheduler.c -o $(OUTPUT_FOLDER)/scheduler.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cmos/cmos.c -o $(OUTPUT_FOLDER)/cmos.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/graphics/graphics.c -o $(OUTPUT_FOLDER)/graphics.o

	@$(LIN) $(LFLAGS) bin/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...
	@rm -f *.o

iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel     $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1                 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst   $(OUTPUT_FOLDER)/iso/boot/grub/

# TODO: Create ISO image
	@genisoimage -R            \
	-b boot/grub/grub1         \
	-no-emul-boot              \
	-boot-load-size 4          \
	-A os                      \
	-input-charset utf8        \
	-quiet                     \
	-boot-info-table           \
	-o $(OUTPUT_FOLDER)/$(ISO_NAME).iso         \
	$(OUTPUT_FOLDER)/iso

	@rm -r $(OUTPUT_FOLDER)/iso

# (Sesuaikan nama file .c Anda di bawah ini)
INSERTER_C_FILES = \
    src/external/external-inserter.c \
    src/filesystem/ext2.c \
    src/stdlib/string.c

# Target 'inserter'
inserter: $(INSERTER_C_FILES)
	@echo "Building inserter..."
	@gcc -g -I src -o src/external/external-inserter $(INSERTER_C_FILES)
	@echo "Build inserter complete."

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/stdlib/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell_elf
	@echo Linking object shell object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

clock:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o $(OUTPUT_FOLDER)/crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/external/clock.c -o $(OUTPUT_FOLDER)/clock.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/clock.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/clock
	@echo Linking object clock object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/clock.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/clock_elf
	@echo Linking object clock object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/clock
	rm -f $(OUTPUT_FOLDER)/clock.o $(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/string.o

hello-world:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o $(OUTPUT_FOLDER)/crt0.o
	@$(CC) 	$(CFLAGS) -fno-pie $(SOURCE_FOLDER)/external/hello-world.c -o $(OUTPUT_FOLDER)/hello-world.o
	@$(CC) 	$(CFLAGS) -fno-pie $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/hello-world.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/hello-world
	@echo Linking object hello-world object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/hello-world.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/hello-world_elf
	@echo Linking object hello-world object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/hello-world
	rm -f $(OUTPUT_FOLDER)/hello-world.o $(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/string.o

spinner:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o $(OUTPUT_FOLDER)/crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/external/spinner.c -o $(OUTPUT_FOLDER)/spinner.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/spinner.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/spinner
	@echo Linking object spinner object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		$(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/spinner.o $(OUTPUT_FOLDER)/string.o -o $(OUTPUT_FOLDER)/spinner_elf
	@echo Linking object spinner object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/spinner
	rm -f $(OUTPUT_FOLDER)/spinner.o $(OUTPUT_FOLDER)/crt0.o $(OUTPUT_FOLDER)/string.o

insert-shell: inserter user-shell
	@echo Inserting shell into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter shell 1 $(DISK_NAME).bin

insert-clock: inserter clock
	@echo Inserting clock into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter clock 1 $(DISK_NAME).bin

insert-spinner: inserter spinner
	@echo Inserting spinner into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter spinner 1 $(DISK_NAME).bin

insert-hello-world: inserter hello-world
	@echo Inserting hello-world into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter hello-world 1 $(DISK_NAME).bin

insert-badapple: inserter
	@echo Inserting badapplebit into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter badapplebit 1 $(DISK_NAME).bin

insert: insert-shell insert-clock insert-spinner insert-badapple insert-hello-world

restart:
	make clean
	make build
	make user-shell
	make clock