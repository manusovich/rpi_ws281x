/*
 * main.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)                        (sizeof(stuff) / sizeof(stuff[0]))

#define TARGET_FREQ                              WS2811_TARGET_FREQ
#define GPIO_PIN                                 18
#define DMA                                      5

#define WIDTH                                    18
#define HEIGHT                                   14
#define LED_COUNT                                (WIDTH * HEIGHT)


ws2811_t ledstring =
        {
                .freq = TARGET_FREQ,
                .dmanum = DMA,
                .channel =
                        {
                                [0] =
                                        {
                                                .gpionum = GPIO_PIN,
                                                .count = LED_COUNT,
                                                .invert = 0,
                                                .brightness = 255,
                                        },
                                [1] =
                                        {
                                                .gpionum = 0,
                                                .count = 0,
                                                .invert = 0,
                                                .brightness = 0,
                                        },
                        },
        };

struct RGB {
    int r;
    int g;
    int b;
};

// thunderstorm20, 10, 0,
int thunderstorm[] = {100, 0, 0, 0, 0, 0, 70, 0, 100, 70, 50, 30, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 50, 30, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 30, 10, 0, 0, 0, 0, 100, 70, 50, 30, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // %
int gthunderstorm[] = {-1, -1, 3, -1, -1, 0, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};


float dotposition[] = {1, 4, 11, 8, 3, 12, 6, 10, 2, 11, 10, 14, 5, 16, 2, 7};
float dotdirection[] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, -1, 1, -1};
int dotspos[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
ws2811_led_t dotcolors[] = // should not be more than 0xDD (!)
        {
                0x600000,  // red 0
                0x603000,  // orange 1
                0x006000,  // green 2
                0x606000,  // yellow 3
                0x006000,  // green 4
                0x006060,  // lightblue 5
                0x000060,  // blue 6
                0x300030,  // purple 7
                0x600030,  // pink 8
                0x300030,  // purple 7
                0x000060,  // blue 6
                0x006000,  // green 4
                0x606000,  // yellow 3
                0x603000,  // orange 1
                0x600000,  // red 0
        };


ws2811_led_t matrix[WIDTH][HEIGHT];


ws2811_led_t createRGB(int r, int g, int b) {
    return (ws2811_led_t) (((g & 0xff) << 16) + ((b & 0xff) << 8) + (r & 0xff));
}

struct RGB getRGB(int hexValue) {
    struct RGB rgbColor;
    rgbColor.g = (hexValue >> 16) & 0xff;
    rgbColor.b = (hexValue >> 8) & 0xff;
    rgbColor.r = hexValue & 0xff;
    return rgbColor;
}

ws2811_led_t up(ws2811_led_t color, float m) {
    struct RGB rgb = getRGB(color);
    rgb.r = (int) (rgb.r * m);
    rgb.b = (int) (rgb.b * m);
    rgb.g = (int) (rgb.g * m);
    return createRGB(rgb.r, rgb.g, rgb.b);
}


void matrix_render(void) {
    int x, y;

    for (x = 0; x < WIDTH; x++) {
        for (y = 0; y < HEIGHT; y++) {
            ledstring.channel[0].leds[(y * WIDTH) + x] = matrix[x][y];
        }
    }
}

void matrix_render_fill(int color) {
    int x, y;

    for (x = 0; x < WIDTH; x++) {
        for (y = 0; y < HEIGHT; y++) {
            ledstring.channel[0].leds[(y * WIDTH) + x] = (ws2811_led_t) color;
        }
    }
}

void matrix_raise(void) {
    int x, y;

    for (y = 0; y < (HEIGHT - 1); y++) {
        for (x = 0; x < WIDTH; x++) {
            matrix[x][y] = matrix[x][y + 1];
        }
    }
}

void matrix_fill(int c) {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            matrix[x][y] = (ws2811_led_t) c;
        }
    }
}

void matrix_fade() {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            struct RGB rgb = getRGB(matrix[x][y]);
            double d = 1.1;
            if (x == 0 || x == WIDTH - 1) {
                d = 1.05;
            }
            rgb.r = (int) (rgb.r / d);
            rgb.b = (int) (rgb.b / d);
            rgb.g = (int) (rgb.g / d);
            matrix[x][y] = createRGB(rgb.r, rgb.g, rgb.b);
        }
    }
}

void matrix_render_colors(void) {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        //  struct RGB target = getRGB(dotcolors[y]);
        for (x = 0; x < WIDTH; x++) {
            // struct RGB rgb = getRGB(matrix[x][y]);
            ws2811_led_t color = dotcolors[y];

//                    createRGB(
//                    abs(target.r - rgb.r) / 2,
//                    abs(target.g - rgb.g) / 2,
//                    abs(target.b = rgb.b) / 2);
            matrix[x][y] = color;
        }
    }
}


void matrix_render_white_and_black(void) {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        //  struct RGB target = getRGB(dotcolors[y]);
        for (x = 0; x < WIDTH; x++) {
            ws2811_led_t color;
//            if (x % 2  == 0) {
//                color = ((ws2811_led_t) 0x333333);
//            } else {
            color = ((ws2811_led_t) 0x000000);
//            if (x % 2)  {
//                color =  ((ws2811_led_t) 0x333333);
//
//            }
//            }
            // struct RGB rgb = getRGB(matrix[x][y]);
//            ws2811_led_t color =  dotcolors[y];
//
//                    createRGB(
//                    abs(target.r - rgb.r) / 2,
//                    abs(target.g - rgb.g) / 2,
//                    abs(target.b = rgb.b) / 2);
            matrix[x][y] = color;
        }
    }
}

void matrix_render_exciter(void) {
    int y;

    for (y = 0; y < HEIGHT; y++) {
        //  struct RGB rgb = getRGB(matrix[dotposition[y]][y]);

        ws2811_led_t color = dotcolors[y];
        int pos = (int) dotposition[y];
        if (pos >= (WIDTH - 1)) {
            pos = WIDTH - 1;
            color = up(color, 2);
        }
        if (pos <= 0) {
            pos = 0;
            color = up(color, 2);
        }

        matrix[pos][y] = color;

        if (dotposition[y] >= WIDTH - 1 && dotdirection[y] > 0) {
            dotdirection[y] = -((20 + (float) (rand() % 50)) / 100);
        }

        if (dotposition[y] <= 0 && dotdirection[y] < 0) {
            dotdirection[y] = (20 + (float) (rand() % 50)) / 100;
        }

        dotposition[y] = dotposition[y] + dotdirection[y];

    }

}


void matrix_render_number(int num) {
    int nums[10][18 * 8] = {
            {//0
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//1
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//2
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//3
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//4
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//5
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//6
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//7
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//8
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            },
            {//9
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            }
    };


    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            if (y < 8) {
                int pos = y * WIDTH + x;
                if (nums[num / 10][pos] > 0) {
                    matrix[x][y] = 0xAAAAAA;
                }
            }
            if (y > 6 && y < 13) {
                int pos = (y - 6) * WIDTH + x;
                if (nums[num % 10][pos] > 0) {
                    matrix[x][y] = 0xAAAAAA;
                }
            }
        }
    }
}

void matrix_render_thunderstorm() {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        if (gthunderstorm[y] >= 0) {
            int intensity = thunderstorm[gthunderstorm[y]];
            struct RGB rgb = getRGB(dotcolors[y]);
            ws2811_led_t color = createRGB(
                    rgb.r + (int) ((double) 0x20 / 100 * intensity),
                    rgb.g + (int) ((double) 0x20 / 100 * intensity),
                    rgb.b + (int) ((double) 0x20 / 100 * intensity));
            for (x = 0; x < WIDTH; x++) {
                matrix[x][y] = color;
            }
            gthunderstorm[y]++;
            if (gthunderstorm[y] == ARRAY_SIZE(thunderstorm)) {
                gthunderstorm[y] = 0;
            }
        }
    }
}


void matrix_bottom(void) {
    int i;

    for (i = 0; i < ARRAY_SIZE(dotspos); i++) {
        dotspos[i]++;
        if (dotspos[i] > (WIDTH - 1)) {
            dotspos[i] = 0;
        }

        matrix[dotspos[i]][HEIGHT - 1] = dotcolors[i];
    }
}

static void ctrl_c_handler(int signum) {
    ws2811_fini(&ledstring);
}

static void setup_handlers(void) {
    struct sigaction sa =
            {
                    .sa_handler = ctrl_c_handler,
            };

    sigaction(SIGKILL, &sa, NULL);
}

int main(int argc, char *argv[]) {
    int ret = 0;

    setup_handlers();

    if (ws2811_init(&ledstring)) {
        return -1;
    }

    long c = 0;


    while (1) {
        //matrix_render_exciter();
        //matrix_render_thunderstorm();
        //  matrix_render_colors();
//        matrix_render_white_and_black();

//        matrix_render_fill(0);

//        if (ws2811_render(&ledstring)) {
//            ret = -1;
//            break;
//        }




        //if (c % 2 == 0) {
        matrix_fade();
        matrix_render_fill(0);
        //matrix_render_exciter();
        matrix_render_number((int) (c % 100));
        //matrix_render_colors();
        matrix_render();

        //} else {
        //    matrix_render_fill(0);
        // }

        if (ws2811_render(&ledstring)) {
            ret = -1;
            break;
        }

        // 15 frames /sec
        usleep(1000000 / 2);
        c++;
    }

    ws2811_fini(&ledstring);

    return ret;
}

