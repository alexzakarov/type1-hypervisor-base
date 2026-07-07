
gcc = g++ -m64 -fpermissive
nasm = nasm -f elf64
ld = ld -m elf_x86_64

scrDir = src

all: kernel iso clean

clean:
	rm -f *.o
	rm -rf ./obj

kernel:
	mkdir -p obj
	$(nasm) $(scrDir)/bootloader.asm -o ./obj/bootloader.o
	$(nasm) $(scrDir)/kernelloader.asm -o ./obj/kernelloader.o
	$(gcc) -c $(scrDir)/kernel.cpp -o ./obj/kernel.o
	$(ld) -T  ./linker.ld -o ./obj/kernel.bin ./obj/bootloader.o ./obj/kernelloader.o ./obj/kernel.o


iso:
	mv ./obj/kernel.bin ./Hypervisor/boot/kernel.bin
	grub-mkrescue -o Hypervisor.iso Hypervisor
