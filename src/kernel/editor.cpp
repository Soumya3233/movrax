#include "editor.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"

// Global editor instance
TextEditor editor;

TextEditor::TextEditor() 
    : file_buffer(nullptr), file_size(0), modified(false), running(false),
      buffer_len(0), cursor_pos(0), cursor_line(0), cursor_col(0), scroll_offset(0) {
    filename[0] = '\0';
    for (uint32_t i = 0; i < EDITOR_BUFFER_SIZE; i++) {
        buffer[i] = 0;
    }
}

bool TextEditor::open(const char* name) {
    str_copy(filename, name, 32);
    load_file();
    return true;
}

void TextEditor::load_file() {
    int32_t size = fs.read(filename, buffer, EDITOR_BUFFER_SIZE - 1);
    if (size >= 0) {
        buffer_len = size;
        buffer[buffer_len] = '\0';
    } else {
        // New file
        buffer_len = 0;
        buffer[0] = '\0';
    }
    cursor_pos = 0;
    cursor_line = 0;
    cursor_col = 0;
    scroll_offset = 0;
    modified = false;
}

void TextEditor::save_file() {
    fs.write(filename, buffer, buffer_len);
    modified = false;
}

uint32_t TextEditor::count_lines() {
    uint32_t lines = 1;
    for (uint32_t i = 0; i < buffer_len; i++) {
        if (buffer[i] == '\n') lines++;
    }
    return lines;
}

uint32_t TextEditor::get_line_start(uint32_t line) {
    if (line == 0) return 0;
    uint32_t current_line = 0;
    for (uint32_t i = 0; i < buffer_len; i++) {
        if (buffer[i] == '\n') {
            current_line++;
            if (current_line == line) return i + 1;
        }
    }
    return buffer_len;
}

uint32_t TextEditor::get_line_end(uint32_t line) {
    uint32_t start = get_line_start(line);
    for (uint32_t i = start; i < buffer_len; i++) {
        if (buffer[i] == '\n') return i;
    }
    return buffer_len;
}

void TextEditor::calculate_cursor_position() {
    cursor_line = 0;
    cursor_col = 0;
    uint32_t line_start = 0;
    
    for (uint32_t i = 0; i < cursor_pos && i < buffer_len; i++) {
        if (buffer[i] == '\n') {
            cursor_line++;
            line_start = i + 1;
        }
    }
    cursor_col = cursor_pos - line_start;
}

void TextEditor::draw_screen() {
    terminal.clear();
    
    // Title bar
    terminal.set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
    terminal.write_string(" MOVRAX Editor - ");
    terminal.write_string(filename);
    if (modified) terminal.write_string(" [modified]");
    // Pad to full width
    for (int i = 0; i < 40; i++) terminal.write_string(" ");
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Content area
    uint32_t total_lines = count_lines();
    
    for (uint32_t screen_line = 0; screen_line < SCREEN_LINES; screen_line++) {
        uint32_t file_line = scroll_offset + screen_line;
        
        if (file_line < total_lines) {
            // Line number
            terminal.set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            char num[4];
            uint32_t n = file_line + 1;
            int i = 2;
            num[3] = '\0';
            while (i >= 0) {
                num[i--] = (n > 0) ? ('0' + n % 10) : ' ';
                n /= 10;
            }
            terminal.write_string(num);
            terminal.write_string(" ");
            
            terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            
            // Line content
            uint32_t start = get_line_start(file_line);
            uint32_t end = get_line_end(file_line);
            uint32_t len = end - start;
            if (len > SCREEN_COLS) len = SCREEN_COLS;
            
            for (uint32_t i = 0; i < len; i++) {
                char c = buffer[start + i];
                if (c != '\n') {
                    // Highlight cursor position
                    if (start + i == cursor_pos) {
                        terminal.set_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
                    }
                    char s[2] = {c, '\0'};
                    terminal.write_string(s);
                    if (start + i == cursor_pos) {
                        terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                    }
                }
            }
            
            // Cursor at end of line
            if (cursor_pos == end && file_line == cursor_line) {
                terminal.set_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
                terminal.write_string(" ");
                terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            }
        }
        terminal.write_string("\n");
    }
    
    // Status bar
    draw_status_bar();
}

void TextEditor::draw_status_bar() {
    terminal.set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
    terminal.write_string(" Line ");
    
    // Print line number
    char buf[8];
    uint32_t n = cursor_line + 1;
    int i = 0;
    do { buf[i++] = '0' + (n % 10); n /= 10; } while (n > 0);
    while (i > 0) { char c[2] = {buf[--i], '\0'}; terminal.write_string(c); }
    
    terminal.write_string(", Col ");
    n = cursor_col + 1;
    i = 0;
    do { buf[i++] = '0' + (n % 10); n /= 10; } while (n > 0);
    while (i > 0) { char c[2] = {buf[--i], '\0'}; terminal.write_string(c); }
    
    terminal.write_string(" | ESC: Save & Exit");
    for (int j = 0; j < 30; j++) terminal.write_string(" ");
    
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void TextEditor::insert_char(char c) {
    if (buffer_len >= EDITOR_BUFFER_SIZE - 1) return;
    
    // Shift everything after cursor
    for (uint32_t i = buffer_len; i > cursor_pos; i--) {
        buffer[i] = buffer[i - 1];
    }
    buffer[cursor_pos] = c;
    buffer_len++;
    buffer[buffer_len] = '\0';
    cursor_pos++;
    modified = true;
    calculate_cursor_position();
}

void TextEditor::delete_char() {
    if (cursor_pos == 0) return;
    
    // Shift everything after cursor
    cursor_pos--;
    for (uint32_t i = cursor_pos; i < buffer_len - 1; i++) {
        buffer[i] = buffer[i + 1];
    }
    buffer_len--;
    buffer[buffer_len] = '\0';
    modified = true;
    calculate_cursor_position();
}

void TextEditor::move_cursor(int delta) {
    int32_t new_pos = (int32_t)cursor_pos + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > (int32_t)buffer_len) new_pos = buffer_len;
    cursor_pos = new_pos;
    calculate_cursor_position();
    
    // Adjust scroll
    if (cursor_line < scroll_offset) {
        scroll_offset = cursor_line;
    }
    if (cursor_line >= scroll_offset + SCREEN_LINES) {
        scroll_offset = cursor_line - SCREEN_LINES + 1;
    }
}

void TextEditor::move_line(int delta) {
    uint32_t target_line;
    if (delta < 0 && cursor_line == 0) return;
    if (delta < 0) {
        target_line = cursor_line - 1;
    } else {
        target_line = cursor_line + 1;
        if (target_line >= count_lines()) return;
    }
    
    uint32_t line_start = get_line_start(target_line);
    uint32_t line_end = get_line_end(target_line);
    uint32_t line_len = line_end - line_start;
    
    if (cursor_col > line_len) {
        cursor_pos = line_end;
    } else {
        cursor_pos = line_start + cursor_col;
    }
    
    calculate_cursor_position();
    
    // Adjust scroll
    if (cursor_line < scroll_offset) {
        scroll_offset = cursor_line;
    }
    if (cursor_line >= scroll_offset + SCREEN_LINES) {
        scroll_offset = cursor_line - SCREEN_LINES + 1;
    }
}

void TextEditor::run() {
    running = true;
    draw_screen();
    
    while (running) {
        if (keyboard.has_key()) {
            uint8_t scancode = keyboard.get_scancode();
            
            // Handle key releases
            if (scancode & 0x80) {
                keyboard.handle_modifier(scancode & 0x7F, true);
                continue;
            }
            
            // Handle modifier keys
            if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT || scancode == KEY_CAPS) {
                keyboard.handle_modifier(scancode, false);
                continue;
            }
            
            switch (scancode) {
                case KEY_ESCAPE:
                    save_file();
                    running = false;
                    break;
                    
                case KEY_UP:
                    move_line(-1);
                    draw_screen();
                    break;
                    
                case KEY_DOWN:
                    move_line(1);
                    draw_screen();
                    break;
                    
                case KEY_LEFT:
                    move_cursor(-1);
                    draw_screen();
                    break;
                    
                case KEY_RIGHT:
                    move_cursor(1);
                    draw_screen();
                    break;
                    
                case KEY_BACKSPACE:
                    delete_char();
                    draw_screen();
                    break;
                    
                case KEY_ENTER:
                    insert_char('\n');
                    draw_screen();
                    break;
                    
                default: {
                    char c = keyboard.convert_scancode(scancode, keyboard.shift_pressed, keyboard.caps_lock);
                    if (c && c != '\t' && c != '\n' && c != '\b') {
                        insert_char(c);
                        draw_screen();
                    }
                }
            }
        }
    }
    
    terminal.clear();
}

