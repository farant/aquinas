#ifndef EDITOR_H
#define EDITOR_H

/* Text editing operations */
void insert_char(char c);
void delete_char(void);

/* Cursor movement */
void move_cursor_left(void);
void move_cursor_right(void);
void move_cursor_up(void);
void move_cursor_down(void);
void move_to_end_of_line(void);
void move_to_first_non_whitespace(void);
void move_word_forward(void);
void move_word_backward(void);

/* Line operations */
void delete_line(void);
void delete_to_eol(void);
void delete_to_bol(void);
void delete_till_char(char target);
void insert_line_below(void);
void insert_line_above(void);

#endif /* EDITOR_H */