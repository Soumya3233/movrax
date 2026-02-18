#ifndef SHELL_H
#define SHELL_H

#include "types.h"

#define SHELL_BUFFER_SIZE 256
#define MAX_ARGS 10

// Shell class
class Shell {
private:
    char input_buffer[SHELL_BUFFER_SIZE];
    uint32_t buffer_pos;
    bool running;
    
    // Command parsing
    void parse_command(char* args[], int* argc);
    
    // Built-in commands
    void cmd_help();
    void cmd_clear();
    void cmd_ls();
    void cmd_cat(const char* filename);
    void cmd_write(const char* filename, char* args[], int argc, int start_arg);
    void cmd_rm(const char* filename);
    void cmd_info();
    void cmd_echo(char* args[], int argc);
    void cmd_edit(const char* filename);
    void cmd_cd(const char* path);
    void cmd_pwd();
    void cmd_mkdir(const char* name);
    
    // Input handling
    void read_line();
    void execute_command();
    
public:
    Shell();
    void initialize();
    void run();  // Main shell loop
    void print_prompt();
};

// Global shell instance
extern Shell shell;

#endif // SHELL_H
