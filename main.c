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

struct XRGB {
    float r;
    float g;
    float b;
};

float dotposition[] = {1, 4, 11, 8, 3, 12, 6, 10, 2, 11, 10, 14, 5, 16, 2, 7};
float dotdirection[] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, -1, 1, -1};
int forecast[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int wind[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int precip[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

ws2811_led_t dotcolors[] =
        {
                0x882D61, 0x6F256F, 0x582A72, 0x4B2D73, 0x403075, 0x343477, 0x2E4272, 0x29506D, 0x226666,
                0x277553, 0x2D882D, 0x609732, 0x7B9F35, 0x91A437, 0xAAAA39, 0xAAA039, 0xAA9739, 0xAA8E39,
                0xAA8439, 0xAA7939, 0xAA6C39, 0xAA5939, 0xAA3939
        };

struct XRGB matrix[WIDTH][HEIGHT];

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

struct XRGB getXRGB(int hexValue) {
    struct XRGB rgbColor;
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
            struct XRGB xrgb = matrix[x][y];
            ledstring.channel[0].leds[(y * WIDTH) + x] = createRGB((int) xrgb.r, (int) xrgb.g, (int) xrgb.b);
        }
    }
}

ws2811_led_t forecast_color(int y) {
    int f = 0;
    if (y == 0 || y == 1 || y == 2) {
        f = forecast[0];
    } else {
        f = forecast[y - 2];
    }

    int max = 9999;
    int min = 3000;
    int a = max - min;
    int c = f - min;
    if (c < 0) {
        c = 0;
    }

    int pos = (int) ((float) ARRAY_SIZE(dotcolors) / a * c);
    ws2811_led_t color = up(dotcolors[pos], 0.3);
    return color;
}

void matrix_fade() {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            struct RGB rgb = getRGB(forecast_color(y));
            struct XRGB xrgb = matrix[x][y];

            double d = 0.95;

            if (xrgb.r > rgb.r) {
                xrgb.r = (float) (xrgb.r * d);
                xrgb.g = (float) (xrgb.g * d);
                xrgb.b = (float) (xrgb.b * d);
            }

            matrix[x][y] = xrgb;
        }
    }
}

void matrix_render_forecast(void) {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        struct XRGB color = getXRGB(forecast_color(y));
        for (x = 0; x < WIDTH; x++) {
            matrix[x][y] = color;
        }
    }
}

void matrix_render_exciter(void) {
    int y;

    for (y = 0; y < HEIGHT; y++) {
        //  struct RGB rgb = getRGB(matrix[dotposition[y]][y]);

        int pos = (int) dotposition[y];
        if (pos >= (WIDTH - 1)) {
            pos = WIDTH - 1;
        }
        if (pos <= 0) {
            pos = 0;
        }

        matrix[pos][y] = getXRGB(up(forecast_color(y), 2));

        if (dotposition[y] >= WIDTH - 1 && dotdirection[y] > 0) {
            dotdirection[y] = - wind[y] / 400;
        }

        if (dotposition[y] <= 0 && dotdirection[y] < 0) {
            dotdirection[y] = wind[y] / 400;
        }

        dotposition[y] = dotposition[y] + dotdirection[y];

    }

}


//void matrix_render_number(int num) {
//    int nums[10][18 * 8] = {
//            {//0
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//1
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//2
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//3
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//4
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//5
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//6
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//7
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//8
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            },
//            {//9
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
//                    0, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0,
//                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
//            }
//    };
//
//
//    int x, y;
//
//    for (y = 0; y < HEIGHT; y++) {
//        for (x = 0; x < WIDTH; x++) {
//            if (y < 8) {
//                int pos = y * WIDTH + x;
//                if (nums[num % 10][pos] > 0) {
//                    matrix[x][y] = 0x777777;
//                }
//            }
//            if (y > 6 && y < 13) {
//                int pos = (y - 6) * WIDTH + x;
//                if (nums[num / 10][pos] > 0) {
//                    matrix[x][y] = 0x777777;
//                }
//            }
//        }
//    }
//}
//
//void matrix_render_thunderstorm() {
//    int x, y;
//
//    for (y = 0; y < HEIGHT; y++) {
//        if (gthunderstorm[y] >= 0) {
//            int intensity = thunderstorm[gthunderstorm[y]];
//            struct RGB rgb = getRGB(dotcolors[y]);
//            ws2811_led_t color = createRGB(
//                    rgb.r + (int) ((double) 0x20 / 100 * intensity),
//                    rgb.g + (int) ((double) 0x20 / 100 * intensity),
//                    rgb.b + (int) ((double) 0x20 / 100 * intensity));
//            for (x = 0; x < WIDTH; x++) {
//                matrix[x][y] = color;
//            }
//            gthunderstorm[y]++;
//            if (gthunderstorm[y] == ARRAY_SIZE(thunderstorm)) {
//                gthunderstorm[y] = 0;
//            }
//        }
//    }
//}

void update_forecast(void) {
    FILE *fp;
    fp = fopen("/home/pi/rpi_ws281x/forecast", "r");
    if (fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    int cnt;
    #define BUFFER_SIZE 12 * 3 * sizeof(int)
    unsigned char buffer[BUFFER_SIZE];
    fread(buffer, 1, BUFFER_SIZE, fp);
    for (cnt = 0; cnt < 12; cnt++) {
        int pos = (int) (cnt * 3 * sizeof(int));
        forecast[cnt] = buffer[pos + 3] + ((int) buffer[pos + 2] << 8)
                + ((int) buffer[pos + 1] << 16) + ((int) buffer[pos] << 24);
        wind[cnt] = buffer[pos + 7] + ((int) buffer[pos + 6] << 8)
                + ((int) buffer[pos + 5] << 16) + ((int) buffer[pos + 4] << 24);
        precip[cnt] = buffer[pos + 11] + ((int) buffer[pos + 10] << 8)
                + ((int) buffer[pos + 9] << 16) + ((int) buffer[pos + 8] << 24);
        printf("Temp: %d, Wind: %d, Precip: %d\n", forecast[cnt], wind[cnt], precip[cnt]);
    }
    fclose(fp);
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
    // 15 frames /sec
    int frames_per_second = 15;


    int ret = 0;

    setup_handlers();

    if (ws2811_init(&ledstring)) {
        return -1;
    }

    long c = 0;

    update_forecast();

    printf("Run loopd\n");

    matrix_render_forecast();

    while (1) {
        matrix_fade();
        matrix_render_exciter();
        matrix_render();

        if (ws2811_render(&ledstring)) {
            ret = -1;
            break;
        }

        usleep((useconds_t) (1000000 / frames_per_second));

        c++;

        if (c % (frames_per_second * 60 * 10) == 0) {
            // each 10 minutes update forecast
            update_forecast();
        }
    }
    ws2811_fini(&ledstring);

    return ret;
}

