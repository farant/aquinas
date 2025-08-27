#include "page.h"
#include "memory.h"
#include "serial.h"

/* Page management globals */
Page* pages[MAX_PAGES];
int current_page = 0;
int total_pages = 1;

/* Navigation history for #back functionality */
int page_history[HISTORY_SIZE];
int history_pos = 0;
int history_count = 0;

/* Forward declaration for refresh_screen - will be in display module */
extern void refresh_screen(void);

/* Allocate a new page */
Page* allocate_page(void) {
    Page* page = (Page*)malloc(sizeof(Page));
    if (page == NULL) {
        serial_write_string("ERROR: Failed to allocate page structure\n");
        return NULL;
    }
    
    /* Allocate buffer for the page */
    page->buffer = (char*)calloc(PAGE_SIZE, sizeof(char));
    if (page->buffer == NULL) {
        serial_write_string("ERROR: Failed to allocate page buffer\n");
        /* Note: We can't free the page in bump allocator, but that's ok */
        return NULL;
    }
    
    /* Initialize page fields */
    page->length = 0;
    page->cursor_pos = 0;
    page->highlight_start = 0;
    page->highlight_end = 0;
    page->name[0] = '\0';  /* Empty name initially */
    
    return page;
}

/* Initialize page array */
void init_pages(void) {
    int i;
    
    /* Clear all page pointers */
    for (i = 0; i < MAX_PAGES; i++) {
        pages[i] = NULL;
    }
    
    /* Allocate the first page */
    pages[0] = allocate_page();
    if (pages[0] == NULL) {
        /* Critical error - can't continue without at least one page */
        serial_write_string("FATAL: Could not allocate initial page\n");
        /* In a real OS, we'd panic here */
    }
    
    current_page = 0;
    total_pages = 1;
}

/* Navigate to a specific page with history tracking */
void navigate_to_page(int new_page) {
    if (new_page == current_page) return;  /* Already on this page */
    
    if (new_page < 0) new_page = 0;
    if (new_page >= MAX_PAGES) new_page = MAX_PAGES - 1;
    
    /* Record current page in history before navigating away */
    if (history_count < HISTORY_SIZE) {
        page_history[history_count] = current_page;
        history_count++;
        history_pos = history_count;  /* Reset position to end */
    } else {
        /* History is full, shift everything left and add new entry */
        int i;
        for (i = 0; i < HISTORY_SIZE - 1; i++) {
            page_history[i] = page_history[i + 1];
        }
        page_history[HISTORY_SIZE - 1] = current_page;
        /* history_pos stays at HISTORY_SIZE */
    }
    
    /* Navigate to the new page */
    current_page = new_page;
    
    /* Create page if it doesn't exist yet */
    if (current_page >= total_pages) {
        total_pages = current_page + 1;
        /* Allocate and initialize new page */
        pages[current_page] = allocate_page();
        if (pages[current_page] == NULL) {
            serial_write_string("ERROR: Failed to allocate new page\n");
            /* Go back to previous page */
            if (history_count > 0) {
                current_page = page_history[history_count - 1];
                history_count--;  /* Remove the failed navigation from history */
            }
            return;
        }
    }
    
    refresh_screen();
}

/* Switch to previous page */
void prev_page(void) {
    if (current_page > 0) {
        navigate_to_page(current_page - 1);
    }
}

/* Switch to next page */
void next_page(void) {
    navigate_to_page(current_page + 1);
}