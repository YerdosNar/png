#include "../include/utils.h"

static uint64_t crc_table[256];
static int crc_table_computed = 0;

static void make_crc_table(void) {
    uint64_t c;
    int n, k;

    for(n = 0; n < 256; n++) {
        c = (uint64_t) n;
        for(k = 0; k < 8; k++) {
            if(c & 1) {
                c = 0xEDB88320L ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

static uint64_t update_crc(uint64_t crc, uint8_t *buf, int len) {
    uint64_t c = crc;
    int n;
    if(!crc_table_computed) {
        make_crc_table();
    }
    for(n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

uint64_t crc(uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void reverse_bytes(void *buffer, size_t buffer_size) {
    uint8_t *temp = buffer;
    for(size_t i = 0; i < buffer_size/2; ++i) {
        uint8_t t = temp[i];
        temp[i] = temp[buffer_size - i - 1];
        temp[buffer_size - i - 1] = t;
    }
}

uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width) {
     uint8_t **matrix = malloc(height * sizeof(uint8_t*));
    if(!matrix) {
        fprintf(stderr, "ERROR: Could not allocate memory for pixel matrix\n");
        exit(1);
    }
    for(uint32_t i = 0; i < height; i++) {
        matrix[i] = malloc(width * sizeof(uint8_t));
        if(!matrix[i]) {
            fprintf(stderr, "ERROR: Could not allocate memory for %u row in pixel matrix\n", i);
            exit(1);
        }
    }
    return matrix;
}

void free_pixel_matrix(uint8_t **matrix, uint32_t height) {
    for(uint32_t i = 0; i < height; i++) {
        free(matrix[i]);
    }
    free(matrix);
}
