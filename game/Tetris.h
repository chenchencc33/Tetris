#ifndef _TETRIS_H
#define _TETRIS_H

#include <linux/ioctl.h>

typedef struct
{
	unsigned short p;
} tetris_arg_t;

typedef struct
{
	int x;
	int y;
	int type;
	int rotation;
} tetris_block;

#define TETRIS_MAGIC 'q'

/* ioctls and their arguments */
#define TETRIS_WRITE_POSITION _IOW(TETRIS_MAGIC, 1, tetris_arg_t *)
#define TETRIS_DEL_ROW  _IOW(TETRIS_MAGIC, 2, tetris_arg_t *)
#define TETRIS_WRITE_SCORE  _IOW(TETRIS_MAGIC, 3, tetris_arg_t *)
#define TETRIS_WRITE_NEXT  _IOW(TETRIS_MAGIC, 4, tetris_arg_t *)
#define TETRIS_WRITE_SPEED  _IOW(TETRIS_MAGIC, 5, tetris_arg_t *)
#define TETRIS_RESET  _IOW(TETRIS_MAGIC, 6, tetris_arg_t *)
#define TETRIS_PAUSE  _IOW(TETRIS_MAGIC, 7, tetris_arg_t *)
#endif
