# type1-hypervisor-base

A bare-metal x86_64 kernel/hypervisor foundation that boots from GRUB (Multiboot),
transitions from 32-bit protected mode into 64-bit long mode, sets up identity-mapped
paging, and writes directly to the VGA text buffer. It is intended as the starting point
for building a Type 1 (bare-metal) hypervisor on top of this minimal kernel base.

> Status: **early stage / boot skeleton.** Only the boot-to-long-mode pipeline and a
> trivial VGA text printer are implemented. No VMX/SVM, no guest support, no interrupts,
> no memory manager yet.

---

## Table of contents

- [Overview](#overview)
- [Boot flow](#boot-flow)
- [Project structure](#project-structure)
- [Requirements](#requirements)
- [Building](#building)
- [Running](#running)
- [Technical details](#technical-details)
- [Known issues / limitations](#known-issues--limitations)
- [Roadmap](#roadmap)

---

## Overview

The repository provides the smallest possible piece of code that:

1. Is recognized by GRUB as a valid **Multiboot** kernel.
2. Runs in 32-bit protected mode right after GRUB hands over control.
3. Constructs a 4-level x86_64 page table hierarchy and identity-maps the first 1 GB
   of physical memory using 2 MB huge pages.
4. Enables PAE, long mode (EFER.LME) and paging, then jumps to a 64-bit entry point
   through a hand-crafted GDT64.
5. Calls a C++ `kmain` that prints a diagnostic line to the VGA text buffer
   (`0xb8000`).

Everything is linked at physical address `0x00100000` (1 MiB) and packaged into a
bootable ISO via `grub-mkrescue`.

---

## Boot flow

```
        GRUB (Multiboot loader)
                 │  loads kernel.bin at 0x00100000, enters 32-bit protected mode
                 ▼
   bootloader.asm  ::  start
        ├─ set up ESP (16 KB stack in .bss)
        ├─ setup_page_tables()
        │     L4[0] -> L3[0] -> L2[0..511]  (each L2 entry = 2 MB huge page)
        │     identity-maps 0 .. 1 GB
        ├─ enable_paging()
        │     cr3 = L4
        │     cr4.PAE  = 1
        │     EFER.LME = 1
        │     cr0.PG   = 1
        ├─ lgdt  gdt64_pointer
        └─ jmp 0x08:long_mode_start
                 ▼
   kernelloader.asm  ::  long_mode_start   (64-bit)
        ├─ zero segment registers (ss, ds, es, fs, gs)
        ├─ mov esi, ebx        (preserve Multiboot info pointer)
        └─ call kmain
                 ▼
   kernel.cpp  ::  kmain(multiboot_header* mb)
        └─ print("Multiboot detected !")  to VGA text buffer
```

---

## Project structure

```
type1-hypervisor-base/
├── Makefile                 # build pipeline: nasm + g++ + ld + grub-mkrescue
├── linker.ld                # link at 0x00100000, keep multiboot_header section
├── src/
│   ├── multiboot.h          # Multiboot 1 header/info structures (from GRUB docs)
│   ├── bootloader.asm       # 32-bit entry: paging, long mode, GDT64, far jump
│   ├── kernelloader.asm     # 64-bit entry: clear segment regs, call kmain
│   └── kernel.cpp           # kmain + raw VGA text-mode printer
├── Hypervisor/
│   └── boot/
│       ├── grub/grub.cfg    # GRUB menuentry that multiboots /boot/kernel.bin
│       └── kernel.bin       # produced by `make` (staged for ISO creation)
└── Hypervisor.iso           # bootable image produced by `make iso`
```

---

## Requirements

Build tools (Linux host recommended):

- `gcc` / `g++` (with 64-bit support, `-m64`)
- `nasm`
- `ld` (GNU binutils)
- `grub-mkrescue` (from `grub2-common` / `xorriso` + `mtools`)
- `make`

For running:

- `qemu-system-x86_64` (recommended), or any PC capable of booting from USB/ISO.

---

## Building

```sh
make           # builds kernel.bin, creates Hypervisor.iso, cleans object files
make kernel    # only link ./obj/kernel.bin
make iso       # stage kernel.bin into Hypervisor/boot and build the .iso
make clean     # remove *.o and ./obj
```

The build pipeline:

1. `nasm -f elf64 src/bootloader.asm      -o obj/bootloader.o`
2. `nasm -f elf64 src/kernelloader.asm    -o obj/kernelloader.o`
3. `g++  -m64 -fpermissive -c src/kernel.cpp -o obj/kernel.o`
4. `ld   -m elf_x86_64 -T linker.ld        -o obj/kernel.bin`
5. `grub-mkrescue -o Hypervisor.iso Hypervisor` (after staging the kernel)

---

## Running

With QEMU:

```sh
qemu-system-x86_64 -cdrom Hypervisor.iso -boot d
```

On boot you should see (top-left of the screen, white on black):

```
Multiboot detected !
```

---

## Technical details

### Multiboot

- Magic `0x1BADB002`, flags `PAGE_ALIGN | MEM_INFO` (no graphics requested in the
  primary header block).
- A second, bare-bones header (`header_start`, flags `0x0`) is also emitted in
  `bootloader.asm`. GRUB matches the first valid header it finds.
- `linker.ld` keeps the `.multiboot_header` section and links the kernel at
  `0x00100000` (1 MiB), the conventional Multiboot load address.

### Paging / long mode

- Four-level hierarchy: `page_table_l4 -> page_table_l3 -> page_table_l2`.
- `page_table_l2` entries are 2 MB huge pages (`PS=1`, present + writable),
  filling 512 entries => 1 GB identity-mapped, 1:1.
- Enable sequence: `cr3 <- L4`, `cr4.PAE <- 1`, `EFER.LME <- 1`, `cr0.PG <- 1`.
- GDT64 contains a 64-bit code descriptor (`0x00af9a000000ffff`) and a data
  descriptor (`0x00af92000000ffff`); long mode is entered via
  `jmp 0x08:long_mode_start`.

### Switch to 64-bit

`kernelloader.asm` zeroes the segment registers (the null data segments expected in
long mode), preserves the Multiboot information pointer held in `ebx` by moving it to
`esi`, and calls `kmain`.

### VGA text printer

`kernel.cpp` writes ASCII + attribute bytes directly to `0xb8000` (the legacy VGA
text framebuffer at 80x25). The attribute `0x0f` is white-on-black.

---

## Known issues / limitations

- **No interrupt support:** no IDT is installed; any exception will triple-fault the
  machine.
- **No memory allocator:** the page tables in `.bss` are not zeroed before use; in
  practice this works under QEMU but may break on real hardware or with stricter
  bootloaders.
- **`kmain` argument type:** GRUB passes a pointer to `multiboot_info` in `ebx`, but
  `kmain` is declared as `kmain(multiboot_header*)`. The `magic` field check is
  therefore effectively reading the wrong structure; the printed message is not a
  reliable Multiboot detection.
- **Standard C++ headers:** `kernel.cpp` includes `<cstring>`, `<iostream>`,
  `<ostream>`, `<wchar.h>` but is freestanding (no libc runtime). These includes are
  unused and should be removed; including them risks pulling hosted-only code.
- **Page-table access bits:** PTE flags set to `0b10000011` (PS + R/W + Present) but
  the two high nibbles of the address are written via `eax` in `setup_page_tables`,
  meaning the physical base above 4 GB is never set (fine for the first 1 GB, but
  worth noting before extending the map).
- **No SMP / guest / VMX / SVM:** this is only the boot skeleton for a hypervisor.

---

## Roadmap

Typical next steps to turn this base into a real Type 1 hypervisor:

1. Install a 64-bit IDT and a bare-bones exception/IRQ handler.
2. Properly parse `multiboot_info` (memory map, modules) instead of
   `multiboot_header`.
3. Add a physical-frame allocator and a virtual memory manager (kmalloc / vmalloc).
4. Implement console output independent of VGA (serial port 0x3F8).
5. Enable VMX (Intel) / SVM (AMD) and implement the VM exit handler loop.
6. Add a guest loader and basic EPT/NP paging.
7. SMP bring-up (APIC, IPIs) for multi-vCPU guests.

---

## License

This project is licensed under the **GNU General Public License v2** — see
[LICENSE](./LICENSE) for the full text. `src/multiboot.h` is taken from the GRUB
Multiboot documentation and is likewise distributed under the GNU GPL v2 (see the
copyright header inside that file).