#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>
#include <stdint.h>

extern const uint8_t png_signature[8];

typedef struct {
    uint32_t size;
    uint8_t type[4];
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
    uint32_t crc;
} ihdr_t;

typedef struct {
    uint32_t size;
    uint8_t type[4];
    uint8_t *data;
    uint32_t crc;
} chunk_t;

typedef struct {
    ihdr_t ihdr;
    chunk_t *chunks;
    uint32_t num_chunks;
} image_t;

#endif
