#include "keyboard.h"

// Global keyboard instance
Keyboard keyboard;

// US keyboard layout - lowercase
static const char scancode_ascii_lower[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',  // 0x00-0x07
    '7', '8', '9', '0', '-', '=',  0,   '\t', // 0x08-0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  // 0x10-0x17
    'o', 'p', '[', ']', '\n', 0,  'a', 's',  // 0x18-0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  // 0x20-0x27
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v', // 0x28-0x2F
    'b', 'n', 'm', ',', '.', '/',  0,  '*',  // 0x30-0x37
    0,   ' ',  0,   0,   0,   0,   0,   0,   // 0x38-0x3F
};

// US keyboard layout - uppercase
static const char scancode_ascii_upper[] = {
    0,    0,   '!', '@', '#', '$', '%', '^',  // 0x00-0x07
    '&', '*', '(', ')', '_', '+',  0,   '\t', // 0x08-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',  // 0x10-0x17
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',  // 0x18-0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  // 0x20-0x27
    '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',   // 0x28-0x2F
    'B', 'N', 'M', '<', '>', '?',  0,  '*',  // 0x30-0x37
    0,   ' ',  0,   0,   0,   0,   0,   0,   // 0x38-0x3F
};

Keyboard::Keyboard() : shift_pressed(false), caps_lock(false) {
}

void Keyboard::initialize() {
    shift_pressed = false;
    caps_lock = false;
    
    // Wait for keyboard to be ready and clear buffer
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }
}

bool Keyboard::has_key() {
    return inb(KEYBOARD_STATUS_PORT) & KEYBOARD_OUTPUT_FULL;
}

uint8_t Keyboard::get_scancode() {
    while (!has_key()) {
        // Wait for key
        // Wait for key (busy wait is safer for polling setup)
        asm volatile("pause");
    }
    return inb(KEYBOARD_DATA_PORT);
}

char Keyboard::scancode_to_ascii(uint8_t scancode) {
    // Handle key release (high bit set)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == KEY_LSHIFT || released == KEY_RSHIFT) {
            shift_pressed = false;
        }
        return 0;
    }
    
    // Handle special keys
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = true;
            return 0;
        case KEY_CAPS:
            caps_lock = !caps_lock;
            return 0;
        case KEY_BACKSPACE:
            return '\b';
        case KEY_ENTER:
            return '\n';
        case KEY_ESCAPE:
            return 27;  // ESC
    }
    
    // Regular character
    if (scancode < sizeof(scancode_ascii_lower)) {
        bool use_upper = shift_pressed ^ caps_lock;
        return use_upper ? scancode_ascii_upper[scancode] : scancode_ascii_lower[scancode];
    }
    
    return 0;
}

char Keyboard::getchar() {
    char c = 0;
    while (c == 0) {
        uint8_t scancode = get_scancode();
        c = scancode_to_ascii(scancode);
    }
    return c;
}

char Keyboard::poll_char() {
    if (!has_key()) {
        return 0;
    }
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    return scancode_to_ascii(scancode);
}

char Keyboard::convert_scancode(uint8_t scancode, bool shift, bool caps) {
    // Skip special keys
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT || 
        scancode == KEY_CAPS || scancode == KEY_ESCAPE ||
        scancode == KEY_UP || scancode == KEY_DOWN ||
        scancode == KEY_LEFT || scancode == KEY_RIGHT) {
        return 0;
    }
    
    if (scancode == KEY_BACKSPACE) return '\b';
    if (scancode == KEY_ENTER) return '\n';
    
    if (scancode < sizeof(scancode_ascii_lower)) {
        // For letters, caps XOR shift gives uppercase
        bool is_letter = (scancode >= 0x10 && scancode <= 0x19) ||
                        (scancode >= 0x1E && scancode <= 0x26) ||
                        (scancode >= 0x2C && scancode <= 0x32);
        
        bool use_upper;
        if (is_letter) {
            use_upper = shift ^ caps;
        } else {
            use_upper = shift;
        }
        
        return use_upper ? scancode_ascii_upper[scancode] : scancode_ascii_lower[scancode];
    }
    
    return 0;
}

void Keyboard::handle_modifier(uint8_t scancode, bool released) {
    if (released) {
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            shift_pressed = false;
        }
    } else {
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            shift_pressed = true;
        } else if (scancode == KEY_CAPS) {
            caps_lock = !caps_lock;
        }
    }
}

