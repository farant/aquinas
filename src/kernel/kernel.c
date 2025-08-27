/* AquinasOS Text Editor 
 *
 * DESIGN
 * ------
 * 
 * This is a page-based text editor that runs as an operating system kernel.
 * The design emphasizes simplicity and independence between pages.
 * 
 * Architecture:
 * - Each page is completely independent with its own buffer and cursor
 * - Pages don't overflow into each other (no continuous buffer)
 * - Maximum of 100 pages, each holding one screen worth of text (24 lines)
 * - Navigation is done through a clickable navigation bar or keyboard shortcuts
 * 
 * Input handling:
 * - Non-blocking keyboard and mouse polling (no interrupts)
 * - Microsoft Serial Mouse protocol via COM1 (3-byte packets)
 * - Simultaneous mouse and keyboard input processing
 * 
 * Visual features:
 * - VGA text mode (80x25) with blue background
 * - Hardware cursor for text insertion point
 * - Green background for mouse cursor position
 * - Red background for highlighted text (click to select words)
 * - Auto-indentation when pressing Enter
 * 
 * The simplicity is intentional - this allows the editor to be reliable
 * and easy to understand, while still providing essential editing features.
 */

#include "io.h"
#include "serial.h"
#include "vga.h"
#include "timer.h"
#include "rtc.h"
#include "memory.h"
#include "graphics.h"
#include "dispi.h"
#include "display_driver.h"
#include "dispi_demo.h"
#include "page.h"
#include "modes.h"
#include "display.h"
#include "commands.h"
#include "editor.h"
#include "input.h"

/* Editor modes moved to modes.c */

/* Mouse state moved to display.c */

/* Page management functions moved to page.c */
unsigned int get_stack_usage(void);

/* Port I/O functions now in io.h */

/* Mode management functions moved to modes.c */

/* Page navigation functions moved to page.c */

/* Display functions moved to display.c */

/* Command functions moved to commands.c */

/* Editor functions moved to editor.c */

/* Old blocking keyboard handler - kept for reference but not used */
/* keyboard_get_scancode was replaced by non-blocking keyboard_check */

/* Input functions moved to input.c */

/* Kernel entry point */
/* Get current stack pointer value */
unsigned int get_esp(void) {
    unsigned int esp;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(esp));
    return esp;
}

/* Calculate stack usage from initial 2MB position */
unsigned int get_stack_usage(void) {
    unsigned int current_esp = get_esp();
    unsigned int initial_esp = 0x200000;  /* Stack starts at 2MB */
    
    /* Stack grows downward, so usage is the difference */
    if (current_esp < initial_esp) {
        return initial_esp - current_esp;
    }
    return 0;  /* Stack pointer is above initial (shouldn't happen) */
}

/* Get maximum stack depth seen so far */
unsigned int get_max_stack_usage(void) {
    static unsigned int max_usage = 0;
    unsigned int current_usage = get_stack_usage();
    
    if (current_usage > max_usage) {
        max_usage = current_usage;
    }
    return max_usage;
}

/* Text rendering functions moved to dispi.c */
/* DISPI demo moved to dispi_demo.c */

void kernel_main(void) {
    int key;
    
    clear_screen();
    
    /* Initialize page management - must be done AFTER memory init */
    /* This is now handled by init_pages() */
    
    /* DEBUG: Test if highlighting works at all */
    /* pages[0]->highlight_start = 0;
    pages[0]->highlight_end = 5; */
    
    /* Initialize debug serial port (COM2) */
    init_debug_serial();
    serial_write_string("\n\nAquinas OS started!\n");
    serial_write_string("COM2 debug port initialized.\n");
    
    /* Initialize memory allocator */
    init_memory();
    
    /* Initialize pages (must be after memory init) */
    init_pages();
    serial_write_string("Pages initialized: allocated first page at ");
    serial_write_hex((unsigned int)pages[0]);
    serial_write_string(" with buffer at ");
    serial_write_hex((unsigned int)pages[0]->buffer);
    serial_write_string("\n");
    
    /* Report initial heap usage */
    serial_write_string("Initial heap usage: ");
    serial_write_int(get_heap_used());
    serial_write_string(" bytes (Page struct + ");
    serial_write_int(PAGE_SIZE);
    serial_write_string(" byte buffer)\n");
    
    /* Initialize timer system */
    init_timer();
    
    /* Initialize RTC to get current date/time */
    init_rtc();
    
    /* Initialize mouse (uses COM1) */
    init_mouse();
    serial_write_string("Mouse initialized on COM1.\n");
    serial_write_string("Text editor ready.\n");
    

    /* Start with empty screen, ready for typing */
    refresh_screen();

		serial_write_string("Made it past first refresh screen\n");
    
    /* Main editor loop - non-blocking */
    while (1) {
        /* Stack monitoring - report every 5 seconds using timer */
        static unsigned int last_stack_report = 0;
        static unsigned int last_clock_update = 0;
        static int last_key = 0;
        static unsigned int last_key_time = 0;
        static int pending_delete = 0;  /* For 'd' command sequences */
        static int pending_dt = 0;      /* For 'dt' command */
        unsigned int current_time = 0;


        current_time = get_ticks();

        
        if (get_elapsed_ms(last_stack_report) >= 5000) {
            unsigned int current_usage = get_stack_usage();
            unsigned int max_usage = get_max_stack_usage();
            rtc_time_t now;
            
            /* Get current time */
            get_current_time(&now);
            
            /* Print time and stack info */
            serial_write_string("[");
            if (now.hour < 10) serial_write_string("0");
            serial_write_int(now.hour);
            serial_write_string(":");
            if (now.minute < 10) serial_write_string("0");
            serial_write_int(now.minute);
            serial_write_string(":");
            if (now.second < 10) serial_write_string("0");
            serial_write_int(now.second);
            serial_write_string("] Stack: ");
            serial_write_int(current_usage);
            serial_write_string("/");
            serial_write_int(max_usage);
            serial_write_string(" bytes, ESP=");
            serial_write_hex(get_esp());
            serial_write_string("\n");
            
            last_stack_report = current_time;
        }
        
        /* Update clock display every second */
        if (get_elapsed_ms(last_clock_update) >= 1000) {
            draw_nav_bar();  /* Redraw nav bar to update time */
            last_clock_update = current_time;
        }
        
        /* Poll for mouse data (will refresh screen if mouse moves) */
        poll_mouse();
        
        /* Check for keyboard input (non-blocking) */
        key = keyboard_check();
        
        /* Skip all keyboard processing if in graphics mode (ESC handled in graphics_demo) */
        if (graphics_mode_active) {
            continue;
        }
        
        /* Handle 'fd' escape sequence - insert 'f' immediately, delete if 'd' follows */
        if (editor_mode == MODE_INSERT) {
            /* Check if 'd' was typed shortly after 'f' */
            if (key == 'd' && last_key == 'f' && get_elapsed_ms(last_key_time) < FD_ESCAPE_TIMEOUT_MS) {
                /* 'fd' sequence detected - delete the 'f' we just inserted and exit */
                Page *page = pages[current_page];
                if (page->cursor_pos > 0 && page->buffer[page->cursor_pos - 1] == 'f') {
                    int i;
                    /* Delete the 'f' we just typed */
                    page->cursor_pos--;
                    page->length--;
                    /* Shift remaining text left */
                    for (i = page->cursor_pos; i < page->length; i++) {
                        page->buffer[i] = page->buffer[i + 1];
                    }
                    /* Refresh screen to show the 'f' was deleted */
                    refresh_screen();
                }
                /* Exit to normal mode */
                set_mode(MODE_NORMAL);
                key = 0;  /* Suppress the 'd' */
                last_key = 0;  /* Clear the 'f' marker */
            } else if (key == 'f') {
                /* Mark that we typed 'f' for potential 'fd' sequence */
                last_key = 'f';
                last_key_time = current_time;
                /* Let the 'f' be processed normally (inserted) below */
            } else if (key > 0) {
                /* Any other key - clear the 'f' marker */
                last_key = 0;
            }
        } else if (editor_mode == MODE_VISUAL) {
            /* For visual mode, just check for 'fd' to exit without insertion */
            if (key == 'd' && last_key == 'f' && get_elapsed_ms(last_key_time) < FD_ESCAPE_TIMEOUT_MS) {
                /* Exit to normal mode */
                key = 27;
                last_key = 0;
            } else if (key == 'f') {
                last_key = 'f';
                last_key_time = current_time;
                key = 0;  /* Don't process 'f' in visual mode */
            }
        }
        
        /* Handle mode-specific key bindings */
        if (editor_mode == MODE_NORMAL) {
            /* Normal mode - vim navigation and commands */
            if (key == 'h' || key == -3) {  /* h or Left arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_left();
                }
                pending_delete = 0;  /* Cancel pending operations */
                pending_dt = 0;
            } else if (key == 'j' || key == -2) {  /* j or Down arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_down();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'k' || key == -1) {  /* k or Up arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_up();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'l' || key == -4) {  /* l or Right arrow */
                if (!pending_delete && !pending_dt) {
                    move_cursor_right();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'i') {  /* Enter insert mode */
                set_mode(MODE_INSERT);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'a') {  /* Append - move right then insert */
                move_cursor_right();
                set_mode(MODE_INSERT);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'v') {  /* Enter visual mode */
                Page *page = pages[current_page];
                page->highlight_start = page->cursor_pos;
                page->highlight_end = page->cursor_pos;
                set_mode(MODE_VISUAL);
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'x') {  /* Delete character under cursor */
                delete_char();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'd') {  /* Delete command */
                if (pending_delete) {
                    /* 'dd' - delete line */
                    delete_line();
                    pending_delete = 0;
                } else {
                    /* First 'd' - wait for next key */
                    pending_delete = 1;
                }
            } else if (key == 't' && pending_delete) {
                /* 'dt' - delete till character */
                pending_dt = 1;
                pending_delete = 0;
            } else if (key == '$' && pending_delete) {
                /* 'd$' - delete to end of line */
                delete_to_eol();
                pending_delete = 0;
            } else if (key == '^' && pending_delete) {
                /* 'd^' - delete to beginning of line (first non-whitespace) */
                delete_to_bol();
                pending_delete = 0;
            } else if (pending_dt && key > 0 && key < 127) {
                /* Complete 'dt<char>' command */
                delete_till_char((char)key);
                pending_dt = 0;
            } else if (key == 'w') {  /* Move forward by word */
                if (!pending_delete) {
                    move_word_forward();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'b') {  /* Move backward by word */
                if (!pending_delete) {
                    move_word_backward();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'o') {  /* Insert line below and enter insert mode */
                insert_line_below();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == 'O') {  /* Insert line above and enter insert mode */
                insert_line_above();
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == '$' && !pending_delete) {  /* Move to end of line */
                move_to_end_of_line();
                pending_dt = 0;
            } else if (key == '^') {  /* Move to first non-whitespace character */
                if (!pending_delete) {
                    move_to_first_non_whitespace();
                }
                pending_delete = 0;
                pending_dt = 0;
            } else if (key == -5) {  /* Shift+Left = Previous page */
                prev_page();
            } else if (key == -6) {  /* Shift+Right = Next page */
                next_page();
            }
        } else if (editor_mode == MODE_INSERT) {
            /* Insert mode - regular typing */
            if (key == 27) {  /* ESC - return to normal mode */
                set_mode(MODE_NORMAL);
            } else if (key == -1) {  /* Up arrow */
                move_cursor_up();
            } else if (key == -2) {  /* Down arrow */
                move_cursor_down();
            } else if (key == -3) {  /* Left arrow */
                move_cursor_left();
            } else if (key == -4) {  /* Right arrow */
                move_cursor_right();
            } else if (key == -5) {  /* Shift+Left = Previous page */
                prev_page();
            } else if (key == -6) {  /* Shift+Right = Next page */
                next_page();
            } else if (key == '\b') {  /* Backspace */
                delete_char();
            } else if (key == '\t') {  /* Tab - insert actual tab character */
                insert_char('\t');
            } else if (key > 0) {  /* Regular character */
                insert_char((char)key);
            }
        } else if (editor_mode == MODE_VISUAL) {
            /* Visual mode - selection and movement */
            Page *page = pages[current_page];
            if (key == 27) {  /* ESC - return to normal mode */
                page->highlight_start = 0;
                page->highlight_end = 0;
                set_mode(MODE_NORMAL);
                refresh_screen();
            } else if (key == 'h' || key == -3) {  /* h or Left arrow */
                move_cursor_left();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'j' || key == -2) {  /* j or Down arrow */
                move_cursor_down();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'k' || key == -1) {  /* k or Up arrow */
                move_cursor_up();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            } else if (key == 'l' || key == -4) {  /* l or Right arrow */
                move_cursor_right();
                page->highlight_end = page->cursor_pos;
                refresh_screen();
            }
        }
    }
}
