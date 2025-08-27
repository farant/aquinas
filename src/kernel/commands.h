#ifndef COMMANDS_H
#define COMMANDS_H

#include "page.h"

/* Command and link execution functions */
void execute_command(Page* page, int cmd_start, int cmd_end);
void execute_link(Page* page, int link_start, int link_end);

/* External functions these commands may call */
extern void graphics_demo(void);
extern void test_dispi_driver(void);

#endif /* COMMANDS_H */