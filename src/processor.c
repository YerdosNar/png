#include "../include/processor.h"

// source: https://www.libpng.org/pub/png/spec/1.2/PNG-Filters.html
uint8_t paeth_predictor(uint8_t left, uint8_t up, uint8_t up_left) {
    int p = up + left - up_left;
    int p_left = abs(p-left);
    int p_up = abs(p-up);
    int p_up_left = abs(p-up_left);

    if(p_left <= p_up && p_left < p_up_left) {
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

        case FILTER_AVG:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bit_ppx) ? current[i-bit_ppx] : 0;
                uint8_t up = previous ? previous[i] : 0;
                current[i] += (left+up) / 2;
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
    // Bytes per pixel init
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
            fprintf(stderr, "ERROR: Paletter currently not supported\n");
            return NULL;

        case 4: // Gray+ALPHA
            channels = 2;
            bpp = 2;
            break;

        case 6: // RGB+ALPHA
            channels = 4;
            bpp = 4;
            break;
    }

    uint32_t height = ihdr->height;
    uint32_t width = ihdr->width;

    // decompress IDAT
    uint64_t expected_size = height * (1 + width * bpp);
    uint8_t *decompressed = malloc(expected_size); // FLAG => FREE IT
    if(!decompressed) {
        fprintf(stderr, "ERROR: Could not allocate memory for decompression\n");
        return NULL;
    }

    int32_t result = uncompress(decompressed, &expected_size, idat_data, idat_size);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to uncompress image data: %d\n", result);
        free(decompressed);
        return NULL;
    }

    image_t *image = malloc(sizeof(image_t));
    image->width = width;
    image->height = height;
    image->channels = channels;
    image->pixels = allocate_pixel_matrix(height, width * channels);

    // process scanlines
    uint32_t scanline_length = width * bpp;
    uint8_t *previous_scanline = NULL;
    for(uint32_t y = 0; y < height; y++) {
        uint32_t offset = y * (1 + scanline_length);
        uint8_t filter_type = decompressed[offset];
        uint8_t *scanline = &decompressed[offset+1];

        // unfilter
        unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);

        // copy to pixel matrix
        memcpy(image->pixels[y], scanline, scanline_length);
        previous_scanline = scanline;
    }

    free(decompressed);
    return image;
}

uint8_t **rgb_to_grayscale(image_t *image) {
    if(image->channels == 1)  // if it Grayscale = do nothing
    {
        return image->pixels;
    }

    uint8_t **gray = allocate_pixel_matrix(image->height, image->width);
    for(uint32_t y = 0; y < image->height; y++) {
        for(uint32_t x = 0; x < image->width; x++) {
            if(image->channels >= 3) {
                // RGB to Grayscale using std weights
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                gray[y][x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b); //magic numbers: -> https://en.wikipedia.org/wiki/Luma_(video)#Rec._601_luma_versus_Rec._709_luma_coefficients
            } else if(image->channels == 2) {
                // gray + ALPHA
                // then extract only gray vlaue
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }

    return gray;
}

/* CONVOLUTION
 * sobel = edge detection
 * laplacian = edge detecetion
 * gaussian = blurring
 * blur
 * sharpen
 */
void apply_convolution(uint8_t **input, uint8_t **output, uint32_t height, uint32_t width, uint8_t kernel_type) {
    float kernels[7][3][3] = {
        { // Sobel x
            {-1, 0, 1},
            {-2, 0, 2},
            {-1, 0, 1}
        },
        { // Sobel y
            {-1, -2, -1},
            {0, 0, 0},
            {1, 2, 1}
        }, // Sobel combined
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        },
        { // Gaussian blur
            {1/16.0, 2/16.0, 1/16.0},
            {2/16.0, 4/16.0, 2/16.0},
            {1/16.0, 2/16.0, 1/16.0}
     },
        { // Blur
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0}
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

    //apply convolution
    if(kernel_type == KERNEL_SOBEL_COMBINED) {
        for(uint32_t y = 1; y < height-1; y++) {
            for(uint32_t x = 1; x < width-1; x++) {
                int gx = 0, gy = 0;

                //convolute with kernels
                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y+ky][x+kx];
                        gx += pixel * kernels[KERNEL_SOBEL_X][ky+1][kx+1];
                        gy += pixel * kernels[KERNEL_SOBEL_Y][ky+1][kx+1];
                    }
                }

                //calculate magnitue 
                int magnitude = (int)sqrt(gx * gx + gy * gy);
                output[y][x] = (magnitude > 0xff) ? 0xff : magnitude;
            }
        }
    } else if(kernel_type < KERNEL_NONE) {
        for(uint32_t y = 1; y < height-1; y++) {
            for(uint32_t x = 1; x < width-1; x++) {
                float sum = 0.0f;

                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        sum += pixel * kernels[kernel_type][ky + 1][kx + 1];
                    }
                }

                //clamp to valid to range
                if(sum < 0) {
                    sum = 0;
                }
                if(sum > 255) {
                    sum = 255;
                }
                output[y][x] = (uint8_t)sum;
            }
        }
    } else {
        for(uint32_t y = 0; y < height; y++) {
            memcpy(output[y], input[y], width);
        }
        return;
    }
    
    // handle borders (set to 0)
    for(uint32_t i = 0; i < width; i++) {
        output[0][i] = input[0][i];
        output[height-1][i] = input[height-1][i];
    }
    for(uint32_t i = 0; i < height; i++) {
        output[i][0] = input[i][0];
        output[i][width-1] = input[i][width-1];
    }
}
