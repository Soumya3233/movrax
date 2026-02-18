#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// PS/2 Keyboard ports
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

// Status flags
#define KEYBOARD_OUTPUT_FULL 0x01

// Special keys
#define KEY_BACKSPACE   0x0E
#define KEY_ENTER       0x1C
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_CAPS        0x3A
#define KEY_ESCAPE      0x01
#define KEY_TAB         0x0F
#define KEY_CTRL        0x1D
#define KEY_ALT         0x38
#define KEY_SPACE       0x39
#define KEY_UP          0x48
#define KEY_DOWN        0x50
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D

// Keyboard class
class Keyboard {
private:
    char scancode_to_ascii(uint8_t scancode);
    
public:
    bool shift_pressed;
    bool caps_lock;
    
    Keyboard();
    void initialize();
    
    // Check if key is available
    bool has_key();
    
    // Get raw scancode
    uint8_t get_scancode();
    
    // Get character (blocking)
    char getchar();
    
    // Get character (non-blocking, returns 0 if none)
    char poll_char();
    
    // Convert scancode to ASCII (for external use with custom modifier tracking)
    char convert_scancode(uint8_t scancode, bool shift, bool caps);
    
    // Handle modifier key press/release
    void handle_modifier(uint8_t scancode, bool released);
};

// Port I/O functions
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Global keyboard instance
extern Keyboard keyboard;

#endif // KEYBOARD_H
