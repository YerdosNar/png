#include "image.h"

uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width) {
    uint8_t **matrix = malloc(height * sizeof(uint8_t*));
    if(!matrix) {
        fprintf(stderr, "ERROR: Could not allocate memory for pixel matrix\n");
        exit(1);
    }
    for(uint32_t i = 0; i < height; i++) {
        matrix[i] = malloc(width * sizeof(uint32_t));
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
