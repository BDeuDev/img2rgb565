#ifndef IMG_H
#define IMG_H

#include "stdint.h"
#include "stdlib.h"

typedef struct
{
    uint32_t width;
    uint32_t height;
} img_header_t;

typedef struct {
    img_header_t header;
    uint16_t *pixels;
} img_t;


#endif