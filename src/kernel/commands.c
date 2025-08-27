#include "commands.h"
#include "page.h"
#include "display.h"
#include "rtc.h"
#include "serial.h"
#include "vga.h"
#include "graphics.h"
#include "dispi_demo.h"
#include "layout_demo.h"
#include "ui_demo.h"

/* Execute a command that starts with $ */
void execute_command(Page* page, int cmd_start, int cmd_end) {
    char cmd_name[32];
    int cmd_len;
    int i;
    int insert_pos;
    char output[64];
    int output_len;
    rtc_time_t now;
    int space_after;
    int visual_space_count;
    int scan_pos;
    int col;
    
    /* Extract command name */
    cmd_len = cmd_end - cmd_start;
    if (cmd_len >= 32) cmd_len = 31;
    
    for (i = 0; i < cmd_len; i++) {
        cmd_name[i] = page->buffer[cmd_start + i];
    }
    cmd_name[cmd_len] = '\0';
    
    /* Debug output */
    serial_write_string("Executing command: ");
    for (i = 0; i < cmd_len; i++) {
        serial_write_char(cmd_name[i]);
    }
    serial_write_char('\n');
    
    /* Process commands */
    if (cmd_len == 5 && cmd_name[1] == 'd' && cmd_name[2] == 'a' && 
        cmd_name[3] == 't' && cmd_name[4] == 'e') {
        /* $date command - insert current date/time */
        get_current_time(&now);
        
        /* Format date as MM/DD/YYYY HH:MM */
        output_len = 0;
        
        /* Month */
        if (now.month >= 10) {
            output[output_len++] = '0' + (now.month / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.month % 10);
        output[output_len++] = '/';
        
        /* Day */
        if (now.day >= 10) {
            output[output_len++] = '0' + (now.day / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.day % 10);
        output[output_len++] = '/';
        
        /* Year */
        output[output_len++] = '0' + ((now.year / 1000) % 10);
        output[output_len++] = '0' + ((now.year / 100) % 10);
        output[output_len++] = '0' + ((now.year / 10) % 10);
        output[output_len++] = '0' + (now.year % 10);
        output[output_len++] = ' ';
        
        /* Hour */
        if (now.hour >= 10) {
            output[output_len++] = '0' + (now.hour / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.hour % 10);
        output[output_len++] = ':';
        
        /* Minute */
        if (now.minute >= 10) {
            output[output_len++] = '0' + (now.minute / 10);
        } else {
            output[output_len++] = '0';
        }
        output[output_len++] = '0' + (now.minute % 10);
        
        /* Determine insertion position */
        insert_pos = cmd_end;
        
        /* Check if there's already a space after the command */
        space_after = 0;
        if (insert_pos < page->length && page->buffer[insert_pos] == ' ') {
            space_after = 1;
            insert_pos++;  /* Skip the existing space */
        }
        
        /* Count visual whitespace after command position */
        visual_space_count = 0;
        scan_pos = insert_pos;
        col = 0;
        
        /* Calculate column position of insert_pos */
        for (i = 0; i < insert_pos && i < page->length; i++) {
            if (page->buffer[i] == '\n') {
                col = 0;
            } else if (page->buffer[i] == '\t') {
                col += 2;
            } else {
                col++;
            }
        }
        
        /* Count spaces until we hit non-whitespace or newline */
        while (scan_pos < page->length && visual_space_count < output_len) {
            if (page->buffer[scan_pos] == ' ') {
                visual_space_count++;
                scan_pos++;
                col++;
            } else if (page->buffer[scan_pos] == '\n') {
                /* Count remaining columns in the line as available space */
                while (col < VGA_WIDTH && visual_space_count < output_len) {
                    visual_space_count++;
                    col++;
                }
                break;
            } else {
                /* Hit non-whitespace character */
                break;
            }
        }
        
        /* Check if we have enough room */
        if (page->length + output_len + 1 - visual_space_count >= PAGE_SIZE) {
            serial_write_string("Not enough space for command output\n");
            return;
        }
        
        /* Add space to output to separate from following text */
        output[output_len++] = ' ';
        
        /* If we have enough visual space, overwrite it */
        if (visual_space_count >= output_len) {
            /* Just overwrite the spaces */
            for (i = 0; i < output_len; i++) {
                page->buffer[insert_pos + i] = output[i];
            }
        } else {
            /* Need to make room - shift text right */
            int shift_amount = output_len - visual_space_count;
            
            /* Add space before output if not already there */
            if (!space_after) {
                shift_amount++;  /* Need one more byte for the space */
            }
            
            /* Shift existing text to make room */
            for (i = page->length - 1; i >= insert_pos + visual_space_count; i--) {
                page->buffer[i + shift_amount] = page->buffer[i];
            }
            
            /* Insert space if needed */
            if (!space_after) {
                page->buffer[cmd_end] = ' ';
                insert_pos = cmd_end + 1;
            }
            
            /* Insert the output */
            for (i = 0; i < output_len; i++) {
                page->buffer[insert_pos + i] = output[i];
            }
            
            /* Update page length */
            page->length += shift_amount;
        }
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
        
        /* Refresh display */
        refresh_screen();
    } else if (cmd_len == 7 && cmd_name[1] == 'r' && cmd_name[2] == 'e' && 
               cmd_name[3] == 'n' && cmd_name[4] == 'a' && cmd_name[5] == 'm' && 
               cmd_name[6] == 'e') {
        /* $rename command - look for name after the command */
        int name_start = cmd_end;
        int name_end = cmd_end;
        int name_len = 0;
        int j;
        
        /* Skip any spaces after $rename */
        while (name_start < page->length && page->buffer[name_start] == ' ') {
            name_start++;
        }
        
        /* Find the end of the name (next space or newline) */
        name_end = name_start;
        while (name_end < page->length && 
               page->buffer[name_end] != ' ' && 
               page->buffer[name_end] != '\n' &&
               page->buffer[name_end] != '\t') {
            name_end++;
        }
        
        /* Extract the new name */
        if (name_start < name_end) {
            /* We have a name argument */
            name_len = name_end - name_start;
            if (name_len > 63) name_len = 63;  /* Limit to 63 chars */
            
            /* Copy the name */
            for (j = 0; j < name_len; j++) {
                page->name[j] = page->buffer[name_start + j];
            }
            page->name[name_len] = '\0';
            
            serial_write_string("Page renamed to: ");
            for (j = 0; j < name_len; j++) {
                serial_write_char(page->name[j]);
            }
            serial_write_char('\n');
        } else {
            /* No name provided - clear the name */
            page->name[0] = '\0';
            serial_write_string("Page name cleared\n");
        }
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
        
        /* Refresh display to show new name in nav bar */
        refresh_screen();
    }
    else if (cmd_len == 9 && cmd_name[1] == 'g' && cmd_name[2] == 'r' &&
             cmd_name[3] == 'a' && cmd_name[4] == 'p' && cmd_name[5] == 'h' &&
             cmd_name[6] == 'i' && cmd_name[7] == 'c' && cmd_name[8] == 's') {
        /* $graphics command - switch to graphics mode for demo */
        serial_write_string("Entering graphics mode demo\n");
        
        /* Run the graphics demo (will return when ESC is pressed) */
        graphics_demo();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
    }
    else if (cmd_len == 6 && cmd_name[1] == 'd' && cmd_name[2] == 'i' &&
             cmd_name[3] == 's' && cmd_name[4] == 'p' && cmd_name[5] == 'i') {
        /* $dispi command - test DISPI driver */
        serial_write_string("Testing DISPI driver\n");
        
        /* Test the DISPI driver */
        test_dispi_driver();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
    } else if (cmd_len == 7 && cmd_name[1] == 'l' && cmd_name[2] == 'a' && 
               cmd_name[3] == 'y' && cmd_name[4] == 'o' && cmd_name[5] == 'u' && 
               cmd_name[6] == 't') {
        /* $layout command - test layout and view system */
        serial_write_string("Testing layout and view system\n");
        
        /* Test the layout demo */
        test_layout_demo();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
    } else if (cmd_len == 3 && cmd_name[1] == 'u' && cmd_name[2] == 'i') {
        /* $ui command - test UI component library */
        serial_write_string("Testing UI component library\n");
        
        /* Test the UI demo */
        test_ui_demo();
        
        /* Screen needs to be redrawn after returning from graphics mode */
        refresh_screen();
        
        /* Clear highlight after command execution */
        page->highlight_start = 0;
        page->highlight_end = 0;
    }
}

/* Execute a link that starts with # */
void execute_link(Page* page, int link_start, int link_end) {
    char link_text[64];
    int link_len;
    int i;
    int target_page = -1;
    
    /* Extract link text (skip the #) */
    link_len = link_end - link_start - 1;  /* -1 to skip # */
    if (link_len >= 64) link_len = 63;
    
    for (i = 0; i < link_len; i++) {
        link_text[i] = page->buffer[link_start + 1 + i];  /* +1 to skip # */
    }
    link_text[link_len] = '\0';
    
    /* Debug output */
    serial_write_string("Clicking link: #");
    for (i = 0; i < link_len; i++) {
        serial_write_char(link_text[i]);
    }
    serial_write_char('\n');
    
    /* Check for #back */
    if (link_len == 4 && link_text[0] == 'b' && link_text[1] == 'a' && 
        link_text[2] == 'c' && link_text[3] == 'k') {
        /* Go back in history */
        if (history_count > 0) {
            int prev_page = page_history[history_count - 1];
            history_count--;  /* Remove from history */
            current_page = prev_page;
            refresh_screen();
        }
        /* Clear highlight */
        page->highlight_start = 0;
        page->highlight_end = 0;
        return;
    }
    
    /* Check for #last-page */
    if (link_len == 9 && link_text[0] == 'l' && link_text[1] == 'a' && 
        link_text[2] == 's' && link_text[3] == 't' && link_text[4] == '-' &&
        link_text[5] == 'p' && link_text[6] == 'a' && link_text[7] == 'g' && 
        link_text[8] == 'e') {
        target_page = total_pages - 1;
    }
    /* Check if it's a page number */
    else if (link_len > 0 && link_text[0] >= '0' && link_text[0] <= '9') {
        /* Parse page number */
        target_page = 0;
        for (i = 0; i < link_len; i++) {
            if (link_text[i] >= '0' && link_text[i] <= '9') {
                target_page = target_page * 10 + (link_text[i] - '0');
            } else {
                /* Invalid number */
                target_page = -1;
                break;
            }
        }
        if (target_page > 0) {
            target_page--;  /* Convert to 0-based index */
        }
    }
    /* Otherwise, it's a page name - search for it */
    else if (link_len > 0) {
        int p;
        for (p = 0; p < total_pages; p++) {
            if (pages[p] && pages[p]->name[0] != '\0') {
                /* Compare page name with link text */
                int matches = 1;
                for (i = 0; i < link_len; i++) {
                    if (pages[p]->name[i] != link_text[i]) {
                        matches = 0;
                        break;
                    }
                }
                /* Check that name ends here too */
                if (matches && pages[p]->name[link_len] == '\0') {
                    target_page = p;
                    break;
                }
            }
        }
    }
    
    /* Navigate if we found a valid target */
    if (target_page >= 0) {
        navigate_to_page(target_page);
    } else {
        serial_write_string("Link target not found\n");
    }
    
    /* Clear highlight */
    page->highlight_start = 0;
    page->highlight_end = 0;
}
