#ifndef LINE_H
#define LINE_H

#define LINE_MAX 256

void line_init(void);
void line_putchar(char c);
int line_readline(char *out, int max);

#endif