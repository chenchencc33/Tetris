#ifndef _TOOL_H
#define _TOOL_H

extern void emptyBlocks(void);
extern void printBlocks(void);
extern int flip(int, int);
extern int testLine(int);
extern int* getShape(int, int);
extern void set_score(int, const int);
extern void set_speed(int, const int);
extern int set_next(int, int);
extern void set_del_row(int, const int);
extern void set_block(int, const int, const int, const int, const int, const int);
extern void set_pause(int, const int);

#endif
