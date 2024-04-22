#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Tetris.h"
#include "Tool.h"

// The block of the game.
int score = 0;
int downCount = 0;
int speed = 0;
int tetris_fd;
int pauseFlag = 1;
int ignoreNext = 0;
int down_key = 0;
int gameEnd = 1;

// A correct assign should return either 0 or 4.
int assign(int x, int y, int type, int rotation)
{
    int* shape = getShape(type, rotation);
    int sum = 0, i;
    for (i = 0; i < 16; i++)
    {
        // Flip the items in the shape.
        if (shape[i] == 1)
        {
            int dx = x + i % 4;
            int dy = y + i / 4;
            if (dx < 10 && dx >= 0 && dy < 22 && dy >= 0) sum += flip(dy, dx);
        }
    }
    return sum;
}

// Rotate the current shape by 90 degree clockwise.
void rotate(tetris_block *input)
{
    assign(input->x, input->y, input->type, input->rotation);
    if (assign(input->x, input->y, input->type, input->rotation + 1) != 4)
    {
        assign(input->x, input->y, input->type, input->rotation);
        assign(input->x, input->y, input->type, input->rotation + 1);
        return;
    }

    // Successfully rotate.
    set_block(tetris_fd, input->y, input->x, input->type, input->rotation, 0);
    set_block(tetris_fd, input->y, input->x, input->type, input->rotation + 1, 1);
    input->rotation = (input->rotation + 1) % 4;
}

// Return -1 if unable to create.
int testAndCreate(int type, tetris_block *input)
{
    int line_num = 0;

    // Test for any existing lines to erase.
    int count = 0;
    for (count = 0; count < 22; count++)
    {
        if (testLine(count) == 1)
        {
            if (ignoreNext == 0)
            {
                usleep(100000);
                ignoreNext = 1;
            }
            line_num += 1;
            set_del_row(tetris_fd, count);
        }
    }

    // Create a new shape.
    input->y = 0;
    input->rotation = 0;
    input->type = type;
    input->x = type == 0? 3 : 4;
    if (assign(input->x, 0, type, 0) != 4)
    {
        assign(input->x, 0, type, 0);
        return -1;
    }
    else set_block(tetris_fd, 0, input->x, type, 0, 1);

    // Set the score.
    score += line_num * (speed + line_num);
    set_score(tetris_fd, score);

    return line_num;
}

// Reset the parameters.
int reset(tetris_block *input)
{
    srand(time(NULL));
    emptyBlocks();
    tetris_arg_t vla;
    vla.p = 0;
    if (ioctl(tetris_fd, TETRIS_RESET, &vla))
    {
        perror("ioctl(TETRIS_RESET) failed");
        return -1;
    }

    downCount = 0;
    speed = 0;
    score = 0;
    down_key = 0;
    pauseFlag = 0;
    gameEnd = 0;
    testAndCreate(rand() % 7, input);
    
    set_pause(tetris_fd, 0);
    return set_next(tetris_fd, input->type);
}

// xy = 0 for x, xy = 1 for y.
int move(int d, int xy, tetris_block *input)
{
    int new_x = input->x + d * (1 - xy);
    int new_y = input->y + d * xy;
    if (assign(input->x, input->y, input->type, input->rotation) != 0)
    {
        assign(input->x, input->y, input->type, input->rotation);
        return -1;
    }
    else if (assign(new_x, new_y, input->type, input->rotation) != 4)
    {
        assign(input->x, input->y, input->type, input->rotation);
        assign(new_x, new_y, input->type, input->rotation);
        return 1;
    }

    set_block(tetris_fd, input->y, input->x, input->type, input->rotation, 0);
    set_block(tetris_fd, new_y, new_x, input->type, input->rotation, 1);
    input->x = new_x;
    input->y = new_y;

    return 0;
}

// Joystick control.
int main(int argc, char *argv[])
{
    char input_dev[] = "/dev/input/event0";
    static const char filename[] = "/dev/Tetris";
    const int input_size = 512;
    unsigned char input_data[input_size];
    struct pollfd fds[1];

    // Open the device.
    if ((fds[0].fd = open(input_dev, O_RDONLY | O_NONBLOCK)) == -1 || (tetris_fd = open(filename, O_RDWR)) == -1)
    {
        fprintf(stderr, "Check the joystick or the module\n");
        return -1;
    }

    int i = 0;
    int key_type = 0, key = 0, value = 0;
    tetris_block block;
    int new_type = 0;

    fds[0].events = POLLIN;
    int downThreshold[3] = {120, 80, 40};

    while(1)
    {
        int ret = poll(fds, 1, 5);

        /* The key values
        * A: 33
        * B: 34
        * X: 32
        * Y: 35
        * L: 36
        * R: 37
        * SELECT: 40
        * START: 41 
        * y-axis: 1
        * x-axis: 0
        */
        if (ignoreNext == 1) ignoreNext = 0;
        else if (ret > 0)
        {
            key = -1;
            value = -1;
            memset(input_data, 0, input_size);
	        ssize_t read_res = read(fds[0].fd, input_data, input_size);

	        for(i = 0; i < read_res / 16 - 1; i++)
	        {
                key_type = input_data[i * 16 + 8];
                if(key_type == 1)
                {
                    // button
                    key = input_data[i * 16 + 10];
                    value = input_data[i * 16 + 12];
                }
                else if(key_type == 3)
                {
                    //axis
                    key = input_data[i * 16 + 10];
                    value = input_data[i * 16 + 12];
                    switch(value){
                        case 0: 
                            value = -1;
                            break;
                        case 255: 
                            value = 1;
                            break;
                        default:
                            value = 0;
                            break;
                    }
                }

		// Received a new button press.
                if (value == 1)
                {
                    // A, rotate.
                    if (key == 33 && pauseFlag == 0) rotate(&block);
                    // L & R, move.
                    else if (key == 36 && pauseFlag == 0) move(-1, 0, &block);
                    else if (key == 37 && pauseFlag == 0) move(1, 0, &block);
                    // Start, pause or reset the game status.
                    else if (key == 41)
                    {
                        if (gameEnd == 1) new_type = reset(&block);
                        else
                        {
                            pauseFlag = 1 - pauseFlag;
                            set_pause(tetris_fd, pauseFlag);
			}
                    }
                    // Down, speed up to the bottom.
                    else if (key == 1 && pauseFlag == 0) down_key = 1;
                    // Select, change speed.
                    else if (key == 40 && pauseFlag == 0)
                    {
                        speed = (speed + 1) % 3;
                        set_speed(tetris_fd, speed + 1);
                    }
                }
                else if (key == 1) down_key = 0;
	        }
        }
        else if (pauseFlag == 0 && (downCount++ >= downThreshold[speed] || (down_key == 1 && downCount++ >= 10)))
        {
            downCount = 0;
            if (move(1, 1, &block) == 1)
            {
                if (testAndCreate(new_type, &block) < 0)
                {
                    printf("Game Over!\n");
                    gameEnd = 1;
		    usleep(1000000);
		    set_pause(tetris_fd, 3);
                }
                else
                {
                    new_type = set_next(tetris_fd, new_type);
                    down_key = 0;
                }
            }
        }
    }

    close(fds[0].fd);
    return 0;
}
