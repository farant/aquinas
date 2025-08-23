/* Interactive shell for AquinasOS */

#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"

#define BUFFER_SIZE 256
#define MAX_ARGS 10

/* Read a line of input from the user */
void readline(char* buffer, int max_length) {
    int pos = 0;
    char c;
    
    while (pos < max_length - 1) {
        c = getchar();
        
        if (c == '\n') {
            buffer[pos] = '\0';
            putchar('\n');
            return;
        } else if (c == '\b') {  /* Backspace */
            if (pos > 0) {
                pos--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
        } else if (c >= 32 && c < 127) {  /* Printable characters */
            buffer[pos++] = c;
            putchar(c);
        }
    }
    buffer[pos] = '\0';
}

/* Command handlers */
static void cmd_help(void) {
    puts("Available commands:\n");
    puts("  help    - Show this help message\n");
    puts("  clear   - Clear the screen\n");
    puts("  about   - About AquinasOS\n");
    puts("  echo    - Echo text to screen\n");
    puts("  colors  - Show color test\n");
    puts("  reboot  - Restart the system\n");
}

static void cmd_about(void) {
    puts("AquinasOS v0.1\n");
    puts("A simple 32-bit protected mode OS\n");
    puts("Written in C and Assembly\n");
    puts("Features:\n");
    puts("  - Keyboard input\n");
    puts("  - Interactive shell\n");
    puts("  - VGA text mode display\n");
}

static void cmd_echo(const char* args) {
    if (strlen(args) > 0) {
        puts(args);
        puts("\n");
    }
}

static void cmd_colors(void) {
    unsigned short* vga = VGA_BUFFER;
    int start_pos = cursor_pos;
    
    puts("Color test:\n");
    
    /* Show all 16 colors */
    for (int bg = 0; bg < 8; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            unsigned short color = (bg << 12) | (fg << 8);
            vga[cursor_pos++] = color | 'X';
        }
        putchar('\n');
    }
}

static void cmd_reboot(void) {
    puts("Rebooting...\n");
    
    /* Triple fault to reboot */
    __asm__ volatile ("int $0x00");
}

/* Process a command */
void process_command(char* cmd) {
    char args[BUFFER_SIZE];
    int cmd_len = 0;
    
    /* Skip leading spaces */
    while (*cmd == ' ') cmd++;
    
    /* Empty command */
    if (*cmd == '\0') return;
    
    /* Extract command and arguments */
    while (cmd[cmd_len] && cmd[cmd_len] != ' ') {
        cmd_len++;
    }
    
    /* Copy arguments if any */
    if (cmd[cmd_len] == ' ') {
        strcpy(args, cmd + cmd_len + 1);
    } else {
        args[0] = '\0';
    }
    
    /* Match commands */
    if (strncmp(cmd, "help", cmd_len) == 0) {
        cmd_help();
    } else if (strncmp(cmd, "clear", cmd_len) == 0) {
        clear_screen();
    } else if (strncmp(cmd, "about", cmd_len) == 0) {
        cmd_about();
    } else if (strncmp(cmd, "echo", cmd_len) == 0) {
        cmd_echo(args);
    } else if (strncmp(cmd, "colors", cmd_len) == 0) {
        cmd_colors();
    } else if (strncmp(cmd, "reboot", cmd_len) == 0) {
        cmd_reboot();
    } else {
        puts("Unknown command: ");
        for (int i = 0; i < cmd_len; i++) {
            putchar(cmd[i]);
        }
        puts("\nType 'help' for available commands\n");
    }
}

/* Main shell loop */
void shell_run(void) {
    char buffer[BUFFER_SIZE];
    
    puts("\nAquinasOS Shell v0.1\n");
    puts("Type 'help' for available commands\n\n");
    
    while (1) {
        puts("> ");
        readline(buffer, BUFFER_SIZE);
        process_command(buffer);
    }
}