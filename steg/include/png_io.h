#ifndef PNG_IO_H
#define PNG_IO_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "image.h"
#include "utils.h"

void read_bytes(FILE *file, void *buffer, size_t buffer_size);
void write_bytes(FILE *file, void *buffer, size_t buffer_size);
void read_ihdr(FILE *file, ihdr_t *ihdr);
void write_ihdr(FILE *file, ihdr_t ihdr);
void write_chunk(FILE *file, chunk_t ch, uint32_t size_be);

#endif
