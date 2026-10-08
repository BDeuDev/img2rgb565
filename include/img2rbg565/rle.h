#ifndef RLE_H
#define RLE_H

#include "stdint.h"
#include "img2rbg565/img.h"
#include "stdlib.h"
#include "stdio.h"

typedef struct
{
    uint16_t count;
    uint16_t pixel;
} rle_pixel_t;

typedef struct
{
    img_header_t header;
    uint32_t px_count;
    rle_pixel_t *pixels;
} rle_img_t;

rle_img_t *rle_encoder(img_t *img);
img_t *rle_decoder(uint32_t rle_img_width, uint32_t rle_img_height, size_t rle_img_px_count, const uint16_t *data);

#endif

#ifdef RLE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

rle_img_t *rle_encoder(img_t *img)
{
    size_t pixels = (size_t)img->header.width * img->header.height;

    if (pixels == 0)
    {
        return NULL;
    }

    rle_img_t *rle_img = malloc(sizeof(rle_img_t));
    rle_pixel_t *rle = calloc(pixels, sizeof(rle_pixel_t));

    if (!rle_img || !rle)
    {
        free(rle_img);
        free(rle);
        return NULL;
    }

    size_t cursor = 0;

    rle[0].pixel = img->pixels[0];
    rle[0].count = 1;

    for (size_t i = 1; i < pixels; i++)
    {
        uint16_t pixel = img->pixels[i];

        if (pixel == rle[cursor].pixel && rle[cursor].count < UINT16_MAX)
        {
            rle[cursor].count++;
        }
        else
        {
            cursor++;

            rle[cursor].pixel = pixel;
            rle[cursor].count = 1;
        }
    }

    size_t px_count = cursor + 1;

    rle_img->header = img->header;
    rle_img->px_count = px_count;
    rle_img->pixels = rle;

    return rle_img;
}

img_t *rle_decoder(uint32_t rle_img_width, uint32_t rle_img_height, size_t rle_img_px_count, const uint16_t *data)
{
    size_t total_pixels = (size_t)rle_img_height * rle_img_width;
    uint16_t *pixels = malloc(total_pixels * sizeof(uint16_t));
    img_t *img = malloc(sizeof(img_t));

    if (!pixels || !img)
    {
        free(pixels);
        free(img);
        return NULL;
    }

    printf("COUNT %zu\n", rle_img_px_count);
    size_t cursor = 0;

    for (size_t i = 0; i < rle_img_px_count; i++)
    {
        uint16_t count = data[0];
        uint16_t pixel = data[1];

        while (count && cursor < total_pixels)
        {
            pixels[cursor++] = pixel;
            count--;
        }

        data += 2;
    }

    printf("TOTAL count %zu\n", cursor);
    printf("TOTAL ORIGINAL count %zu\n", total_pixels);

    img->header.width = rle_img_width;
    img->header.height = rle_img_height;
    img->pixels = pixels;
    return img;
}

#endif
