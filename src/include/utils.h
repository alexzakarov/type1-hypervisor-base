#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

extern int line;
extern int column;
extern const char scancode_to_ascii_table[];

void print_char(char c);
void print(const char* str);
void print_int(uint64_t num);
void print_hex(uint64_t num);
void clear_screen(void);
const char* get_ascii_from_scancode(uint8_t scancode);

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
