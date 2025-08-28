#include "input.h"
#include "io.h"
#include "serial.h"
#include "vga.h"
#include "page.h"
#include "display.h"
#include "commands.h"

/* Shift key state - must persist between calls */
int shift_pressed = 0;
int ctrl_pressed = 0;

/* Get keyboard event with both scancode and ASCII */
int keyboard_get_key_event(unsigned char *scancode, char *ascii) {
    unsigned char status;
    unsigned char keycode;
    
    /* Simple scancode to ASCII */
    static const char scancode_map[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    /* Shifted scancode map */
    static const char scancode_map_shift[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    /* Check if keyboard data is available */
    status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) {
        return 0;  /* No keyboard data available */
    }
    
    /* Read the keycode */
    keycode = inb(0x60);
    
    /* Return the scancode */
    if (scancode) {
        *scancode = keycode;
    }
    
    /* Handle key release (high bit set) */
    if (keycode & 0x80) {
        /* Key release */
        unsigned char released_key = keycode & 0x7F;
        
        /* Check for shift release */
        if (released_key == 0x2A || released_key == 0x36) {
            shift_pressed = 0;
        }
        /* Check for control release */
        else if (released_key == 0x1D) {
            ctrl_pressed = 0;
        }
        
        /* Return ASCII as 0 for key release */
        if (ascii) {
            *ascii = 0;
        }
        return -1;  /* Key release event */
    }
    
    /* Check for shift press */
    if (keycode == 0x2A || keycode == 0x36) {
        shift_pressed = 1;
        if (ascii) *ascii = 0;
        return 1;
    }
    
    /* Check for control press */
    if (keycode == 0x1D) {
        ctrl_pressed = 1;
        if (ascii) *ascii = 0;
        return 1;
    }
    
    /* Convert to ASCII if it's a printable key */
    if (ascii) {
        if (keycode < 128) {
            if (shift_pressed) {
                *ascii = scancode_map_shift[keycode];
            } else {
                *ascii = scancode_map[keycode];
            }
        } else {
            *ascii = 0;
        }
    }
    
    return 1;  /* Key press event */
}

/* External functions from graphics mode */
extern int graphics_mode_active;
extern void handle_graphics_mouse_raw(signed char dx, signed char dy);
extern void handle_graphics_mouse_move(int x, int y);

/* Initialize serial mouse */
void init_mouse(void) {
    init_serial_port();
    
    /* Send 'M' to identify as Microsoft mouse */
    /* Some mice auto-detect, others need this */
    while (inb(COM1_LSR) & 0x01) {
        inb(COM1_DATA);  /* Clear any pending data */
    }
    
    mouse_visible = 1;
}

/* Poll for serial mouse data (non-blocking)
 *
 * MICROSOFT SERIAL MOUSE PROTOCOL
 * --------------------------------
 * 
 * Serial mice communicate using 3-byte packets at 1200 baud, 7N1.
 * The protocol was designed to be self-synchronizing - you can start
 * reading at any point and quickly identify packet boundaries.
 * 
 * Packet format:
 *   Byte 0: 01LR YYyy XXxx
 *           - Bit 6 is always 1 (identifies first byte)
 *           - Bit 5 (L) = left button state
 *           - Bit 4 (R) = right button state  
 *           - Bits 3-2 (YY) = Y movement high bits
 *           - Bits 1-0 (XX) = X movement high bits
 * 
 *   Byte 1: 00XX XXXX
 *           - Bit 6 is always 0 (not a first byte)
 *           - Bits 5-0 = X movement low 6 bits
 * 
 *   Byte 2: 00YY YYYY
 *           - Bit 6 is always 0 (not a first byte)
 *           - Bits 5-0 = Y movement low 6 bits
 * 
 * Movement values are 8-bit signed integers (-128 to +127).
 * Positive X = right, Positive Y = down (in mouse coordinates).
 * We invert Y since our screen coordinates have Y=0 at top.
 * 
 * The accumulator approach below smooths out small movements and
 * provides sub-pixel precision for better cursor control.
 */
void poll_mouse(void) {
    static unsigned char mouse_bytes[3];
    static int byte_num = 0;
    static int accumulated_dx = 0;  /* Accumulate fractional X movements */
    static int accumulated_dy = 0;  /* Accumulate fractional Y movements */
    static int prev_left_button = 0;  /* Track previous button state */
    unsigned char data;
    int packets_processed = 0;
    int left_button, right_button;
    int dx, dy;
    int old_x, old_y;
    int click_x, click_y;
    int nav_text_len, nav_start;
    
    /* Read all available bytes from serial port.
     * Why limit to 10 packets: We process at most 10 packets per poll to
     * prevent the mouse from monopolizing CPU time. Since we poll frequently,
     * this doesn't cause noticeable lag but ensures keyboard remains responsive. */
    while ((inb(COM1_LSR) & 0x01) && packets_processed < 10) {
        data = inb(COM1_DATA);
        
        /* Microsoft protocol: First byte has bit 6 set, bit 7 clear.
         * Why: This bit pattern allows re-synchronization if we start
         * reading in the middle of a packet stream. We can always find
         * the start of the next packet by looking for this signature. */
        if ((data & 0xC0) == 0x40) {
            /* This looks like a first byte - start new packet */
            byte_num = 0;
            mouse_bytes[0] = data;
            byte_num = 1;
        } else if (byte_num > 0 && (data & 0x40) == 0) {
            /* This looks like a continuation byte (bit 6 clear) */
            mouse_bytes[byte_num++] = data;
        } else {
            /* Invalid byte - reset and wait for valid first byte */
            byte_num = 0;
            continue;
        }
        
        /* Process complete packet */
        if (byte_num == 3) {
            byte_num = 0;  /* Reset for next packet */
            packets_processed++;
            
            /* Parse Microsoft mouse packet */
            /* Byte 0: 01LR YYyy XXxx (L=left button, R=right button) */
            /* Byte 1: 00XX XXXX (X movement, 6 bits) */
            /* Byte 2: 00YY YYYY (Y movement, 6 bits) */
            
            /* Extract button states */
            left_button = (mouse_bytes[0] >> 5) & 1;
            right_button = (mouse_bytes[0] >> 4) & 1;
            
            /* Extract and combine movement values from split fields */
            dx = (mouse_bytes[1] & 0x3F) | ((mouse_bytes[0] & 0x03) << 6);
            dy = (mouse_bytes[2] & 0x3F) | ((mouse_bytes[0] & 0x0C) << 4);
            
            /* Sign extend from 8-bit to full int */
            if (dx & 0x80) dx = dx - 256;
            if (dy & 0x80) dy = dy - 256;
        
        /* Store old position to check if mouse moved */
        old_x = mouse_x;
        old_y = mouse_y;
        
        /* Check for left click BEFORE updating position */
        /* This uses the position where the button was pressed, not where mouse moved to */
        if (left_button && !prev_left_button) {
            /* Skip all click handling if in graphics mode */
            if (graphics_mode_active) {
                /* Just update button state and continue */
                prev_left_button = left_button;
                continue;
            }
            
            /* Capture click at current position before any movement */
            click_x = mouse_x;
            click_y = mouse_y;
            
            /* Check if click is on navigation bar */
            if (click_y == 0) {
                /* Calculate where the buttons are - fixed positions now */
                nav_text_len = 27;  /* Fixed: 6 spaces/[prev] + space + "Page xx of yy [next]" */
                nav_start = (VGA_WIDTH - nav_text_len) / 2;
                
                /* Check for [prev] button click (only if visible) */
                if (current_page > 0 && click_x >= nav_start && click_x < nav_start + 6) {
                    prev_page();
                }
                /* Check for [next] button click - always at same position */
                else if (click_x >= nav_start + nav_text_len - 6 && click_x < nav_start + nav_text_len) {
                    next_page();
                }
            }
            /* Normal text click handling */
            else {
                /* Get current page */
                Page *page = pages[current_page];
                
                int click_y = mouse_y - 1;
                if (click_y >= 0 && click_y < VGA_HEIGHT - 1) {
                    int click_x = mouse_x;
                    
                    /* Calculate buffer position from screen coordinates */
                    /* Simplified approach: directly calculate position */
                    int buf_pos = 0;
                    int line = 0;
                    int col = 0;
                    
                    /* Walk through the buffer character by character, tracking our
                     * screen position until we reach the clicked coordinates */
                    for (buf_pos = 0; buf_pos < page->length; buf_pos++) {
                        /* Check if we reached the clicked position */
                        if (line == click_y && col == click_x) {
                            break;
                        }
                        
                        /* Handle newlines */
                        if (page->buffer[buf_pos] == '\n') {
                            /* If we're on the target line but past the click column, we clicked past line end */
                            if (line == click_y) {
                                break;
                            }
                            line++;
                            col = 0;
                        } else if (page->buffer[buf_pos] == '\t') {
                            /* Tabs take up 2 visual spaces */
                            col += 2;
                            /* Handle line wrap */
                            if (col >= VGA_WIDTH) {
                                line++;
                                col = 0;
                            }
                        } else {
                            /* Normal character - move to next column */
                            col++;
                            /* Handle line wrap */
                            if (col >= VGA_WIDTH) {
                                line++;
                                col = 0;
                            }
                        }
                        
                        /* Stop if we've gone past the target line */
                        if (line > click_y) {
                            break;
                        }
                    }
                    
                    /* Check if click is within text */
                    if (buf_pos >= 0 && buf_pos < page->length) {
                        /* Check if clicked on a word or whitespace */
                        char clicked_char = page->buffer[buf_pos];
                        if (clicked_char == ' ' || clicked_char == '\n' || clicked_char == '\t') {
                            /* Clear any existing highlight */
                            page->highlight_start = 0;
                            page->highlight_end = 0;
                        } else {
                            /* Find and highlight the complete word */
                            page->highlight_start = buf_pos;
                            page->highlight_end = buf_pos + 1;  /* Start with at least one char */
                            
                            /* Find start of word */
                            while (page->highlight_start > 0 && 
                                   page->buffer[page->highlight_start - 1] != ' ' &&
                                   page->buffer[page->highlight_start - 1] != '\n' &&
                                   page->buffer[page->highlight_start - 1] != '\t') {
                                page->highlight_start--;
                            }
                            
                            /* Find end of word */
                            while (page->highlight_end < page->length &&
                                   page->buffer[page->highlight_end] != ' ' &&
                                   page->buffer[page->highlight_end] != '\n' &&
                                   page->buffer[page->highlight_end] != '\t') {
                                page->highlight_end++;
                            }
                            
                            /* Check if this is a command (starts with $) */
                            if (page->buffer[page->highlight_start] == '$') {
                                /* Execute the command */
                                execute_command(page, page->highlight_start, page->highlight_end);
                            }
                            /* Check if this is a link (starts with #) */
                            else if (page->buffer[page->highlight_start] == '#') {
                                /* Execute the link */
                                execute_link(page, page->highlight_start, page->highlight_end);
                            }
                            
                            /* Debug: Log the highlighted word (via COM2) */
                            serial_write_string("Highlighted word at position ");
                            serial_write_hex(buf_pos);
                            serial_write_string(" (");
                            serial_write_hex(page->highlight_start);
                            serial_write_string(" to ");
                            serial_write_hex(page->highlight_end);
                            serial_write_string(")\n");
                        }
                    } else {
                        /* Clicked beyond text - clear highlight */
                        page->highlight_start = 0;
                        page->highlight_end = 0;
                    }
                    
                    refresh_screen();
                }
            }  /* End of else (normal text click) */
        }
        
        /* Only update mouse position if button is NOT held down */
        /* This prevents cursor movement during clicks/drags */
        if (!left_button) {
            /* In graphics mode, send raw deltas directly */
            if (graphics_mode_active) {
                handle_graphics_mouse_raw((signed char)dx, (signed char)dy);
                /* Skip text mode processing */
                continue;
            }
            
            /* Accumulate mouse movements for smooth, responsive control */
            /* This ensures even tiny movements eventually register */
            accumulated_dx += dx;
            accumulated_dy += dy;
            
            /* Apply accumulated movement with scaling */
            /* Divide by 10 for slower, more controlled movement */
            if (accumulated_dx != 0) {
                /* Add threshold for horizontal movement to prevent column jitter */
                /* Less strict than vertical to keep horizontal movement fluid */
                int move_x = accumulated_dx / 12;  /* Slightly higher divisor for X axis */
                if (move_x == 0 && (accumulated_dx > 10 || accumulated_dx < -10)) {
                    /* Only move if accumulated enough (more than 10 units) */
                    /* This prevents accidental column changes */
                    move_x = (accumulated_dx > 0) ? 1 : -1;
                    accumulated_dx -= move_x * 12;  /* Consume the movement */
                } else if (move_x != 0) {
                    accumulated_dx -= move_x * 12;  /* Keep remainder for next time */
                }
                /* If not enough accumulation, keep building up */
                mouse_x += move_x;
            }
            
            if (accumulated_dy != 0) {
                /* Add extra threshold for vertical movement to prevent line jitter */
                /* Require more accumulation before moving vertically */
                int move_y = accumulated_dy / 15;  /* Higher divisor for Y axis */
                if (move_y == 0 && (accumulated_dy > 12 || accumulated_dy < -12)) {
                    /* Only move if accumulated enough (more than 12 units) */
                    /* This prevents accidental line changes */
                    move_y = (accumulated_dy > 0) ? 1 : -1;
                    accumulated_dy -= move_y * 15;  /* Consume the movement */
                } else if (move_y != 0) {
                    accumulated_dy -= move_y * 15;  /* Keep remainder for next time */
                }
                /* If not enough accumulation, keep building up */
                mouse_y += move_y;
            }
            
            /* Clamp to screen bounds and clear accumulated values at edges */
            if (mouse_x < 0) {
                mouse_x = 0;
                accumulated_dx = 0;  /* Clear accumulator at edge */
            }
            if (mouse_x >= VGA_WIDTH) {
                mouse_x = VGA_WIDTH - 1;
                accumulated_dx = 0;  /* Clear accumulator at edge */
            }
            if (mouse_y < 0) {
                mouse_y = 0;
                accumulated_dy = 0;  /* Clear accumulator at edge */
            }
            if (mouse_y >= VGA_HEIGHT) {
                mouse_y = VGA_HEIGHT - 1;
                accumulated_dy = 0;  /* Clear accumulator at edge */
            }
        } else {
            /* While button is held, discard movement data */
            /* This prevents accumulation during drag */
            accumulated_dx = 0;
            accumulated_dy = 0;
        }
        
        /* Refresh if mouse moved */
        if (mouse_x != old_x || mouse_y != old_y) {
            /* If in graphics mode, update graphics cursor instead */
            if (graphics_mode_active) {
                handle_graphics_mouse_move(mouse_x, mouse_y);
            } else {
                refresh_screen();
            }
        }
        
        /* Update previous button state for next frame */
        prev_left_button = left_button;
        
        }  /* End of if (byte_num == 3) */
    }  /* End of while loop reading serial data */
}

/* Non-blocking keyboard check */
int keyboard_check(void) {
    /* Simple scancode to ASCII - extended to handle all keys properly */
    static const char scancode_map[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',  /* 0-14 */
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',    /* 15-28 */
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',            /* 29-41 */
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,              /* 42-54 */
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       /* 55-70 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 71-86 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 87-102 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 103-118 */
        0, 0, 0, 0, 0, 0, 0, 0, 0                                                  /* 119-127 */
    };
    
    /* Shifted scancode map */
    static const char scancode_map_shift[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',  /* 0-14 */
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',    /* 15-28 */
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',            /* 29-41 */
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,              /* 42-54 */
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                       /* 55-70 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 71-86 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 87-102 */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                           /* 103-118 */
        0, 0, 0, 0, 0, 0, 0, 0, 0                                                  /* 119-127 */
    };
    unsigned char status;
    unsigned char keycode;
    
    /* Check if keyboard data available (not mouse data) */
    status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) {
        return 0;  /* No keyboard data available */
    }
    
    /* Read the keycode directly without blocking */
    keycode = inb(0x60);
    
    /* Check for shift press/release (left shift = 0x2A, right shift = 0x36) */
    if (keycode == 0x2A || keycode == 0x36) {
        shift_pressed = 1;
        return 0;  /* Don't return anything for shift press */
    } else if (keycode == 0xAA || keycode == 0xB6) {  /* Shift release */
        shift_pressed = 0;
        return 0;
    }
    
    /* Check for Ctrl press/release (left ctrl = 0x1D) */
    if (keycode == 0x1D) {
        ctrl_pressed = 1;
        return 0;
    } else if (keycode == 0x9D) {  /* Ctrl release */
        ctrl_pressed = 0;
        return 0;
    }
    
    /* Skip key release events (high bit set) */
    if (keycode & 0x80) {
        return 0;
    }
    
    /* Special keys - return negative values */
    /* Arrow keys: Up=0x48, Left=0x4B, Right=0x4D, Down=0x50 */
    if (keycode == 0x48) return -1;  /* Up arrow */
    if (keycode == 0x50) return -2;  /* Down arrow */
    if (keycode == 0x4B) {
        if (shift_pressed) return -5;  /* Shift+Left = Previous page */
        return -3;  /* Left arrow */
    }
    if (keycode == 0x4D) {
        if (shift_pressed) return -6;  /* Shift+Right = Next page */
        return -4;  /* Right arrow */
    }
    
    /* Process regular key press */
    if (keycode < 128) {  /* Make sure we're within array bounds */
        char c;
        
        if (shift_pressed) {
            c = scancode_map_shift[(int)keycode];
        } else {
            c = scancode_map[(int)keycode];
        }
        
        /* Handle Ctrl combinations */
        if (ctrl_pressed && c == '[') {
            /* Ctrl-[ should be treated as ESC */
            return 27;
        }
        
        /* Return character (positive value) */
        if (c != 0) {
            return (int)c;
        }
    }
    
    return 0;
}