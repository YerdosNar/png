#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t crc(uint8_t *buffer, int len);

uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width);
void free_pixel_matrix(uint8_t **matrix, uint32_t height);
void reverse(void *buffer, size_t size);

#endif
