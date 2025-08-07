#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>

void reverse(void *buffer, size_t buffer_size);
uint32_t crc(uint8_t *buf, int32_t len);

#endif
