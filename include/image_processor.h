#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "processor.h"
#include "png_io.h"
#include "utils.h"

// Main processing function that orchestrates the entire workflow
int process_png_image(png_data_t *png, const char *output_file, 
                      bool force_grayscale, bool do_upscale, 
                      kernel_type kernel, uint8_t steps, float scale_factor);

// Process grayscale image with optional filter
void process_grayscale_image(image_t *image, const char *output_file,
                             kernel_type kernel, uint8_t steps);

// Process RGB/RGBA image with optional filter
void process_rgb_image(image_t *image, const char *output_file,
                       kernel_type kernel, uint8_t steps);

// Process image upscaling
void process_upscale_image(image_t *image, const char *output_file,
                          bool force_grayscale, float scale_factor);

#endif
