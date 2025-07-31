#ifndef PNG_IO
#define PNG_IO

#include "image.h"
#include <stdint.h>
#include <stdio.h>

#define PNG_SIG_SIZE 8
const uint8_t png_signature[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

void read_bytes(FILE *file, void *buffer, size_t buffer_size);
void write_bytes(FILE *file, const void *buffer, size_t buffer_size);
uint32_t read_chunk_size(FILE *file);
void read_chunk_type(FILE *file, uint8_t chunk_type[]);
uint32_t read_chunk_crc(FILE *file);
void write_chunk(FILE *file, const char *type, uint8_t *data, uint32_t length);
void save_png(const char *filename, uint8_t **pixels, ihdr_t ihdr, uint32_t channels); 
void print_bytes(uint8_t *buffer, size_t buffer_size);
void print_info(FILE *file);

#endif
