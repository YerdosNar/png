#include "../include/processor.h"
#include <math.h> // Required for sqrtf and fabsf

// source: https://www.libpng.org/pub/png/spec/1.2/PNG-Filters.html
uint8_t paeth_predictor(uint8_t left, uint8_t up, uint8_t up_left) {
    // Using int for intermediate calculations to prevent overflow
    int p = (int)left + (int)up - (int)up_left;
    int p_left = abs(p - left);
    int p_up = abs(p - up);
    int p_up_left = abs(p - up_left);

    if (p_left <= p_up && p_left <= p_up_left) {
        return left;
    } else if (p_up <= p_up_left) {
        return up;
    } else {
        return up_left;
    }
}

void unfilter_scanline(uint8_t *current, const uint8_t *previous, uint32_t length, uint32_t bpp, uint8_t filter_type) {
    // NOTE: bpp is bytes per pixel, not bits.
    // It's the offset to the corresponding pixel on the left.
    switch (filter_type) {
        case FILTER_NONE:
            break;

        case FILTER_SUB:
            for (uint32_t i = bpp; i < length; i++) {
                // Recon(x) = Filt(x) + Recon(a)
                current[i] += current[i - bpp];
            }
            break;

        case FILTER_UP:
            if (previous) {
                for (uint32_t i = 0; i < length; i++) {
                    // Recon(x) = Filt(x) + Recon(b)
                    current[i] += previous[i];
                }
            }
            break;

        case FILTER_AVG:
            for (uint32_t i = 0; i < length; i++) {
                // FIX: Use reconstructed values for 'left' and 'up'.
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                // Recon(x) = Filt(x) + floor((Recon(a) + Recon(b)) / 2)
                current[i] += (left + up) / 2;
            }
            break;

        case FILTER_PAETH:
            for (uint32_t i = 0; i < length; i++) {
                // FIX: Use reconstructed values for 'left', 'up', and 'up_left'.
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                uint8_t up_left = (previous && i >= bpp) ? previous[i - bpp] : 0;
                // Recon(x) = Filt(x) + PaethPredictor(Recon(a), Recon(b), Recon(c))
                current[i] += paeth_predictor(left, up, up_left);
            }
            break;
    }
}

image_t *process_idat_chunks(ihdr_t *ihdr, palette_t *palette, uint8_t *idat_data, uint64_t idat_size) {
    if (!ihdr || !idat_data || idat_size == 0) {
        fprintf(stderr, "ERROR: Invalid input parameters to process_idat_chunks\n");
        return NULL;
    }

    uint32_t channels;
    switch (ihdr->color_type) {
        case 0: channels = 1; break; // Grayscale
        case 2: channels = 3; break; // RGB
        case 4: channels = 2; break; // Grayscale + Alpha
        case 6: channels = 4; break; // RGB + Alpha
        case 3: // Palette
            if(!palette || !palette->entries) {
                fprintf(stderr, "ERROR: Palette (PLTE) chunk missing for color type 3.\n");
                return NULL;
            }
            channels = (palette->alphas) ? 4 : 3;
            break;
        default:
            fprintf(stderr, "ERROR: Unknown color type: %u\n", ihdr->color_type);
            return NULL;
    }

    // Bytes per pixel. Assumes bit depth is 8, which is true for this project.
    uint32_t bpp = (ihdr->color_type == 3) ? 1 : channels;

    // Calculate decompressed data size. Each row is preceded by 1 filter-type byte.
    uLongf decompressed_size = (uLongf)ihdr->height * (1 + (uLongf)ihdr->width * bpp);
    uint8_t *decompressed = malloc(decompressed_size);
    if (!decompressed) {
        fprintf(stderr, "ERROR: Could not allocate memory for decompression.\n");
        return NULL;
    }

    int result = uncompress(decompressed, &decompressed_size, idat_data, (uLong)idat_size);
    if (result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to uncompress IDAT data (zlib error: %d)\n", result);
        free(decompressed);
        return NULL;
    }

    image_t *image = malloc(sizeof(image_t));
    if (!image) {
        fprintf(stderr, "ERROR: Could not allocate memory for image structure.\n");
        free(decompressed);
        return NULL;
    }
    image->width = ihdr->width;
    image->height = ihdr->height;
    image->channels = channels;
    image->pixels = allocate_pixel_matrix(ihdr->height, ihdr->width * channels);

    uint32_t scanline_length = ihdr->width * bpp;
    const uint8_t *previous_scanline = NULL;

    if(ihdr->color_type == 3) {
        uint8_t *unfiltered_indices = malloc(ihdr->height * ihdr->width);
        if(!unfiltered_indices) {
            fprintf(stderr, "ERROR: Could not allocate for palette indices.\n");
            free(unfiltered_indices);
            return NULL;
        }

        for(uint32_t y = 0; y < ihdr->height; y++) {
            uint64_t offset = y * (1 + scanline_length);
            uint8_t filter_type = decompressed[offset];
            uint8_t *scanline = &decompressed[offset + 1];

            unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);

            memcpy(unfiltered_indices + (y * ihdr->width), scanline, scanline_length);
            previous_scanline = unfiltered_indices + (y * ihdr->width);
        }

        for(uint32_t y = 0; y < ihdr->height; y++) {
            for(uint32_t x = 0; x < ihdr->width; x++) {
                uint8_t index = unfiltered_indices[y * ihdr->width + x];
                if(index >= palette->entry_count) {
                    fprintf(stderr, "ERROR: Invalid palette index %u at (%u, %u)\n", index, y, x);
                    index = 0;
                }

                rgb_t color = palette->entries[index];
                image->pixels[y][x * channels + 0] = color.r;
                image->pixels[y][x * channels + 1] = color.g;
                image->pixels[y][x * channels + 2] = color.b;

                if(channels == 4) {
                    image->pixels[y][x * channels + 3] = (index < palette->alpha_count) ? palette->alphas[index] : 255;
                }
            }
        }
        free(unfiltered_indices);
    }
    else {
        for (uint32_t y = 0; y < ihdr->height; y++) {
            // Calculate the offset to the start of the current scanline in the decompressed buffer.
            uint64_t offset = y * (1 + scanline_length);
            uint8_t filter_type = decompressed[offset];
            uint8_t *scanline = &decompressed[offset + 1];

            if (filter_type > FILTER_PAETH) {
                fprintf(stderr, "ERROR: Invalid filter type %u at row %u\n", filter_type, y);
                free_pixel_matrix(image->pixels, image->height);
                free(image);
                free(decompressed);
                return NULL;
            }

            unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);

            memcpy(image->pixels[y], scanline, scanline_length);

            // FIX: The *unfiltered* current row becomes the previous row for the next iteration.
            previous_scanline = image->pixels[y];
        }
    }

    free(decompressed);
    return image;
}

uint8_t **rgb_to_grayscale(image_t *image) {
    if (!image || !image->pixels) {
        fprintf(stderr, "ERROR: Invalid image for grayscale conversion\n");
        return NULL;
    }

    if (image->channels == 1) {
        // Already grayscale, no conversion needed.
        return image->pixels;
    }

    uint8_t **gray = allocate_pixel_matrix(image->height, image->width);
    if (!gray) return NULL;

    for (uint32_t y = 0; y < image->height; y++) {
        for (uint32_t x = 0; x < image->width; x++) {
            if (image->channels >= 3) { // RGB or RGBA
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                // Luminosity conversion: gray = 0.299*R + 0.587*G + 0.114*B
                gray[y][x] = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
            } else if (image->channels == 2) { // Grayscale + Alpha
                // Just take the grayscale value, ignore alpha.
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }
    return gray;
}

void apply_convolution(uint8_t **input, uint8_t **output, uint32_t height, uint32_t width, kernel_type type) {
    if (!input || !output || height < 3 || width < 3) {
        fprintf(stderr, "ERROR: Invalid parameters for convolution\n");
        return;
    }

    // Using a simple "copy border" strategy. More advanced methods like
    // mirroring or extending exist but are more complex.
    for(uint32_t y = 0; y < height; y++) {
        memcpy(output[y], input[y], width);
    }

    if (type == KERNEL_NONE) {
        return; // Nothing to do if no kernel is selected
    }

    float kernels[7][3][3] = {
        [KERNEL_SOBEL_X]        = {{-1, 0, 1},
                                   {-2, 0, 2},
                                   {-1, 0, 1}},

        [KERNEL_SOBEL_Y]        = {{-1, -2, -1},
                                   {0, 0, 0},
                                   {1, 2, 1}},

        [KERNEL_SOBEL_COMBINED] = {{0}}, // Handled as a special case

        [KERNEL_GAUSSIAN]       = {{1/16.f, 2/16.f, 1/16.f},
                                   {2/16.f, 4/16.f, 2/16.f},
                                   {1/16.f, 2/16.f, 1/16.f}},

        [KERNEL_BLUR]           = {{1/9.f, 1/9.f, 1/9.f},
                                   {1/9.f, 1/9.f, 1/9.f},
                                   {1/9.f, 1/9.f, 1/9.f}},

        [KERNEL_LAPLACIAN]      = {{0, -1, 0},
                                   {-1, 4, -1},
                                   {0, -1, 0}},

        [KERNEL_SHARPEN]        = {{0, -1, 0},
                                   {-1, 5, -1},
                                   {0, -1, 0}}
    };

    // Iterate over the inner pixels, avoiding the 1-pixel border
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            float gx = 0.0f, gy = 0.0f;

            if (type == KERNEL_SOBEL_COMBINED) {
                 for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        gx += pixel * kernels[KERNEL_SOBEL_X][ky + 1][kx + 1];
                        gy += pixel * kernels[KERNEL_SOBEL_Y][ky + 1][kx + 1];
                    }
                }
                float magnitude = sqrtf(gx * gx + gy * gy);
                output[y][x] = (magnitude > 255.0f) ? 255 : (uint8_t)magnitude;
            } else {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        sum += input[y + ky][x + kx] * kernels[type][ky + 1][kx + 1];
                    }
                }

                // FIX: For Sobel X/Y, take the absolute value to see all edges.
                if (type == KERNEL_SOBEL_X || type == KERNEL_SOBEL_Y) {
                    sum = fabsf(sum);
                }

                // Clamp the result to the valid 0-255 range
                if (sum < 0.0f) sum = 0.0f;
                if (sum > 255.0f) sum = 255.0f;
                output[y][x] = (uint8_t)sum;
            }
        }
    }
}

// Just upscaling. Nothing more
uint8_t **upscale(uint8_t **input, uint32_t height, uint32_t width) {
    if(!input) {
        return NULL;
    }

    uint32_t new_h = height * 3;
    uint32_t new_w = width * 3;
    uint8_t **output = allocate_pixel_matrix(new_h, new_w);
    if(!output) {
        fprintf(stderr, "ERROR: Could not allocate memory for output in upscale()\n");
        return NULL;
    }

    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x++) {
            uint8_t pixel_value = input[y][x];
            for(uint8_t ky = 0; ky < 3; ky++) {
                for(uint8_t kx = 0; kx < 3; kx++) {
                    output[y * 3 + ky][x * 3 + kx] = pixel_value;
                }
            }
        }
    }
    // apply_convolution(output, output, new_h, new_w, KERNEL_SHARPEN);

    return output;
}

/**
 * Upscales a single-channel image by a factor of 3 using bilinear interpolation.
 *
 * @param input The input pixel matrix (grayscale).
 * @param height The height of the input image.
 * @param width The width of the input image.
 * @return A new, upscaled pixel matrix, or NULL on failure.
 */
uint8_t **bilinear_upscale(uint8_t **input, uint32_t height, uint32_t width, float scale_factor) {
    if (!input) {
        return NULL;
    }

    uint32_t new_height = height * (int)(scale_factor);
    uint32_t new_width = width * (int)(scale_factor);

    uint8_t **output = allocate_pixel_matrix(new_height, new_width);
    if (!output) {
        fprintf(stderr, "ERROR: Could not allocate memory for bilinear upscale output\n");
        return NULL;
    }

    for (uint32_t y_new = 0; y_new < new_height; y_new++) {
        for (uint32_t x_new = 0; x_new < new_width; x_new++) {
            // Map the new pixel's coordinates back to the original image
            float x_orig = (x_new + 0.5f) / scale_factor - 0.5f;
            float y_orig = (y_new + 0.5f) / scale_factor - 0.5f;

            // Get the integer coordinates of the top-left surrounding pixel
            int x1 = (int)floor(x_orig);
            int y1 = (int)floor(y_orig);

            // Handle edge cases by clamping coordinates
            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if ((uint32_t)x1 >= width - 1) x1 = width - 2;
            if ((uint32_t)y1 >= height - 1) y1 = height - 2;

            // Coordinates of the other 3 surrounding pixels
            int x2 = x1 + 1;
            int y2 = y1 + 1;

            // Get the pixel values of the four neighbors
            uint8_t Q11 = input[y1][x1]; // Top-left
            uint8_t Q21 = input[y1][x2]; // Top-right
            uint8_t Q12 = input[y2][x1]; // Bottom-left
            uint8_t Q22 = input[y2][x2]; // Bottom-right

            // Calculate the fractional distances (weights)
            float x_frac = x_orig - x1;
            float y_frac = y_orig - y1;

            // Interpolate horizontally
            float R1 = Q11 * (1.0f - x_frac) + Q21 * x_frac; // Top edge
            float R2 = Q12 * (1.0f - x_frac) + Q22 * x_frac; // Bottom edge

            // Interpolate vertically and clamp the final value
            float value = R1 * (1.0f - y_frac) + R2 * y_frac;
            if (value > 255.0f) value = 255.0f;
            if (value < 0.0f) value = 0.0f;

            output[y_new][x_new] = (uint8_t)value;
        }
    }

    return output;
}
