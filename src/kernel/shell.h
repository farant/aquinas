#ifndef SHELL_H
#define SHELL_H

void readline(char* buffer, int max_length);
void process_command(char* cmd);
void shell_run(void);

#endif