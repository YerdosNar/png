#include "../include/processor.h"

// source: https://www.libpng.org/pub/png/spec/1.2/PNG-Filters.html
uint8_t paeth_predictor(uint8_t left, uint8_t up, uint8_t up_left) {
    int p = up + left - up_left;
    int p_left = abs(p-left);
    int p_up = abs(p-up);
    int p_up_left = abs(p-up_left);

    if(p_left <= p_up && p_left <= p_up_left) {
        return left;
    } else if(p_up <= p_up_left) {
        return up;
    } else {
        return up_left;
    }
}

void unfilter_scanline(uint8_t *current, uint8_t *previous, uint32_t length, uint32_t bit_ppx, uint8_t filter_type) {
    switch(filter_type) {
        case FILTER_NONE:
            break;

        case FILTER_SUB:
            for(uint32_t i = bit_ppx; i < length; i++) {
                current[i] += current[i-bit_ppx];
            }
            break;

        case FILTER_UP:
            if(previous) {
                for(uint32_t i = 0; i < length; i++) {
                    current[i] += previous[i];
                }
            }
            break;

        case FILTER_AVG:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bit_ppx) ? current[i-bit_ppx] : 0;
                uint8_t up = previous ? previous[i] : 0;
                current[i] += (left + up) / 2;
            }
            break;

        case FILTER_PAETH:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bit_ppx) ? current[i-bit_ppx] : 0;
                uint8_t up = previous ? previous[i] : 0;
                uint8_t up_left = (previous && i >= bit_ppx) ? previous[i-bit_ppx] : 0;
                current[i] += paeth_predictor(left, up, up_left);
            }
            break;
    }
}

image_t *process_idat_chunks(ihdr_t *ihdr, uint8_t *idat_data, uint32_t idat_size) {
    // Validate input parameters
    if(!ihdr || !idat_data || idat_size == 0) {
        fprintf(stderr, "ERROR: Invalid input parameters\n");
        return NULL;
    }

    // Initialize bytes per pixel and channels
    uint32_t bpp = 1;
    uint32_t channels = 1;

    switch(ihdr->color_type) {
        case 0: // Grayscale
            channels = 1;
            bpp = 1;
            break;

        case 2: // RGB
            channels = 3;
            bpp = 3;
            break;

        case 3: // Palette
            fprintf(stderr, "ERROR: Palette color type currently not supported\n");
            return NULL;

        case 4: // Gray+ALPHA
            channels = 2;
            bpp = 2;
            break;

        case 6: // RGB+ALPHA
            channels = 4;
            bpp = 4;
            break;

        default:
            fprintf(stderr, "ERROR: Unknown color type: %u\n", ihdr->color_type);
            return NULL;
    }

    uint32_t height = ihdr->height;
    uint32_t width = ihdr->width;

    // Validate image dimensions
    if(width == 0 || height == 0) {
        fprintf(stderr, "ERROR: Invalid image dimensions: %u x %u\n", width, height);
        return NULL;
    }

    // Calculate expected decompressed size
    uLongf expected_size = height * (1 + width * bpp);
    uint8_t *decompressed = malloc(expected_size);
    if(!decompressed) {
        fprintf(stderr, "ERROR: Could not allocate memory for decompression (%lu bytes)\n", expected_size);
        return NULL;
    }

    // Decompress IDAT data
    int result = uncompress(decompressed, &expected_size, idat_data, idat_size);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to uncompress image data: %d\n", result);
        free(decompressed);
        return NULL;
    }

    // Create image structure
    image_t *image = malloc(sizeof(image_t));
    if(!image) {
        fprintf(stderr, "ERROR: Could not allocate memory for image structure\n");
        free(decompressed);
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->pixels = allocate_pixel_matrix(height, width * channels);

    if(!image->pixels) {
        fprintf(stderr, "ERROR: Could not allocate pixel matrix\n");
        free(decompressed);
        free(image);
        return NULL;
    }

    // Process scanlines
    uint32_t scanline_length = width * bpp;
    uint8_t *previous_scanline = NULL;
    
    for(uint32_t y = 0; y < height; y++) {
        uint32_t offset = y * (1 + scanline_length);
        
        // Bounds check
        if(offset + scanline_length >= expected_size) {
            fprintf(stderr, "ERROR: Scanline data exceeds buffer bounds at row %u\n", y);
            free_pixel_matrix(image->pixels, height);
            free(image);
            free(decompressed);
            return NULL;
        }
        
        uint8_t filter_type = decompressed[offset];
        uint8_t *scanline = &decompressed[offset + 1];

        // Validate filter type
        if(filter_type > FILTER_PAETH) {
            fprintf(stderr, "ERROR: Invalid filter type %u at row %u\n", filter_type, y);
            free_pixel_matrix(image->pixels, height);
            free(image);
            free(decompressed);
            return NULL;
        }

        // Apply unfiltering
        unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);

        // Copy to pixel matrix
        memcpy(image->pixels[y], scanline, scanline_length);
        previous_scanline = scanline;
    }

    free(decompressed);
    return image;
}

uint8_t **rgb_to_grayscale(image_t *image) {
    if(!image || !image->pixels) {
        fprintf(stderr, "ERROR: Invalid image for grayscale conversion\n");
        return NULL;
    }

    if(image->channels == 1) {
        // Already grayscale
        return image->pixels;
    }

    uint8_t **gray = allocate_pixel_matrix(image->height, image->width);
    if(!gray) {
        return NULL;
    }

    for(uint32_t y = 0; y < image->height; y++) {
        for(uint32_t x = 0; x < image->width; x++) {
            if(image->channels >= 3) {
                // RGB to Grayscale using standard weights
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                gray[y][x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            } else if(image->channels == 2) {
                // Gray + Alpha - extract only gray value
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }

    return gray;
}

void apply_convolution(uint8_t **input, uint8_t **output, uint32_t height, uint32_t width, uint8_t kernel_type) {
    if(!input || !output || height < 3 || width < 3) {
        fprintf(stderr, "ERROR: Invalid parameters for convolution\n");
        return;
    }

    // Define convolution kernels
    float kernels[7][3][3] = {
        { // Sobel X
            {-1, 0, 1},
            {-2, 0, 2},
            {-1, 0, 1}
        },
        { // Sobel Y
            {-1, -2, -1},
            {0, 0, 0},
            {1, 2, 1}
        },
        { // Sobel combined (placeholder - handled specially)
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        },
        { // Gaussian blur
            {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f},
            {2.0f/16.0f, 4.0f/16.0f, 2.0f/16.0f},
            {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f}
        },
        { // Box blur
            {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f},
            {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f},
            {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f}
        },
        { // Laplacian edge detection
            {0, -1, 0},
            {-1, 4, -1},
            {0, -1, 0}
        },
        { // Sharpen
            {0, -1, 0},
            {-1, 5, -1},
            {0, -1, 0}
        }
    };

    if(kernel_type == KERNEL_SOBEL_COMBINED) {
        // Special handling for combined Sobel
        for(uint32_t y = 1; y < height - 1; y++) {
            for(uint32_t x = 1; x < width - 1; x++) {
                float gx = 0.0f, gy = 0.0f;

                // Apply both Sobel kernels
                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        gx += pixel * kernels[KERNEL_SOBEL_X][ky + 1][kx + 1];
                        gy += pixel * kernels[KERNEL_SOBEL_Y][ky + 1][kx + 1];
                    }
                }

                // Calculate magnitude
                float magnitude = sqrtf(gx * gx + gy * gy);
                output[y][x] = (magnitude > 255.0f) ? 255 : (uint8_t)magnitude;
            }
        }
    } else if(kernel_type < KERNEL_NONE) {
        // Apply single kernel
        for(uint32_t y = 1; y < height - 1; y++) {
            for(uint32_t x = 1; x < width - 1; x++) {
                float sum = 0.0f;

                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        sum += pixel * kernels[kernel_type][ky + 1][kx + 1];
                    }
                }

                // Clamp to valid range
                if(sum < 0.0f) sum = 0.0f;
                if(sum > 255.0f) sum = 255.0f;
                output[y][x] = (uint8_t)sum;
            }
        }
    } else {
        // No kernel - copy input to output
        for(uint32_t y = 0; y < height; y++) {
            memcpy(output[y], input[y], width);
        }
        return;
    }
    
    // Handle borders by copying from input
    for(uint32_t x = 0; x < width; x++) {
        output[0][x] = input[0][x];                // Top row
        output[height - 1][x] = input[height - 1][x]; // Bottom row
    }
    for(uint32_t y = 0; y < height; y++) {
        output[y][0] = input[y][0];                // Left column
        output[y][width - 1] = input[y][width - 1]; // Right column
    }
}
