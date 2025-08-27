#include "editor.h"
#include "page.h"
#include "display.h"
#include "modes.h"

/* Insert a character at cursor position */
void insert_char(char c) {
    Page *page = pages[current_page];
    int line_start;
    int indent_count;
    int check_pos;
    int i;
    
    /* Check if page is full.
     * Why PAGE_SIZE - 1: We reserve one byte as a safety margin to prevent
     * buffer overflows during operations like auto-indentation which may
     * insert multiple characters. */
    if (page->length >= PAGE_SIZE - 1) return;
    
    /* If inserting newline, handle auto-indentation */
    if (c == '\n') {
        /* Find the start of the current line */
        line_start = page->cursor_pos;
        while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
            line_start--;
        }
        
        /* Count leading spaces/tabs on current line */
        indent_count = 0;
        check_pos = line_start;
        while (check_pos < page->length && 
               (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t')) {
            indent_count++;
            check_pos++;
        }
        
        /* Make sure we have enough space for newline + indentation */
        if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
        
        /* Shift everything after cursor forward to make room for newline + indentation */
        for (i = page->length + indent_count; i > page->cursor_pos; i--) {
            page->buffer[i] = page->buffer[i - 1 - indent_count];
        }
        
        /* Insert newline */
        page->buffer[page->cursor_pos] = '\n';
        page->cursor_pos++;
        page->length++;
        
        /* Copy indentation from current line */
        for (i = 0; i < indent_count; i++) {
            page->buffer[page->cursor_pos] = page->buffer[line_start + i];
            page->cursor_pos++;
            page->length++;
        }
    } else {
        /* Normal character insertion */
        /* Shift everything after cursor forward */
        for (i = page->length; i > page->cursor_pos; i--) {
            page->buffer[i] = page->buffer[i - 1];
        }
        
        /* Insert the character */
        page->buffer[page->cursor_pos] = c;
        page->cursor_pos++;
        page->length++;
    }
    
    refresh_screen();
}

/* Delete character before cursor (backspace) */
void delete_char(void) {
    Page *page = pages[current_page];
    int i;
    
    if (page->cursor_pos == 0) return;
    
    /* Shift everything after cursor backward */
    for (i = page->cursor_pos - 1; i < page->length - 1; i++) {
        page->buffer[i] = page->buffer[i + 1];
    }
    
    page->cursor_pos--;
    page->length--;
    
    refresh_screen();
}

/* Move cursor left */
void move_cursor_left(void) {
    Page *page = pages[current_page];
    if (page->cursor_pos > 0) {
        page->cursor_pos--;
        refresh_screen();
    }
}

/* Move cursor right */
void move_cursor_right(void) {
    Page *page = pages[current_page];
    if (page->cursor_pos < page->length) {
        page->cursor_pos++;
        refresh_screen();
    }
}

/* Move cursor up one line */
void move_cursor_up(void) {
    Page *page = pages[current_page];
    int line_start;
    int prev_line_start;
    int col;
    int prev_line_length;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* If at first line, can't go up */
    if (line_start == 0) return;
    
    /* Find start of previous line */
    prev_line_start = line_start - 1;
    while (prev_line_start > 0 && page->buffer[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }
    
    /* Calculate position in line */
    col = page->cursor_pos - line_start;
    
    /* Move to same column in previous line */
    prev_line_length = (line_start - 1) - prev_line_start;
    if (col > prev_line_length) {
        page->cursor_pos = line_start - 1; /* End of previous line */
    } else {
        page->cursor_pos = prev_line_start + col;
    }
    
    refresh_screen();
}

/* Move cursor down one line */
void move_cursor_down(void) {
    Page *page = pages[current_page];
    int line_end;
    int line_start;
    int col;
    int next_line_start;
    int next_line_end;
    int next_line_length;
    
    /* Find end of current line */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* If at last line, can't go down */
    if (line_end >= page->length) return;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Calculate position in line */
    col = page->cursor_pos - line_start;
    
    /* Find length of next line */
    next_line_start = line_end + 1;
    next_line_end = next_line_start;
    while (next_line_end < page->length && page->buffer[next_line_end] != '\n') {
        next_line_end++;
    }
    
    /* Move to same column in next line */
    next_line_length = next_line_end - next_line_start;
    if (col > next_line_length) {
        page->cursor_pos = next_line_end;
    } else {
        page->cursor_pos = next_line_start + col;
    }
    
    refresh_screen();
}

/* Delete current line */
void delete_line(void) {
    Page *page = pages[current_page];
    int line_start, line_end;
    int delete_count;
    int i;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Find end of current line (including newline) */
    line_end = line_start;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    if (line_end < page->length && page->buffer[line_end] == '\n') {
        line_end++;  /* Include the newline */
    }
    
    /* Calculate how many characters to delete */
    delete_count = line_end - line_start;
    
    /* Shift remaining text left */
    for (i = line_end; i < page->length; i++) {
        page->buffer[i - delete_count] = page->buffer[i];
    }
    
    /* Update length and cursor */
    page->length -= delete_count;
    page->cursor_pos = line_start;
    
    /* Move to first non-space character of next line if exists */
    while (page->cursor_pos < page->length && 
           (page->buffer[page->cursor_pos] == ' ' || 
            page->buffer[page->cursor_pos] == '\t')) {
        page->cursor_pos++;
    }
    
    refresh_screen();
}

/* Delete to end of line */
void delete_to_eol(void) {
    Page *page = pages[current_page];
    int line_end;
    int delete_count;
    int i;
    
    /* Find end of current line (not including newline) */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* Calculate how many characters to delete */
    delete_count = line_end - page->cursor_pos;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = line_end; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length */
        page->length -= delete_count;
        
        refresh_screen();
    }
}

/* Delete from cursor to beginning of line's first non-whitespace */
void delete_to_bol(void) {
    Page *page = pages[current_page];
    int line_start;
    int first_non_ws;
    int delete_start;
    int delete_count;
    int i;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Find first non-whitespace character position */
    first_non_ws = line_start;
    while (first_non_ws < page->length && 
           first_non_ws < page->cursor_pos &&
           (page->buffer[first_non_ws] == ' ' || 
            page->buffer[first_non_ws] == '\t')) {
        first_non_ws++;
    }
    
    /* Delete from first_non_ws to cursor_pos (not including cursor_pos) */
    delete_start = first_non_ws;
    delete_count = page->cursor_pos - delete_start;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = page->cursor_pos; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length and cursor position */
        page->length -= delete_count;
        page->cursor_pos = delete_start;
        
        refresh_screen();
    }
}

/* Delete till character */
void delete_till_char(char target) {
    Page *page = pages[current_page];
    int end_pos;
    int delete_count;
    int i;
    
    /* Find target character */
    end_pos = page->cursor_pos;
    while (end_pos < page->length && 
           page->buffer[end_pos] != target && 
           page->buffer[end_pos] != '\n') {
        end_pos++;
    }
    
    /* Don't delete if we hit newline or end of buffer instead of target */
    if (end_pos >= page->length || page->buffer[end_pos] != target) {
        return;
    }
    
    /* Calculate how many characters to delete (not including target) */
    delete_count = end_pos - page->cursor_pos;
    
    if (delete_count > 0) {
        /* Shift remaining text left */
        for (i = end_pos; i < page->length; i++) {
            page->buffer[i - delete_count] = page->buffer[i];
        }
        
        /* Update length */
        page->length -= delete_count;
        
        refresh_screen();
    }
}

/* Insert new line below current line */
void insert_line_below(void) {
    Page *page = pages[current_page];
    int line_end;
    int line_start;
    int indent_count;
    int check_pos;
    int i;
    
    /* Find end of current line */
    line_end = page->cursor_pos;
    while (line_end < page->length && page->buffer[line_end] != '\n') {
        line_end++;
    }
    
    /* Find start of current line to get indentation */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Count leading spaces/tabs on current line for auto-indent */
    indent_count = 0;
    check_pos = line_start;
    while (check_pos < page->length && 
           (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t')) {
        indent_count++;
        check_pos++;
    }
    
    /* Check if we have enough space for newline + indentation */
    if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
    
    /* Move cursor to end of current line */
    page->cursor_pos = line_end;
    
    /* Shift everything after line_end forward to make room */
    for (i = page->length + indent_count; i > line_end; i--) {
        page->buffer[i] = page->buffer[i - 1 - indent_count];
    }
    
    /* Insert newline */
    page->buffer[line_end] = '\n';
    page->cursor_pos = line_end + 1;
    page->length++;
    
    /* Copy indentation from current line (preserving tabs/spaces) */
    for (i = 0; i < indent_count; i++) {
        page->buffer[page->cursor_pos] = page->buffer[line_start + i];
        page->cursor_pos++;
        page->length++;
    }
    
    /* Enter insert mode */
    set_mode(MODE_INSERT);
    refresh_screen();
}

/* Insert new line above current line */
void insert_line_above(void) {
    Page *page = pages[current_page];
    int line_start;
    int original_line_start;
    int indent_count;
    int check_pos;
    int i;
    char indent_chars[80];  /* Store indentation characters */
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    original_line_start = line_start;
    
    /* Count and save indentation from current line */
    indent_count = 0;
    check_pos = line_start;
    while (check_pos < page->length && 
           (page->buffer[check_pos] == ' ' || page->buffer[check_pos] == '\t') &&
           indent_count < 80) {
        indent_chars[indent_count] = page->buffer[check_pos];
        indent_count++;
        check_pos++;
    }
    
    /* Check if we have enough space for newline + indentation */
    if (page->length + 1 + indent_count >= PAGE_SIZE - 1) return;
    
    /* Shift everything from line_start forward to make room for newline + indentation */
    for (i = page->length + indent_count; i >= line_start; i--) {
        if (i - 1 - indent_count >= line_start) {
            page->buffer[i] = page->buffer[i - 1 - indent_count];
        }
    }
    
    /* Now insert the indentation and newline */
    if (line_start > 0) {
        /* Not at beginning - insert indentation then newline */
        for (i = 0; i < indent_count; i++) {
            page->buffer[line_start + i] = indent_chars[i];
        }
        /* Add newline after indentation */
        page->buffer[line_start + indent_count] = '\n';
        /* Position cursor at end of indentation on new line */
        page->cursor_pos = line_start + indent_count;
    } else {
        /* At beginning of buffer */
        for (i = 0; i < indent_count; i++) {
            page->buffer[i] = indent_chars[i];
        }
        /* Add newline after indentation */
        page->buffer[indent_count] = '\n';
        /* Position cursor at end of indentation */
        page->cursor_pos = indent_count;
    }
    
    /* Update length */
    page->length += 1 + indent_count;
    
    /* Enter insert mode */
    set_mode(MODE_INSERT);
    refresh_screen();
}

/* Move to end of line */
void move_to_end_of_line(void) {
    Page *page = pages[current_page];
    
    /* Find end of current line */
    while (page->cursor_pos < page->length && 
           page->buffer[page->cursor_pos] != '\n') {
        page->cursor_pos++;
    }
    
    /* If not at end of buffer and not on empty line, move back one 
     * to be on last character rather than newline */
    if (page->cursor_pos > 0 && 
        page->cursor_pos < page->length &&
        page->buffer[page->cursor_pos] == '\n' &&
        (page->cursor_pos == 0 || page->buffer[page->cursor_pos - 1] != '\n')) {
        page->cursor_pos--;
    }
    
    refresh_screen();
}

/* Move to first non-whitespace character of line */
void move_to_first_non_whitespace(void) {
    Page *page = pages[current_page];
    int line_start;
    
    /* Find start of current line */
    line_start = page->cursor_pos;
    while (line_start > 0 && page->buffer[line_start - 1] != '\n') {
        line_start--;
    }
    
    /* Move to start of line first */
    page->cursor_pos = line_start;
    
    /* Skip whitespace to find first non-whitespace character */
    while (page->cursor_pos < page->length && 
           page->buffer[page->cursor_pos] != '\n' &&
           (page->buffer[page->cursor_pos] == ' ' || 
            page->buffer[page->cursor_pos] == '\t')) {
        page->cursor_pos++;
    }
    
    refresh_screen();
}

/* Move forward one word */
void move_word_forward(void) {
    Page *page = pages[current_page];
    int pos = page->cursor_pos;
    
    /* Skip current word (alphanumeric chars) */
    while (pos < page->length && 
           ((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
            (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
            (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos++;
    }
    
    /* Skip whitespace and punctuation to find next word */
    while (pos < page->length && 
           !((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
             (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
             (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos++;
    }
    
    page->cursor_pos = pos;
    refresh_screen();
}

/* Move backward one word */
void move_word_backward(void) {
    Page *page = pages[current_page];
    int pos = page->cursor_pos;
    
    /* Move back one char to get off current position */
    if (pos > 0) pos--;
    
    /* Skip whitespace and punctuation backwards */
    while (pos > 0 && 
           !((page->buffer[pos] >= 'a' && page->buffer[pos] <= 'z') ||
             (page->buffer[pos] >= 'A' && page->buffer[pos] <= 'Z') ||
             (page->buffer[pos] >= '0' && page->buffer[pos] <= '9'))) {
        pos--;
    }
    
    /* Move to beginning of word */
    while (pos > 0 && 
           ((page->buffer[pos-1] >= 'a' && page->buffer[pos-1] <= 'z') ||
            (page->buffer[pos-1] >= 'A' && page->buffer[pos-1] <= 'Z') ||
            (page->buffer[pos-1] >= '0' && page->buffer[pos-1] <= '9'))) {
        pos--;
    }
    
    page->cursor_pos = pos;
    refresh_screen();
}