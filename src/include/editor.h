#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"

// Editor constants
#define EDITOR_MAX_LINES    100
#define EDITOR_MAX_LINE_LEN 78     // 80 - 2 for line numbers
#define EDITOR_BUFFER_SIZE  8192   // 8KB max file

// Editor class
class TextEditor {
private:
    char* file_buffer;           // Points to file in persistent memory
    uint32_t file_size;
    char filename[32];
    bool modified;
    bool running;
    
    // Editing buffer
    char buffer[EDITOR_BUFFER_SIZE];
    uint32_t buffer_len;
    
    // Cursor
    uint32_t cursor_pos;
    uint32_t cursor_line;
    uint32_t cursor_col;
    uint32_t scroll_offset;      // First visible line
    
    // Screen dimensions
    static const uint32_t SCREEN_LINES = 20;
    static const uint32_t SCREEN_COLS = 78;
    
    // Helper methods
    void load_file();
    void save_file();
    void draw_screen();
    void draw_status_bar();
    void move_cursor(int delta);
    void move_line(int delta);
    void insert_char(char c);
    void delete_char();
    void calculate_cursor_position();
    uint32_t get_line_start(uint32_t line);
    uint32_t get_line_end(uint32_t line);
    uint32_t count_lines();
    
public:
    TextEditor();
    
    // Open file for editing
    bool open(const char* name);
    
    // Run editor loop
    void run();
};

// Global editor instance
extern TextEditor editor;

#endif // EDITOR_H
