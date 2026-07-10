
gcc = g++ -m64 -fpermissive -I src/include
nasm = nasm -f elf64
ld = ld -m elf_x86_64

scrDir = src

all: kernel iso clean

clean:
	rm -f *.o
	rm -rf ./obj

kernel:
	mkdir -p obj
	$(nasm) $(scrDir)/boot/bootloader.asm -o ./obj/bootloader.o
	$(nasm) $(scrDir)/asm/idt.asm -o ./obj/idt.o
	$(nasm) $(scrDir)/boot/data.asm -o ./obj/data.o
	$(nasm) $(scrDir)/asm/vmm_loop.asm -o ./obj/vmm_loop_asm.o
	$(gcc) -c $(scrDir)/kernel/kernel.cpp -o ./obj/kernel.o
	$(gcc) -c $(scrDir)/utils/utils.cpp -o ./obj/utils.o
	$(gcc) -c $(scrDir)/mm/pmm.cpp -o ./obj/pmm.o
	$(gcc) -c $(scrDir)/mm/vmm.cpp -o ./obj/vmm.o
	$(gcc) -c $(scrDir)/vmm/vmm_ext.cpp -o ./obj/vmm_ext.o
	$(gcc) -c $(scrDir)/vmm/v_cpu.cpp -o ./obj/v_cpu.o
	$(gcc) -c $(scrDir)/vmm/vmcs_init.cpp -o ./obj/vmcs_init.o
	$(gcc) -c $(scrDir)/mm/ept.cpp -o ./obj/ept.o
	$(gcc) -c $(scrDir)/vmm/vmm_loop.cpp -o ./obj/vmm_loop.o
	$(ld) -T  ./linker.ld -o ./obj/kernel.bin ./obj/bootloader.o ./obj/idt.o ./obj/data.o ./obj/vmm_loop_asm.o ./obj/kernel.o ./obj/utils.o ./obj/pmm.o ./obj/vmm.o ./obj/vmm_ext.o ./obj/v_cpu.o ./obj/vmcs_init.o ./obj/ept.o ./obj/vmm_loop.o


iso:
	mv ./obj/kernel.bin ./Hypervisor/boot/kernel.bin
	grub-mkrescue -o Hypervisor.iso Hypervisor
