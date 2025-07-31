#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>

uint64_t crc(uint8_t *buffer, int len);
void reverse_bytes(void *buffer, size_t buffer_size);
uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width);
void free_pixel_matrix(uint8_t **matrix, uint32_t height);

#endif
