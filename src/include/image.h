#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

typedef struct {
    uint8_t r, b, g, a;
} pixel_t;

typedef struct {
    uint8_t **pixels;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
} image_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
} ihdr_t;

typedef enum {
    KERNEL_SOBEL_X = 0,
    KERNEL_SOBEL_Y = 1,
    KERNEL_SOBEL_COMBINED = 2,
    KERNEL_GAUSSIAN = 3,
    KERNEL_BLUR = 4,
    KERNEL_LAPLACIAN = 5,
    KERNEL_SHARPEN = 6,
    KERNEL_NONE = 7
} kernel_type;

enum {
    FILTER_NONE  = 0,
    FILTER_SUB   = 1,
    FILTER_UP    = 2,
    FILTER_AVG   = 3,
    FILTER_PAETH = 4
};

uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c);
void unfilter_scanline(uint8_t *current, uint8_t *previous, uint32_t length, uint32_t bpp, uint8_t filter_type);
image_t *process_idat_chunks(ihdr_t *ihdr, uint8_t *idat_data, uint64_t idat_size);
uint8_t **rgb_to_grayscale(image_t *image);
void apply_convolution(uint8_t **input, uint8_t **output, uint32_t height, uint32_t width, uint8_t kernel_type);

#endif
