#include <cstring>
#include <iostream>
#include <ostream>
#include <wchar.h>

#include "multiboot.h"

int line = 0;
void print (const char* str)
{
    char* video_memory = (char*)0xb8000;
    for (int i = 0; str[i] != '\0'; i++)
    {
        video_memory[line * 80 *2 + i * 2] = str[i];
        video_memory[line * 80 *2 + i * 2 + 1] = 0x0f;
    }
}

extern "C" void kmain(multiboot_header* mb)
{
    if (mb->magic)
    {
        print("Multiboot detected !\n");
    } else
    {
        print("Multiboot not detected !\n");
    }
}
