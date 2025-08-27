#ifndef PAGE_H
#define PAGE_H

#include "vga.h"

/* Page size is one screen minus the navigation bar */
#define PAGE_SIZE ((VGA_HEIGHT - 1) * VGA_WIDTH)
#define MAX_PAGES 100

/* Page structure - each page has its own buffer and cursor */
typedef struct {
    char* buffer;           /* Dynamically allocated buffer */
    int length;             /* Current length of text in this page */
    int cursor_pos;         /* Cursor position in this page */
    int highlight_start;    /* Start of highlighted text in this page */
    int highlight_end;      /* End of highlighted text in this page */
    char name[64];          /* Optional page name (empty string if unnamed) */
} Page;

/* Navigation history for #back functionality */
#define HISTORY_SIZE 32

/* Global page state */
extern Page* pages[MAX_PAGES];
extern int current_page;
extern int total_pages;
extern int page_history[HISTORY_SIZE];
extern int history_pos;
extern int history_count;

/* Page management functions */
Page* allocate_page(void);
void init_pages(void);
void navigate_to_page(int new_page);
void prev_page(void);
void next_page(void);

#endif /* PAGE_H */