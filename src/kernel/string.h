#ifndef STRING_H
#define STRING_H

int strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, int n);

#endif