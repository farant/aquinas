#ifndef INPUT_H
#define INPUT_H

/* Mouse functions */
void init_mouse(void);
void poll_mouse(void);

/* Keyboard functions */
int keyboard_check(void);

/* Key state tracking */
extern int shift_pressed;
extern int ctrl_pressed;

#endif /* INPUT_H */