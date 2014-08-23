

#ifndef __VT100_H__
#define __VT100_H__

#define ESC "\x1B"
#define VT100_RESET ESC "c"
#define VT100_HOME ESC "[H"
#define VT100_CLEAR_SCREEN VT100_HOME ESC "J"
#define VT100_CLR ESC "[2J"

int vt100_init(FILE *out_stream);

void vt100_reset(void);

void vt100_clear(void);

void vt100_home(void);

void vt100_pos(int row, int col);

int vt100_printf(int row, int col, char *fmt, ...);

void vt100_flush(void);

#endif
