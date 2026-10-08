# img2rgb565

Command-line tool written in C that converts an image (JPEG, PNG, BMP, etc.) to the RGB565 pixel format, ready to be displayed on an ST7796 LCD driven over SPI by an ESP32.

For each image it generates two files: a raw one and an RLE-compressed one.

```sh
img2rgb565 photo.jpg -o photo.rgb565
# generates photo.rgb565 and photo.rgb565.rle, both 480x320
```

## Table of contents

1. [Project structure](#project-structure)
2. [Building and installing](#building-and-installing)
3. [Usage](#usage)
4. [Conversion pipeline](#conversion-pipeline)
5. [RGB565 pixel format](#rgb565-pixel-format)
6. [File formats](#file-formats)
7. [RLE compression](#rle-compression)
8. [Header API](#header-api)
9. [Using the files on the ESP32](#using-the-files-on-the-esp32)
10. [Known limitations](#known-limitations)

## Project structure

```
img2rgb565/
├── CMakeLists.txt
├── include/
│   ├── stb_image.h              image decoding (third-party library)
│   ├── stb_image_resize2.h      resizing (third-party library)
│   └── img2rbg565/
│       ├── img.h                img_header_t and img_t types
│       ├── rgb565.h             RGB888 -> RGB565 conversion and raw file writer
│       └── rle.h                RLE encoder and decoder (header-only)
└── src/
    ├── main.c                   CLI and pipeline orchestration
    ├── rle.c                    instantiates the rle.h implementation
    ├── stb_image.c              instantiates the stb_image.h implementation
    └── stb_image_resize2.c      instantiates the stb_image_resize2.h implementation
```

The headers in `include/img2rbg565/` are shared with the ESP32 firmware, which keeps its own copy. There is no automatic sync; when one is modified it has to be copied to the other.

`rle.h` and the stb libraries follow the *single-header* pattern: the header always declares the prototypes, and the implementation is only compiled in the translation unit that defines the matching macro before the `#include`.

| Macro | Defined in | Enables the implementation of |
|---|---|---|
| `RLE_IMPLEMENTATION` | `src/rle.c` | `rle_encoder`, `rle_decoder` |
| `STB_IMAGE_IMPLEMENTATION` | `src/stb_image.c` | `stbi_load`, etc. |
| `STB_IMAGE_RESIZE_IMPLEMENTATION` | `src/stb_image_resize2.c` | `stbir_resize_uint8_linear`, etc. |

## Building and installing

Requires CMake ≥ 3.20 and a C11 compiler. The only link dependency is `libm`.

```sh
cmake -B build
cmake --build build
```

The executable is written to `build/img2rgb565`. To install it:

```sh
sudo cmake --install build                  # /usr/local/bin/img2rgb565
cmake --install build --prefix ~/.local     # ~/.local/bin/img2rgb565, no sudo
```

## Usage

```sh
img2rgb565 <image> -o <output> [-w <width>] [-h <height>]
```

| Argument | Required | Default | Description |
|---|---|---|---|
| `<image>` | yes | — | Input image. It must be the **first** argument (`argv[1]`). |
| `-o <path>` | yes | — | Path of the raw file. The RLE file is written to `<path>.rle`. Missing parent directories are created. |
| `-w <n>` | no | 480 | Output width in pixels. |
| `-h <n>` | no | 320 | Output height in pixels. |

Example:

```sh
img2rgb565 hk47.jpg -o hk47.rgb565
# generates hk47.rgb565 and hk47.rgb565.rle, both 480x320
```

The 480×320 defaults match the ST7796 panel in landscape orientation (`LCD_WIDTH` and `LCD_HEIGHT` in `src/main.c`).

### Input formats

Whatever `stb_image` supports: JPEG, PNG, BMP, TGA, GIF (first frame), PSD, HDR and PNM. **WebP is not supported.**

### Argument parsing

`main` walks `argv` and compares each argument against the `commands[]` table. Each entry maps a flag to a `CommandFn` that receives the state and the value that follows the flag:

```c
static const Command commands[] = {
    {"set output path", "-o", "img2rgb565  ", cmd_output_path},
    {"set new width for the image", "-w", "", cmd_width},
    {"set new height for the image", "-h", "", cmd_height},
};
```

An argument that matches no flag is ignored by this loop; that is why the input path needs no flag, but it is always read from `argv[1]`. A flag without a value ends the program with exit code 1. Adding an option only takes writing its `cmd_*` function and adding a row to the table.

## Conversion pipeline

```
input image
      │  stbi_load                     decodes to 8 bits per channel, keeping the file's native channels
      ▼
RGB(A) buffer at the original size
      │  stbir_resize_uint8_linear     resizes to exactly w × h
      ▼
RGB(A) buffer of w × h
      ├──► conv_to_rgb565              converts and writes <output>
      │
      └──► conv_to_rgb565_px           converts into an in-memory img_t
                 │  rle_encoder
                 ▼
            rle_img_t ──► <output>.rle is written
                 │  rle_decoder
                 ▼
            img_t (verification; prints the number of decoded pixels)
```

Details of each stage:

1. **Decoding.** `stbi_load(argv[1], &width, &height, &channels, 0)`. The trailing `0` asks for the channels as stored in the file (3 for JPEG, 3 or 4 for PNG, etc.).
2. **Resizing.** `stbir_resize_uint8_linear` brings the image to exactly `w × h`. **The aspect ratio is not preserved**: if the source image is not 3:2, it gets stretched. Filtering is done on the values as they are, with no sRGB gamma correction.
3. **RGB565 conversion.** It runs twice over the same resized buffer: `conv_to_rgb565` writes straight to disk and `conv_to_rgb565_px` returns an in-memory `img_t` for the encoder. Both produce exactly the same pixel values.
4. **Compression.** `rle_encoder` walks the pixels in raster order (row by row, left to right, top to bottom) and groups consecutive equal ones.
5. **Writing the `.rle` file.** A 12-byte header followed by the runs.
6. **Verification.** The data is decoded again with `rle_decoder`, which prints `TOTAL count` (decoded pixels) and `TOTAL ORIGINAL count` (`w × h`). If the encoder works correctly, both numbers match. The decoded result is not compared pixel by pixel and is not used for anything else.

## RGB565 pixel format

Each pixel takes 16 bits: 5 for red, 6 for green and 5 for blue. The conversion drops the low bits of each 8-bit channel:

```c
uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
```

```
bit   15 14 13 12 11 | 10  9  8  7  6  5 |  4  3  2  1  0
       R  R  R  R  R |  G  G  G  G  G  G |  B  B  B  B  B
```

Then the two bytes are swapped:

```c
rgb565 = (rgb565 >> 8) | (rgb565 << 8);
```

The reason is the byte order on the wire. The ST7796 expects the high byte first, and the `esp_lcd` driver sends the buffer over SPI byte by byte in memory order. Both the host (x86) and the ESP32 are little-endian, so an unswapped `uint16_t` would go out low byte first. With the swap done in the generator, the firmware can hand the buffer to the driver untouched.

Consequence: **on disk each pixel is big-endian**. To interpret a value read from the file as a "regular" RGB565 value, the bytes have to be swapped back.

Examples:

| Color | RGB888 | RGB565 | Bytes on disk |
|---|---|---|---|
| White | `FF FF FF` | `0xFFFF` | `FF FF` |
| Red | `FF 00 00` | `0xF800` | `F8 00` |
| Green | `00 FF 00` | `0x07E0` | `07 E0` |
| Blue | `00 00 FF` | `0x001F` | `00 1F` |

The alpha channel, if present, is ignored: only channels 0, 1 and 2 of each pixel are read.

## File formats

All integer header fields are little-endian (the native order of the host that generates the file). Pixel values are big-endian, as explained above.

### Raw file (`<output>`)

| Offset | Size | Type | Content |
|---|---|---|---|
| 0 | 4 | `int32` LE | width |
| 4 | 4 | `int32` LE | height |
| 8 | 2 × width × height | `uint16` BE each | pixels in raster order |

Total size: `8 + 2 × width × height` bytes. For 480×320 that is 307208 bytes.

### RLE file (`<output>.rle`)

| Offset | Size | Type | Content |
|---|---|---|---|
| 0 | 4 | `uint32` LE | width |
| 4 | 4 | `uint32` LE | height |
| 8 | 4 | `uint32` LE | number of runs (`px_count`) |
| 12 | 4 × `px_count` | runs | see below |

Each run takes 4 bytes:

| Offset within the run | Size | Type | Content |
|---|---|---|---|
| 0 | 2 | `uint16` LE | `count`: how many times the pixel repeats (1 to 65535) |
| 2 | 2 | `uint16` BE | `pixel`: RGB565 value with its bytes already swapped |

Total size: `12 + 4 × px_count` bytes. The sum of all `count` values equals `width × height`.

Real example, the first bytes of `colors.rgb565.rle`:

```
e0 01 00 00   width  = 0x000001E0 = 480
40 01 00 00   height = 0x00000140 = 320
c2 50 00 00   runs   = 0x000050C2 = 20674
06 00 ff ff   run 0: 6 pixels  of FF FF (white)
01 00 ff df   run 1: 1 pixel   of FF DF
3d 00 ff ff   run 2: 61 pixels of FF FF
```

The on-disk run layout matches `rle_pixel_t` in memory byte for byte (two `uint16_t` with no padding), so a pointer to the first run can be read as a `const uint16_t *` in `{count, pixel}` pairs, or as a `const rle_pixel_t *`.

## RLE compression

Run-Length Encoding: instead of storing every pixel, a (count, value) pair is stored for each stretch of consecutive equal pixels.

### Encoder

```c
rle[0].pixel = img->pixels[0];
rle[0].count = 1;

for (size_t i = 1; i < pixels; i++)
{
    uint16_t pixel = img->pixels[i];

    if (pixel == rle[cursor].pixel && rle[cursor].count < UINT16_MAX)
        rle[cursor].count++;
    else
    {
        cursor++;
        rle[cursor].pixel = pixel;
        rle[cursor].count = 1;
    }
}
```

- `cursor` points to the current run. A pixel equal to the run's pixel extends it; a different one opens a new run.
- `count` is a `uint16_t`, so a run is cut when it reaches 65535 and the next pixel opens another run with the same color. Without that cut, a flat background of more than 65535 pixels would overflow the counter.
- Runs cross row boundaries: the image is treated as a single sequence of `width × height` pixels.
- The run buffer is allocated for the worst case (`width × height` runs, one per pixel), so it never needs to grow.

### Decoder

It walks the `px_count` runs and writes `count` copies of `pixel` to the output buffer. It stops when the buffer is full (`cursor < total_pixels`), so a corrupt file cannot write outside the allocated memory.

### When it pays off

Each run costs 4 bytes and each raw pixel costs 2, so RLE only shrinks the file when the average run length is greater than 2. In the worst case (no two consecutive pixels are equal) the file is twice the size of the raw one.

| Image (480×320) | Runs | `.rle` | Raw | Ratio |
|---|---|---|---|---|
| `colors` (flat color areas) | 20674 | 82708 B | 307208 B | 27 % |
| `hk47` (photo) | 82007 | 328040 B | 307208 B | 107 % |

Photos, and any image that comes from a JPEG, have noise in almost every pixel, and resizing with a linear filter adds gradients along the edges. For that kind of image the raw file is the better choice.

## Header API

### `img.h`

```c
typedef struct { uint32_t width; uint32_t height; } img_header_t;
typedef struct { img_header_t header; uint16_t *pixels; } img_t;
```

`pixels` points to `width × height` RGB565 values in raster order, with their bytes already swapped.

### `rgb565.h`

| Function | Description |
|---|---|
| `int create_dirs_r(const char *path, mode_t mode)` | Creates every parent directory of `path` (equivalent to `mkdir -p` on its `dirname`). Does nothing if `path` has no `/`. |
| `void conv_to_rgb565(const char *opath, int width, int height, uint8_t *pixels, int channels)` | Converts the buffer to RGB565 and writes the raw file to `opath`. |
| `img_t *conv_to_rgb565_px(const char *opath, int width, int height, uint8_t *pixels, int channels)` | Converts the buffer to RGB565 and returns it in a `malloc`-allocated `img_t`. `opath` is unused. |

### `rle.h`

```c
typedef struct { uint16_t count; uint16_t pixel; } rle_pixel_t;
typedef struct { img_header_t header; uint32_t px_count; rle_pixel_t *pixels; } rle_img_t;
```

| Function | Description |
|---|---|
| `rle_img_t *rle_encoder(img_t *img)` | Compresses `img`. Returns `NULL` if the image has 0 pixels or if allocation fails. |
| `img_t *rle_decoder(uint32_t width, uint32_t height, size_t px_count, const uint16_t *data)` | Decompresses `px_count` runs read from `data` (`{count, pixel}` pairs). Allocates `width × height × 2` bytes; returns `NULL` if there is not enough memory. |

The caller owns the returned memory: both the structure and its `pixels` field have to be freed.

## Using the files on the ESP32

This section describes how the firmware uses the files (ESP-IDF, `esp_lcd` driver with an ST7796 panel).

**Embedding.** The file is copied into `main/` and declared in `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "rle.c" "main.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES esp_lcd driver esp_lcd_st7796
    EMBED_FILES "hk47.rgb565.rle"
)
```

ESP-IDF generates symbols from the file name, replacing every non-alphanumeric character with `_`. For `hk47.rgb565.rle`:

```c
extern const uint8_t _binary_hk47_rgb565_rle_start[];
extern const uint8_t _binary_hk47_rgb565_rle_end[];
```

The `_rle` suffix is part of the symbol; if it does not match the embedded file, linking fails with `undefined reference`.

**Reading the header.** Three consecutive `uint32_t` values (width, height, number of runs), followed by the runs.

**Stripe-by-stripe decoding.** A full 480×320 frame is 307200 bytes, and an ESP32 without PSRAM has no free heap block of that size, so `rle_decoder` returns `NULL` on the device. The firmware decodes the stream 20 lines at a time into a static buffer of `480 × 20 × 2 = 19200` bytes and draws each stripe with `esp_lcd_panel_draw_bitmap`. The state of the current run (`count`, `pixel`) is kept across stripes, because a run can start in one and end in the next.

**Two alternating buffers.** Over SPI, `esp_lcd_panel_draw_bitmap` queues the transfer and returns before it finishes. If the next stripe is decoded into the same buffer, it overwrites data that is still being sent and the image shows repeated stripes. That is why two buffers are alternated: the driver drains the queue at the start of the next `draw_bitmap`, so there is never more than one stripe in flight.

**Panel configuration.** `rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR`, `bits_per_pixel = 16`, `swap_xy` enabled and mirroring on both axes for the 480×320 landscape orientation.

## Known limitations

Behaviors of the current code to keep in mind:

- **`-o` is effectively required.** Without it, `state.o` stays `NULL` and the program fails when opening the output file.
- **The `.rle` path is built with `strcat(state.o, ".rle")` on `argv` memory.** It writes 4 bytes past the original argument and overwrites the next argument (or the environment). It breaks nothing today because it is one of the last things the program does, but the path should be built in its own buffer.
- **Output files are not checked for open errors** and `fclose` is never called; the data is flushed when the process exits.
- **Grayscale images.** The conversion always reads channels 0, 1 and 2. With 1 or 2 channels it reads components of neighboring pixels and, at the end of the buffer, past it. Supporting them would mean asking `stbi_load` for 3 channels.
- **The aspect ratio is not preserved** when resizing.
- **Load errors.** If `stbi_load` fails, the reason is printed and the program leaves through a bare `return;`, so the exit code is not reliable.
- **`create_dirs_r` uses a 256-byte buffer**: longer paths are truncated.
- **`rgb565.h` defines non-`static` functions in the header** (`create_dirs_r`), so it can only be included from a single translation unit.
- **Minor memory leaks** at the end of `main`: the `img_t` and `rle_img_t` structures are freed but their `pixels` buffers are not. It has no practical effect because the process exits right away.
- **Host endianness.** Header fields are written in native order. The documented format assumes a little-endian host, same as the ESP32.
- **The header directory is named `img2rbg565`** (with the letters `b` and `g` swapped relative to the project name). The `#include` lines of both projects depend on that name.
