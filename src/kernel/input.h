#ifndef INPUT_H
#define INPUT_H

/* Mouse functions */
void init_mouse(void);
void poll_mouse(void);

/* Keyboard functions */
int keyboard_check(void);
int keyboard_get_key_event(unsigned char *scancode, char *ascii);

/* Key state tracking */
extern int shift_pressed;
extern int ctrl_pressed;

#endif /* INPUT_H */