#include "../include/image_processor.h"
#include <math.h>

void process_grayscale_image(image_t *image, const char *output_file,
                             kernel_type kernel, uint8_t steps) {
    // Convert to grayscale if needed
    uint8_t **grayscale = rgb_to_grayscale(image);
    uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
    uint8_t **temp = NULL;

    // Apply convolution
    if (kernel != KERNEL_NONE) {
        printf("Applying filter");
        if (steps > 1) {
            printf(" (%d steps)", steps);
            temp = allocate_pixel_matrix(image->height, image->width);
        }
        printf("...\n");

        uint8_t **input = grayscale;
        uint8_t **output = processed;

        for (uint8_t i = 0; i < steps; i++) {
            output = ((steps - 1 - i) % 2 == 0) ? processed : temp;
            apply_convolution(input, output, image->height, image->width, kernel);
            input = output;
        }

        if (output != processed) {
            for (uint32_t y = 0; y < image->height; y++) {
                memcpy(processed[y], output[y], image->width);
            }
        }
    } else {
        for (uint32_t y = 0; y < image->height; y++) {
            memcpy(processed[y], grayscale[y], image->width);
        }
    }

    // Save grayscale image
    save_png(output_file, processed, image->width, image->height, 0, 1);

    // Cleanup
    if (grayscale != image->pixels) {
        free_pixel_matrix(grayscale, image->height);
    }
    free_pixel_matrix(processed, image->height);
    if (temp) {
        free_pixel_matrix(temp, image->height);
    }
}

void process_rgb_image(image_t *image, const char *output_file,
                       kernel_type kernel, uint8_t steps) {
    if (kernel == KERNEL_NONE) {
        // No kernel applied - just save original
        uint8_t color_type = (image->channels == 4) ? 6 : 2;
        save_png(output_file, image->pixels, image->width, image->height, color_type, image->channels);
        return;
    }

    // Process RGB/RGBA image with kernel
    if (image->channels >= 3) {
        uint8_t **processed = allocate_pixel_matrix(image->height, image->width * image->channels);

        // Initialize with original data
        for (uint32_t y = 0; y < image->height; y++) {
            memcpy(processed[y], image->pixels[y], image->width * image->channels);
        }

        printf("Applying filter");
        if (steps > 1) printf(" (%d steps)", steps);
        printf("...\n");

        // Apply kernel to each channel
        for (uint32_t ch = 0; ch < 3; ch++) {
            uint8_t **channel = allocate_pixel_matrix(image->height, image->width);
            uint8_t **proc_channel = allocate_pixel_matrix(image->height, image->width);
            uint8_t **temp_channel = NULL;

            if (steps > 1) {
                temp_channel = allocate_pixel_matrix(image->height, image->width);
            }

            // Extract channel
            for (uint32_t y = 0; y < image->height; y++) {
                for (uint32_t x = 0; x < image->width; x++) {
                    channel[y][x] = image->pixels[y][x * image->channels + ch];
                }
            }

            // Apply kernel
            uint8_t **input = channel;
            uint8_t **output = proc_channel;

            for (uint8_t i = 0; i < steps; i++) {
                apply_convolution(input, output, image->height, image->width, kernel);

                if (i < steps - 1) {
                    uint8_t **swap = input;
                    input = output;
                    output = (swap == channel && temp_channel) ? temp_channel : channel;
                }
            }

            // Copy processed channel back
            for (uint32_t y = 0; y < image->height; y++) {
                for (uint32_t x = 0; x < image->width; x++) {
                    processed[y][x * image->channels + ch] = output[y][x];
                }
            }

            free_pixel_matrix(channel, image->height);
            free_pixel_matrix(proc_channel, image->height);
            if (temp_channel) {
                free_pixel_matrix(temp_channel, image->height);
            }
        }

        // Copy alpha channel if present
        if (image->channels == 4) {
            for (uint32_t y = 0; y < image->height; y++) {
                for (uint32_t x = 0; x < image->width; x++) {
                    processed[y][x * image->channels + 3] = image->pixels[y][x * image->channels + 3];
                }
            }
        }

        // Save RGB/RGBA image
        uint8_t color_type = (image->channels == 4) ? 6 : 2;
        save_png(output_file, processed, image->width, image->height, color_type, image->channels);
        free_pixel_matrix(processed, image->height);
    } else {
        // Grayscale with kernel
        uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
        uint8_t **temp = NULL;

        // Initialize with original data
        for (uint32_t y = 0; y < image->height; y++) {
            memcpy(processed[y], image->pixels[y], image->width);
        }

        if (steps > 1) {
            temp = allocate_pixel_matrix(image->height, image->width);
        }

        uint8_t **input = processed;
        uint8_t **output = processed;

        for (uint8_t i = 0; i < steps; i++) {
            if (i > 0) {
                output = (input == processed) ? temp : processed;
            }
            apply_convolution(input, output, image->height, image->width, kernel);
            input = output;
        }

        // Ensure final result is in processed
        if (output != processed && temp) {
            for (uint32_t y = 0; y < image->height; y++) {
                memcpy(processed[y], output[y], image->width);
            }
        }

        save_png(output_file, processed, image->width, image->height, 0, 1);
        free_pixel_matrix(processed, image->height);
        if (temp) {
            free_pixel_matrix(temp, image->height);
        }
    }
}

void process_upscale_image(image_t *image, const char *output_file,
                          bool force_grayscale, float scale_factor) {
    printf("Upscaling image by a factor of %.2f...\n", scale_factor);
    
    // Calculate new dimensions using the scale_factor, rounding for accuracy
    uint32_t new_width = (uint32_t)roundf(image->width * scale_factor);
    uint32_t new_height = (uint32_t)roundf(image->height * scale_factor);

    if (force_grayscale || image->channels == 1) {
        uint8_t **grayscale = rgb_to_grayscale(image);
        uint8_t **upscaled_pixels = bilinear_upscale(grayscale, image->height, image->width, scale_factor);

        save_png(output_file, upscaled_pixels, new_width, new_height, 0, 1);

        if (grayscale != image->pixels) {
            free_pixel_matrix(grayscale, image->height);
        }
        free_pixel_matrix(upscaled_pixels, new_height);
    } else {
        // Handle color images
        uint8_t **processed = allocate_pixel_matrix(new_height, new_width * image->channels);

        // Upscale each color channel (R, G, B) separately
        for (uint32_t ch = 0; ch < 3; ch++) {
            uint8_t **channel = allocate_pixel_matrix(image->height, image->width);
            for (uint32_t y = 0; y < image->height; y++) {
                for (uint32_t x = 0; x < image->width; x++) {
                    channel[y][x] = image->pixels[y][x * image->channels + ch];
                }
            }

            uint8_t **upscaled_channel = bilinear_upscale(channel, image->height, image->width, scale_factor);

            // Recombine the upscaled channel into the final image
            for (uint32_t y = 0; y < new_height; y++) {
                for (uint32_t x = 0; x < new_width; x++) {
                    processed[y][x * image->channels + ch] = upscaled_channel[y][x];
                }
            }
            free_pixel_matrix(channel, image->height);
            free_pixel_matrix(upscaled_channel, new_height);
        }

        // Copy alpha channel if it exists (using nearest-neighbor for simplicity)
        if (image->channels == 4) {
            for (uint32_t y = 0; y < new_height; y++) {
                for (uint32_t x = 0; x < new_width; x++) {
                    uint32_t orig_y = (uint32_t)fmin(roundf(y / scale_factor), image->height - 1);
                    uint32_t orig_x = (uint32_t)fmin(roundf(x / scale_factor), image->width - 1);
                    processed[y][x * 4 + 3] = image->pixels[orig_y][orig_x * 4 + 3];
                }
            }
        }

        uint8_t color_type = (image->channels == 4) ? 6 : 2;
        save_png(output_file, processed, new_width, new_height, color_type, image->channels);
        free_pixel_matrix(processed, new_height);
    }
}

int process_png_image(png_data_t *png, const char *output_file,
                      bool force_grayscale, bool do_upscale,
                      kernel_type kernel, uint8_t steps, float scale_factor) {
    if (!png->idat_data || png->idat_size == 0) {
        fprintf(stderr, "ERROR: No IDAT chunks found or empty image data\n");
        return 1;
    }

    printf("\nProcessing image data...\n");
    image_t *image = process_idat_chunks(&png->ihdr, &png->palette, png->idat_data, png->idat_size);

    if (!image) {
        fprintf(stderr, "ERROR: Failed to process image data\n");
        return 1;
    }

    // Process based on mode
    if (do_upscale) {
        process_upscale_image(image, output_file, force_grayscale, scale_factor);
    } else if (force_grayscale || image->channels == 1) {
        process_grayscale_image(image, output_file, kernel, steps);
    } else {
        process_rgb_image(image, output_file, kernel, steps);
    }

    // Cleanup
    free_pixel_matrix(image->pixels, image->height);
    free(image);

    return 0;
}
