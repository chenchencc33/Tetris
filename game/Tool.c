#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Tetris.h"

int blocks[22][10];

void emptyBlocks() { memset(blocks, '\0', 240 * sizeof(int)); }

// Flip the target block and return the flipped value.
int flip(int r, int c)
{
    blocks[r][c] = 1 - blocks[r][c];
    return blocks[r][c];
}

// Test a line and return if the line is connected or not.
int testLine(int r)
{
    int c = 0;
    for (c = 0; c < 10; c++)
    {
        if (blocks[r][c] == 0) return 0;
    }
    for (c = r; c > 0; c--) memcpy(blocks[c], blocks[c - 1], sizeof(int) * 10);
    memset(blocks, '\0', sizeof(int) * 10);
    return 1;
}

// Assign the 4 bits to 1 in the shape.
void shapeAssign(int a, int b, int c, int d, int* shape)
{
    shape[a] = 1;
    shape[b] = 1;
    shape[c] = 1;
    shape[d] = 1;
}

// Return the 4*4 matrix of the teris shape.
int* getShape(int type, int rotation)
{
    static int r[16];
    memset(r, '\0', 16 * sizeof(int));

    int merged_type;
    if (type == 0 || type >= 5) merged_type = 10 * type + rotation % 2;
    else if (type == 1) merged_type = 10;
    else merged_type = 10 * type + rotation % 4;

    switch (merged_type)
    {
        // I for type 0
        case 0:
            shapeAssign(0, 1, 2, 3, r);
            break;
        case 1:
            shapeAssign(0, 4, 8, 12, r);
            break;
        // O for type 1
        case 10:
            shapeAssign(0, 1, 4, 5, r);
            break;
        // T for type 2
        case 20:
            shapeAssign(0, 1, 2, 5, r);
            break;
        case 21:
            shapeAssign(1, 4, 5, 9, r);
            break;
        case 22:
            shapeAssign(1, 4, 5, 6, r);
            break;
        case 23:
            shapeAssign(0, 4, 5, 8, r);
            break;
        // L for type 3
        case 30:
            shapeAssign(0, 1, 2, 4, r);
            break;
        case 31:
            shapeAssign(0, 1, 5, 9, r);
            break;
        case 32:
            shapeAssign(2, 4, 5, 6, r);
            break;
        case 33:
            shapeAssign(0, 4, 8, 9, r);
            break;
        // J for type 4
        case 40:
            shapeAssign(0, 1, 2, 6, r);
            break;
        case 41:
            shapeAssign(1, 5, 8, 9, r);
            break;
        case 42:
            shapeAssign(0, 4, 5, 6, r);
            break;
        case 43:
            shapeAssign(0, 1, 4, 8, r);
            break;
        // Z for type 5
        case 50:
            shapeAssign(0, 1, 5, 6, r);
            break;
        case 51:
            shapeAssign(1, 4, 5, 8, r);
            break;
        // N for type 6
        case 60:
            shapeAssign(1, 2, 4, 5, r);
            break;
        case 61:
            shapeAssign(0, 4, 5, 9, r);
            break;
    }
    return r;
}

// Set the score value.
void set_score(int tetris_fd, const int score)
{
    int temp = score > 9999? 9999 : score;
    tetris_arg_t vla;
    vla.p = 0;

    for(int i = 0; i < 4; i++)
    {
        vla.p += ((temp % 10) << (4 * i));
        temp /= 10;
    }
    if (ioctl(tetris_fd, TETRIS_WRITE_SCORE, &vla))
    {
        perror("ioctl(TETRIS_WRITE_SCORE) failed");
        return;
    }
}

// Set the speed of the game.
void set_speed(int tetris_fd, const int speed)
{
    tetris_arg_t vla;
    vla.p = speed;
    if (ioctl(tetris_fd, TETRIS_WRITE_SPEED, &vla))
    {
        perror("ioctl(TETRIS_WRITE_SPEED) failed");
        return;
    }
}

// Set the next block.
int set_next(int tetris_fd, int current)
{
    int type = rand() % 7;
    if (type == current) type = (type + 1 + rand() % 6) % 7;
    tetris_arg_t vla;
    vla.p = type + 1;
    if (ioctl(tetris_fd, TETRIS_WRITE_NEXT, &vla))
    {
        perror("ioctl(TETRIS_WRITE_NEXT) failed");
        return -1;
    }
    return type;
}

// Set the lines to erase.
void set_del_row(int tetris_fd, const int count)
{
    tetris_arg_t vla;
    vla.p = count;
    if (ioctl(tetris_fd, TETRIS_DEL_ROW, &vla))
    {
        perror("ioctl(TETRIS_DEL_ROW) failed");
        return;
    }
}

// Send the information about the new block position.
void set_block(int tetris_fd, const int x, const int y, const int type, const int rotation, const int value)
{
    tetris_arg_t vla;
    vla.p = (x << 11) + (y << 7) + ((type % 7 + 1) << 4) + ((rotation % 4) << 2) + (value << 1);
    if (ioctl(tetris_fd, TETRIS_WRITE_POSITION, &vla))
    {
        perror("ioctl(TETRIS_WRITE_POSITION) failed");
        return;
    }
}

// Pause or end game, 1 for pause, 11 (3) for end + pause.
void set_pause(int tetris_fd, const int paused)
{
    tetris_arg_t vla;
    vla.p = paused;
    if (ioctl(tetris_fd, TETRIS_PAUSE, &vla))
    {
        perror("ioctl(TETRIS_PAUSE) failed");
        return;
    }
}
