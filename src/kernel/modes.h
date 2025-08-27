#ifndef MODES_H
#define MODES_H

/* Editor modes */
typedef enum {
    MODE_NORMAL,    /* Normal/command mode (vim-like) */
    MODE_INSERT,    /* Insert mode for typing */
    MODE_VISUAL     /* Visual mode for selection */
} EditorMode;

/* 'fd' escape sequence timeout in milliseconds */
#define FD_ESCAPE_TIMEOUT_MS 300

/* Global mode state */
extern EditorMode editor_mode;

/* Mode management functions */
void set_mode(EditorMode mode);
const char* get_mode_string(void);

#endif /* MODES_H */