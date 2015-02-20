#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
                                        }
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

float dotposition[] = {15, 4, 11, 8, 0, 12, 6, 10, 2, 13, 3, 14, 5, 9, 7, 16};
float dotdirection[] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, -1, 1, -1, 1};
int forecast[]  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int wind[]      = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int precip[]    = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int precippos[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// paletton.com
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
            ledstring.channel[0].leds[(y * WIDTH) + x] =
                    createRGB((int) xrgb.r, (int) xrgb.g, (int) xrgb.b);
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
    int a = max;

    if (f < 0) {
        f = 0;
    }

    int pos = (int) ((float) ARRAY_SIZE(dotcolors) / a * f);
    ws2811_led_t color = up(dotcolors[pos], 0.3);
    return color;
}

void matrix_fade() {
    int x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            struct RGB rgb = getRGB(forecast_color(y));
            struct XRGB xrgb = matrix[x][y];

            double d = 0.98;

            if (xrgb.r > rgb.r) {
                xrgb.r = (float) (xrgb.r * d);
                xrgb.g = (float) (xrgb.g * d);
                xrgb.b = (float) (xrgb.b * d);
            }
            if (xrgb.r < rgb.r) {
                xrgb.r += (float) (rgb.r * (1 - d));
                xrgb.g += (float) (rgb.g * (1 - d));
                xrgb.b += (float) (rgb.b * (1 - d));
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

int precip_level(int value) {
    if (value > 0 && value < 50) {
        return 1;
    } else if (value >= 50 && value < 100) {
        return 2;
    } else if (value >= 100) {
        return 3;
    }
    return 0;
}

void matrix_render_wind(void) {
    int y;

    for (y = 0; y < HEIGHT; y++) {
        int offset = precip_level(precip[y]);

        int pos = (int) dotposition[y];
        if (pos >= (WIDTH - (1 + offset))) {
            pos = WIDTH - (1 + offset);
        }
        if (pos <= offset) {
            pos = offset;
        }

        matrix[pos][y] = getXRGB(up(forecast_color(y), 2));

        if (dotposition[y] >= WIDTH - 1 && dotdirection[y] > 0) {
            dotdirection[y] = -(float) wind[y] / 1500;
        }

        if (dotposition[y] <= 0 && dotdirection[y] < 0) {
            dotdirection[y] = (float) wind[y] / 1500;
        }

        dotposition[y] = dotposition[y] + dotdirection[y];
    }
}

void matrix_render_precip(int counter) {
    int y, x;
    for (y = 0; y < HEIGHT; y++) {
        int pl = precip_level(precip[y]);
        if (pl > 0) {
            if (counter % 2 == 0) {
                precippos[y]++;
                if (precippos[y] > ARRAY_SIZE(dotcolors) - 1) {
                    precippos[y] = 0;
                }
            }
            struct XRGB color = getXRGB(up(dotcolors[precippos[y]], .3));
            for (x = 0; x < pl; x++) {
                int xa = x;
                int xb = WIDTH - 1 - x;
                matrix[xa][y] = color;
                matrix[xb][y] = color;
            }
        }
    }
}

void update_forecast(void) {
    FILE *fp;
    fp = fopen("/home/pi/rpi_ws281x/forecast", "r");
    if (fp == NULL) {
        perror("Error while opening the file.\n");
        exit(EXIT_FAILURE);
    }

    int cnt;
#define BUFFER_SIZE 14 * 3 * sizeof(int)
    unsigned char buffer[BUFFER_SIZE];
    fread(buffer, 1, BUFFER_SIZE, fp);
    for (cnt = 0; cnt < 14; cnt++) {
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
    int frames_per_second = 30;
    int ret = 0;

    setup_handlers();
    if (ws2811_init(&ledstring)) {
        return -1;
    }

    long c = 0;
    update_forecast();
    matrix_render_forecast();

    while (1) {
        matrix_fade();
        matrix_render_wind();
        matrix_render_precip(c);
        matrix_render();

        if (ws2811_render(&ledstring)) {
            ret = -1;
            break;
        }

        usleep((useconds_t) (1000000 / frames_per_second));
        c++;

        if (c % (frames_per_second * 60 * 5) == 0) {
            // each 5 minutes update forecast
            update_forecast();
        }
    }
    ws2811_fini(&ledstring);

    return ret;
}

