#ifndef PNG_IO
#define PNG_IO

#include "include.h"

#define PNG_SIG_SIZE 8

void read_bytes(FILE *file, void *buffer, size_t buffer_size);
void write_bytes(FILE *file, const void *buffer, size_t buffer_size);
uint32_t read_chunk_size(FILE *file);
void read_chunk_type(FILE *file, uint8_t chunk_type[]);
uint32_t read_chunk_crc(FILE *file);
void write_chunk_size(FILE *file, uint32_t chunk_size);
void write_chunk_type(FILE *file, uint8_t chunk_type[]);
void write_chunk_crc(FILE *file, const uint8_t *type, uint8_t *data, uint32_t length);

#endif
