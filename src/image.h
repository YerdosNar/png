#ifndef IMAGE_H
#define IMAGE_H

#include "include.h"

uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width);
void free_pixel_matrix(uint8_t **matrix, uint32_t height);

#endif
