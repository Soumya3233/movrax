#include "shell.h"
#include "audit.h"
#include "editor.h"
#include "elf.h"
#include "fs.h"
#include "heap.h"
#include "integrity.h"
#include "keyboard.h"
#include "vga.h"
#include "watchdog.h"

// Global shell instance
Shell shell;

Shell::Shell() : buffer_pos(0), running(false) {
  for (int i = 0; i < SHELL_BUFFER_SIZE; i++) {
    input_buffer[i] = 0;
  }
}

void Shell::initialize() {
  buffer_pos = 0;
  running = true;

  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("\n=====================================\n");
  terminal.write_string("  MOVRAX Shell v1.0\n");
  terminal.write_string("  Type 'help' for available commands\n");
  terminal.write_string("=====================================\n\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void Shell::print_prompt() {
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  terminal.write_string("movrax");
  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string(":");
  terminal.write_string(fs.getcwd());
  terminal.set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
  terminal.write_string("$ ");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void Shell::read_line() {
  buffer_pos = 0;
  uint32_t cursor_pos = 0;
  input_buffer[0] = '\0';

  while (true) {
    uint8_t scancode = keyboard.get_scancode();

    // Handle key releases
    if (scancode & 0x80) {
      keyboard.handle_modifier(scancode & 0x7F, true);
      continue;
    }

    // Handle modifier keys
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT ||
        scancode == KEY_CAPS) {
      keyboard.handle_modifier(scancode, false);
      continue;
    }

    // Handle special keys
    if (scancode == KEY_ENTER) {
      terminal.write_string("\n");
      input_buffer[buffer_pos] = '\0';
      return;
    } else if (scancode == KEY_BACKSPACE) {
      if (cursor_pos > 0) {
        for (uint32_t i = cursor_pos - 1; i < buffer_pos - 1; i++) {
          input_buffer[i] = input_buffer[i + 1];
        }
        buffer_pos--;
        cursor_pos--;
        input_buffer[buffer_pos] = '\0';

        terminal.write_string("\b");
        for (uint32_t i = cursor_pos; i < buffer_pos; i++) {
          char s[2] = {input_buffer[i], '\0'};
          terminal.write_string(s);
        }
        terminal.write_string(" \b");
        for (uint32_t i = cursor_pos; i < buffer_pos; i++) {
          terminal.write_string("\b");
        }
      }
    } else if (scancode == KEY_LEFT) {
      if (cursor_pos > 0) {
        cursor_pos--;
        terminal.write_string("\b");
      }
    } else if (scancode == KEY_RIGHT) {
      if (cursor_pos < buffer_pos) {
        cursor_pos++;
        terminal.putchar(input_buffer[cursor_pos - 1]);
      }
    } else {
      // Use keyboard's unified conversion
      char c = keyboard.convert_scancode(scancode, keyboard.shift_pressed,
                                         keyboard.caps_lock);

      if (c && c != '\t' && c != '\n' && c != '\b' &&
          buffer_pos < SHELL_BUFFER_SIZE - 1) {
        for (uint32_t i = buffer_pos; i > cursor_pos; i--) {
          input_buffer[i] = input_buffer[i - 1];
        }
        input_buffer[cursor_pos] = c;
        buffer_pos++;
        cursor_pos++;
        input_buffer[buffer_pos] = '\0';

        for (uint32_t i = cursor_pos - 1; i < buffer_pos; i++) {
          char s[2] = {input_buffer[i], '\0'};
          terminal.write_string(s);
        }
        for (uint32_t i = cursor_pos; i < buffer_pos; i++) {
          terminal.write_string("\b");
        }
      }
    }
  }
}

void Shell::parse_command(char *args[], int *argc) {
  *argc = 0;
  char *ptr = input_buffer;

  while (*ptr && *argc < MAX_ARGS) {
    // Skip whitespace
    while (*ptr == ' ')
      ptr++;
    if (!*ptr)
      break;

    // Start of argument
    args[(*argc)++] = ptr;

    // Find end of argument
    while (*ptr && *ptr != ' ')
      ptr++;
    if (*ptr) {
      *ptr = '\0';
      ptr++;
    }
  }
}

void Shell::cmd_help() {
  terminal.set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
  terminal.write_string("Available commands:\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("  help              - Show this help\n");
  terminal.write_string("  clear             - Clear screen\n");
  terminal.write_string("  ls                - List files in current dir\n");
  terminal.write_string("  cd <dir>          - Change directory\n");
  terminal.write_string("  pwd               - Print working directory\n");
  terminal.write_string("  mkdir <name>      - Create directory\n");
  terminal.write_string("  cat <file>        - Show file contents\n");
  terminal.write_string("  write <file> text - Write text to file\n");
  terminal.write_string("  edit <file>       - Edit file (ESC to save/exit)\n");
  terminal.write_string("  rm <file>         - Remove file/empty dir\n");
  terminal.write_string("  info              - Show filesystem info\n");
  terminal.write_string("  echo <text>       - Print text\n");
  terminal.write_string(
      "  audit [n]         - Show last n audit log entries\n");
  terminal.write_string("  heap              - Show kernel heap status\n");
  terminal.write_string("  fscheck           - Filesystem integrity check\n");
  terminal.write_string("  watchdog          - Watchdog timer status\n");
  terminal.write_string("  status            - Full system status\n");
}

void Shell::cmd_clear() { terminal.clear(); }

void Shell::cmd_ls() {
  uint32_t dir = fs.get_cwd_index();
  uint32_t count = fs.get_dir_file_count(dir);

  if (count == 0) {
    terminal.set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal.write_string("(empty directory)\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  char name[32];
  uint32_t size;
  uint8_t type;

  for (uint32_t i = 0; i < count; i++) {
    if (fs.get_file_info(dir, i, name, &size, &type)) {
      if (type == FILE_TYPE_DIR) {
        terminal.set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        terminal.write_string(name);
        terminal.write_string("/");
      } else {
        terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        terminal.write_string(name);
      }
      terminal.set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
      terminal.write_string("  (");

      char buf[16];
      uint32_t n = size;
      int j = 0;
      if (n == 0)
        buf[j++] = '0';
      while (n > 0) {
        buf[j++] = '0' + (n % 10);
        n /= 10;
      }
      while (j > 0) {
        char c[2] = {buf[--j], '\0'};
        terminal.write_string(c);
      }

      terminal.write_string(" bytes)\n");
      terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
  }
}

void Shell::cmd_cat(const char *filename) {
  if (!filename) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: cat <filename>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  char buffer[1024];
  int32_t size = fs.read(filename, buffer, sizeof(buffer) - 1);

  if (size < 0) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("File not found: ");
    terminal.write_string(filename);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  buffer[size] = '\0';
  terminal.write_string(buffer);
  terminal.write_string("\n");
}

void Shell::cmd_write(const char *filename, char *args[], int argc,
                      int start_arg) {
  if (!filename || argc <= start_arg) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: write <filename> <content>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  // Concatenate remaining args as content
  char content[512];
  uint32_t pos = 0;

  for (int i = start_arg; i < argc && pos < sizeof(content) - 1; i++) {
    if (i > start_arg && pos < sizeof(content) - 1) {
      content[pos++] = ' ';
    }
    const char *arg = args[i];
    while (*arg && pos < sizeof(content) - 1) {
      content[pos++] = *arg++;
    }
  }
  content[pos] = '\0';

  if (fs.write(filename, content, pos)) {
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("Written to ");
    terminal.write_string(filename);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  } else {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Failed to write file\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  }
}

void Shell::cmd_rm(const char *filename) {
  if (!filename) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: rm <filename>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  if (fs.remove(filename)) {
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("Removed: ");
    terminal.write_string(filename);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  } else {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("File not found: ");
    terminal.write_string(filename);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  }
}

void Shell::cmd_info() {
  terminal.set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
  terminal.write_string("Filesystem Info:\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  terminal.write_string("  Files: ");
  uint32_t count = fs.get_file_count();
  char buf[16];
  int i = 0;
  if (count == 0)
    buf[i++] = '0';
  while (count > 0) {
    buf[i++] = '0' + (count % 10);
    count /= 10;
  }
  while (i > 0) {
    char c[2] = {buf[--i], '\0'};
    terminal.write_string(c);
  }
  terminal.write_string("\n");

  terminal.write_string("  Used:  ");
  uint32_t used = fs.get_used_space();
  i = 0;
  if (used == 0)
    buf[i++] = '0';
  while (used > 0) {
    buf[i++] = '0' + (used % 10);
    used /= 10;
  }
  while (i > 0) {
    char c[2] = {buf[--i], '\0'};
    terminal.write_string(c);
  }
  terminal.write_string(" bytes\n");

  terminal.write_string("  Free:  ");
  uint32_t free = fs.get_free_space();
  i = 0;
  if (free == 0)
    buf[i++] = '0';
  while (free > 0) {
    buf[i++] = '0' + (free % 10);
    free /= 10;
  }
  while (i > 0) {
    char c[2] = {buf[--i], '\0'};
    terminal.write_string(c);
  }
  terminal.write_string(" bytes\n");
}

void Shell::cmd_echo(char *args[], int argc) {
  for (int i = 1; i < argc; i++) {
    if (i > 1)
      terminal.write_string(" ");
    terminal.write_string(args[i]);
  }
  terminal.write_string("\n");
}

void Shell::cmd_edit(const char *filename) {
  if (!filename) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: edit <filename>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  editor.open(filename);
  editor.run();

  // Re-display shell header after editor exits
  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("\nReturned to shell.\n\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void Shell::cmd_cd(const char *path) {
  if (!path) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: cd <directory>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  if (!fs.chdir(path)) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Directory not found: ");
    terminal.write_string(path);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  }
}

void Shell::cmd_pwd() {
  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string(fs.getcwd());
  terminal.write_string("\n");
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void Shell::cmd_mkdir(const char *name) {
  if (!name) {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Usage: mkdir <name>\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return;
  }

  if (fs.mkdir(name)) {
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("Created directory: ");
    terminal.write_string(name);
    terminal.write_string("\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  } else {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("Failed to create directory (already exists?)\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  }
}

void Shell::execute_command() {
  char *args[MAX_ARGS];
  int argc;

  parse_command(args, &argc);

  if (argc == 0)
    return;

  if (str_compare(args[0], "help") == 0) {
    cmd_help();
  } else if (str_compare(args[0], "clear") == 0) {
    cmd_clear();
  } else if (str_compare(args[0], "ls") == 0) {
    cmd_ls();
  } else if (str_compare(args[0], "cat") == 0) {
    cmd_cat(argc > 1 ? args[1] : nullptr);
  } else if (str_compare(args[0], "write") == 0) {
    cmd_write(argc > 1 ? args[1] : nullptr, args, argc, 2);
  } else if (str_compare(args[0], "rm") == 0) {
    cmd_rm(argc > 1 ? args[1] : nullptr);
  } else if (str_compare(args[0], "info") == 0) {
    cmd_info();
  } else if (str_compare(args[0], "echo") == 0) {
    cmd_echo(args, argc);
  } else if (str_compare(args[0], "edit") == 0) {
    cmd_edit(argc > 1 ? args[1] : nullptr);
  } else if (str_compare(args[0], "cd") == 0) {
    cmd_cd(argc > 1 ? args[1] : nullptr);
  } else if (str_compare(args[0], "pwd") == 0) {
    cmd_pwd();
  } else if (str_compare(args[0], "mkdir") == 0) {
    cmd_mkdir(argc > 1 ? args[1] : nullptr);
  } else if (str_compare(args[0], "audit") == 0) {
    // Parse optional count argument
    uint32_t count = 20;
    if (argc > 1) {
      count = 0;
      for (int i = 0; args[1][i]; i++) {
        count = count * 10 + (args[1][i] - '0');
      }
    }
    AuditLog::dump(count);
  } else if (str_compare(args[0], "heap") == 0) {
    terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal.write_string("=== Kernel Heap Status ===\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  Used:  ");
    // Simple number print
    uint32_t used = kheap.get_used();
    uint32_t free_mem = kheap.get_free();
    uint32_t total = kheap.get_total();
    char buf[12];
    int bi;
    bi = 0;
    if (used == 0)
      buf[bi++] = '0';
    else {
      uint32_t v = used;
      while (v) {
        buf[bi++] = '0' + (v % 10);
        v /= 10;
      }
    }
    while (bi > 0) {
      char c[2] = {buf[--bi], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string(" bytes\n  Free:  ");
    bi = 0;
    if (free_mem == 0)
      buf[bi++] = '0';
    else {
      uint32_t v = free_mem;
      while (v) {
        buf[bi++] = '0' + (v % 10);
        v /= 10;
      }
    }
    while (bi > 0) {
      char c[2] = {buf[--bi], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string(" bytes\n  Total: ");
    bi = 0;
    if (total == 0)
      buf[bi++] = '0';
    else {
      uint32_t v = total;
      while (v) {
        buf[bi++] = '0' + (v % 10);
        v /= 10;
      }
    }
    while (bi > 0) {
      char c[2] = {buf[--bi], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string(" bytes\n");
  } else if (str_compare(args[0], "fscheck") == 0) {
    FSIntegrity::report();
  } else if (str_compare(args[0], "watchdog") == 0) {
    terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal.write_string("=== Watchdog Status ===\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  Enabled:    ");
    if (Watchdog::is_enabled()) {
      terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
      terminal.write_string("YES\n");
    } else {
      terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
      terminal.write_string("NO\n");
    }
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  Kicks:      ");
    char kb[12];
    int ki = 0;
    uint32_t kc = Watchdog::get_kick_count();
    if (kc == 0)
      kb[ki++] = '0';
    else {
      while (kc) {
        kb[ki++] = '0' + (kc % 10);
        kc /= 10;
      }
    }
    while (ki > 0) {
      char c[2] = {kb[--ki], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string("\n  Violations: ");
    ki = 0;
    kc = Watchdog::get_violations();
    if (kc == 0)
      kb[ki++] = '0';
    else {
      while (kc) {
        kb[ki++] = '0' + (kc % 10);
        kc /= 10;
      }
    }
    while (ki > 0) {
      char c[2] = {kb[--ki], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string("\n");
  } else if (str_compare(args[0], "status") == 0) {
    terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal.write_string("=== MOVRAX System Status ===\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  Watchdog:   ");
    terminal.set_color(Watchdog::is_enabled() ? VGA_COLOR_LIGHT_GREEN
                                              : VGA_COLOR_LIGHT_RED,
                       VGA_COLOR_BLACK);
    terminal.write_string(Watchdog::is_enabled() ? "ARMED\n" : "DISABLED\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  Heap free:  ");
    char sb[12];
    int si = 0;
    uint32_t sf = kheap.get_free() / 1024;
    if (sf == 0)
      sb[si++] = '0';
    else {
      while (sf) {
        sb[si++] = '0' + (sf % 10);
        sf /= 10;
      }
    }
    while (si > 0) {
      char c[2] = {sb[--si], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string(" KB\n");
    terminal.write_string("  Audit log:  ");
    si = 0;
    sf = AuditLog::get_count();
    if (sf == 0)
      sb[si++] = '0';
    else {
      while (sf) {
        sb[si++] = '0' + (sf % 10);
        sf /= 10;
      }
    }
    while (si > 0) {
      char c[2] = {sb[--si], '\0'};
      terminal.write_string(c);
    }
    terminal.write_string(" events\n");
    terminal.write_string("  Encrypt:    ");
    terminal.set_color(XORCipher::is_ready() ? VGA_COLOR_LIGHT_GREEN
                                             : VGA_COLOR_YELLOW,
                       VGA_COLOR_BLACK);
    terminal.write_string(XORCipher::is_ready() ? "ACTIVE\n" : "INACTIVE\n");
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal.write_string("  FS Check:   ");
    uint32_t corr = FSIntegrity::verify_all();
    if (corr == 0) {
      terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
      terminal.write_string("CLEAN\n");
    } else {
      terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
      si = 0;
      sf = corr;
      if (sf == 0)
        sb[si++] = '0';
      else {
        while (sf) {
          sb[si++] = '0' + (sf % 10);
          sf /= 10;
        }
      }
      while (si > 0) {
        char c[2] = {sb[--si], '\0'};
        terminal.write_string(c);
      }
      terminal.write_string(" CORRUPTED\n");
    }
    terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  } else {
    // Check if executable file
    if (fs.exists(args[0])) {
      int pid = ELF::load(args[0]);
      if (pid >= 0) {
        terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal.write_string("Process started. PID: ");
        // print pid...
        terminal.write_string("\n");
        terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
      } else {
        terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal.write_string("Failed to execute: ");
        terminal.write_string(args[0]);
        terminal.write_string("\n");
        terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
      }
    } else {
      terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
      terminal.write_string("Unknown command: ");
      terminal.write_string(args[0]);
      terminal.write_string("\n");
      terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
  }
}

void Shell::run() {
  while (running) {
    Watchdog::kick(); // Keep watchdog alive
    print_prompt();
    read_line();
    Watchdog::kick(); // Kick after user input (may have waited)
    execute_command();
  }
}
