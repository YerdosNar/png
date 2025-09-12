#ifndef PNG_IO_H
#define PNG_IO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "utils.h"

#define PNG_SIG_SIZE 8
extern const uint8_t png_sig[PNG_SIG_SIZE];

typedef struct {
    uint8_t r, g, b;
} rgb_t;

typedef struct {
    rgb_t *entries;
    uint8_t *alphas;
    uint32_t entry_count;
    uint32_t alpha_count;
} palette_t;

void read_bytes(FILE *file, void *buffer, size_t size);
void write_bytes(FILE *file, const void *buffer, size_t size);

uint32_t read_chunk_size(FILE *file);
void write_chunk_size(FILE *file, uint32_t size);

void read_chunk_type(FILE *file, uint8_t type[]);
void write_chunk_type(FILE *file, const char type[]);

uint32_t read_chunk_crc(FILE *file);
void write_chunk_crc(FILE *file, uint32_t crc);

void write_chunk(FILE *file, const char type[], uint8_t *data, uint32_t length_le);

void save_png(const char *filename, uint8_t **pixels, uint32_t width, uint32_t height, uint8_t color_type, uint32_t channels);
void print_info(FILE *file);

#endif
