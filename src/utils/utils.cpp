#include "utils.h"

int line = 0;
int column = 0;

const char scancode_to_ascii_table[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
  '9', '0', '-', '=', '\b',
  '\t',
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.',
  '/',   0,
  '*',
    0,
  ' '
};

static void scroll_screen() {
    char* video_memory = (char*)0xb8000;
    int bytes_per_line = 80 * 2;

    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < bytes_per_line; col++) {
            video_memory[row * bytes_per_line + col] =
                video_memory[(row + 1) * bytes_per_line + col];
        }
    }

    int last_offset = 24 * bytes_per_line;
    for (int col = 0; col < 80; col++) {
        video_memory[last_offset + col * 2] = ' ';
        video_memory[last_offset + col * 2 + 1] = 0x0F;
    }
}

void print_char(char c) {
    char* video_memory = (char*)0xb8000;

    if (c == '\n') {
        line++;
        column = 0;
        if (line >= 25) {
            scroll_screen();
            line = 24;
        }
        return;
    }

    if (column >= 80) {
        column = 0;
        line++;
        if (line >= 25) {
            scroll_screen();
            line = 24;
        }
    }

    int offset = (line * 80 * 2) + (column * 2);
    video_memory[offset] = c;
    video_memory[offset + 1] = 0x0F;
    column++;
}

void print_int(uint64_t num) {
    if (num == 0) {
        print_char('0');
        print_char('\n');
        return;
    }

    char buffer[32];
    int i = 0;

    while (num > 0 && i < 30) {
        buffer[i] = (num % 10) + '0';
        i++;
        num /= 10;
    }

    for (int j = i - 1; j >= 0; j--) {
        print_char(buffer[j]);
    }

    print_char('\n');
}

void print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            line++;
            column = 0;
            if (line >= 25) {
                scroll_screen();
                line = 24;
            }
            continue;
        }
        print_char(str[i]);
    }
    line++;
    column = 0;
    if (line >= 25) {
        scroll_screen();
        line = 24;
    }
}

void print_hex(uint64_t num) {
    print_char('0');
    print_char('x');

    char hex_chars[] = "0123456789ABCDEF";
    char buffer[16];

    for (int i = 15; i >= 0; i--) {
        buffer[i] = hex_chars[num & 0xF];
        num >>= 4;
    }

    for (int i = 0; i < 16; i++) {
        print_char(buffer[i]);
    }
}

void clear_screen(void) {
    char* video_memory = (char*)0xb8000;

    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i * 2] = ' ';
        video_memory[i * 2 + 1] = 0x0F;
    }

    line = 0;
    column = 0;
}

const char* get_ascii_from_scancode(uint8_t scancode) {
    if (scancode & 0x80) {
        return nullptr;
    }

    if (scancode < sizeof(scancode_to_ascii_table)) {
        return &scancode_to_ascii_table[scancode];
    }

    return nullptr;
}
