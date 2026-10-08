#include "stb_image.h"
#include "stb_image_resize2.h"
#include <stdio.h>
#include <stdint.h>
#include "string.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "img2rbg565/rgb565.h"
#include "img2rbg565/rle.h"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320

static const char *program_name = "img2rgb565";

typedef struct
{
    int w;
    int h;
    char *o;
    char *f;
} State;

typedef void (*CommandFn)(State *, char *);

typedef struct
{
    char *desc;
    char *flag;
    char *usage;
    CommandFn execute;
} Command;

void cmd_output_path(State *state, char *value)
{
    state->o = value;
}

void cmd_width(State *state, char *value)
{
    int p;

    sscanf(value, "%d", &p);
    state->w = p;
}

void cmd_height(State *state, char *value)
{
    int p;

    sscanf(value, "%d", &p);
    state->h = p;
}

void cmd_format(State *state, char *value)
{
    state->f = value;
}

static const Command commands[] = {
    {"set output path", "-o", "img2rgb565  ", cmd_output_path},
    {"set new width for the image", "-w", "", cmd_width},
    {"set new height for the image", "-h", "", cmd_height},
    //{"set format output for the image", "-f", "", cmd_format},
};

static const size_t cmd_len = sizeof(commands) / sizeof(commands[0]);

int main(int argc, char **argv)
{
    State state = {0};

    state.h = LCD_HEIGHT;
    state.w = LCD_WIDTH;

    for (size_t i = 1; i < argc; i++)
    {

        for (size_t j = 0; j < cmd_len; j++)
        {

            Command cmd = commands[j];

            if (strcmp(cmd.flag, argv[i]) == 0)
            {
                if (i + 1 < argc)
                {
                    cmd.execute(&state, argv[i + 1]);
                    i++;
                    break;
                }
                else
                {
                    fprintf(stderr, "Error: The flag %s requires an argument.\n", argv[i]);
                    return 1;
                }
            }
        }
    }

    if (argc < 2)
    {
        printf("Usage: %s img.png\n", program_name);
        return 1;
    }

    uint32_t width;
    uint32_t height;
    int channels;

    uint8_t *pixels = stbi_load(
        argv[1],
        &width,
        &height,
        &channels,
        0);

    if (pixels == NULL)
    {
        printf("Error: %s\n", stbi_failure_reason());
        return;
    }

    uint8_t *resized = malloc(state.w * state.h * channels);

    stbir_resize_uint8_linear(
        pixels,
        width,
        height,
        0,
        resized,
        state.w,
        state.h,
        0,
        (stbir_pixel_layout)channels);

    // old
    conv_to_rgb565(state.o, state.w, state.h, resized, channels);

    // new
    img_t *ptr = conv_to_rgb565_px(state.o, state.w, state.h, resized, channels);

    rle_img_t *rle_img = rle_encoder(ptr);
    printf("%u\n", ptr->header.width);

    printf("PX C %u", rle_img->px_count);

    create_dirs_r(state.o, 0777);

    FILE *file = fopen(strcat(state.o, ".rle"), "wb");

    fwrite(&rle_img->header.width, sizeof(uint32_t), 1, file);
    fwrite(&rle_img->header.height, sizeof(uint32_t), 1, file);
    fwrite(&rle_img->px_count, sizeof(uint32_t), 1, file);
    
    for (size_t i = 0; i < rle_img->px_count; i++)
    {
        fwrite(&rle_img->pixels[i].count, sizeof(uint16_t), 1, file);    
        fwrite(&rle_img->pixels[i].pixel, sizeof(uint16_t), 1, file);    
    }
    
    img_t *img = rle_decoder(rle_img->header.width, rle_img->header.height, rle_img->px_count, (const uint16_t *)rle_img->pixels);

    printf("%u\n", rle_img->px_count);
    free(img);
    free(rle_img);
    free(ptr);
    free(resized);
    stbi_image_free(pixels);
    return 0;
}