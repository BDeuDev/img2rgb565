#ifndef RGB565_H
#define RGB565_H

#include "stdio.h"
#include "stdint.h"
#include "sys/stat.h"
#include <sys/types.h>
#include "img2rbg565/img.h"

int create_dirs_r(const char *path, mode_t mode)
{
    char tmp[256];
    char *p;
    char *slash;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);

    slash = strrchr(tmp, '/');
    if (slash == NULL) {
        return 0;
    }

    *slash = '\0';

    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode);
            *p = '/';
        }
    }

    return mkdir(tmp, mode);
}

static void conv_to_rgb565(const char *opath, int width, int height, uint8_t *pixels, int channels)
{
    create_dirs_r(opath, 0777);

    FILE *file = fopen(opath, "wb");

    fwrite(&width, sizeof(width), 1, file);
    fwrite(&height, sizeof(height), 1, file);

    for (size_t i = 0; i < width * height; i++)
    {
        uint8_t r = pixels[i * channels + 0];
        uint8_t g = pixels[i * channels + 1];
        uint8_t b = pixels[i * channels + 2];

        uint16_t rgb565 =
            ((r >> 3) << 11) |
            ((g >> 2) << 5) |
            (b >> 3);
        rgb565 = (rgb565 >> 8) | (rgb565 << 8);

        fwrite(&rgb565, sizeof(rgb565), 1, file);
    }
}

static img_t *conv_to_rgb565_px(const char *opath, int width, int height, uint8_t *pixels, int channels)
{
    img_header_t h = {
        .height = height,
        .width = width,
    };

    img_t *img = malloc(sizeof(img_t));

    img->header = h;

    uint16_t *rgb565_buff = malloc((width * height ) * sizeof(uint16_t));

    for (size_t i = 0; i < width * height; i++)
    {
        uint8_t r = pixels[i * channels + 0];
        uint8_t g = pixels[i * channels + 1];
        uint8_t b = pixels[i * channels + 2];

        uint16_t rgb565 =
            ((r >> 3) << 11) |
            ((g >> 2) << 5) |
            (b >> 3);
        rgb565 = (rgb565 >> 8) | (rgb565 << 8);

        rgb565_buff[i] = rgb565;
    }

    img->pixels = rgb565_buff;

    return img;
}

#endif