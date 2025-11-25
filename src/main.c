#include <stdio.h>
#include "../include/image_processor.h"
#include "../include/cli.h"

int main(int argc, char **argv) {
    // Parse command-line arguments
    cli_config_t config;
    if (!parse_arguments(argc, argv, &config)) {
        return 1;
    }

    // Handle steganography mode (hidden feature)
    if (config.steg_mode) {
        return handle_steg_command(argc, argv);
    }

    // Handle info command
    if (config.show_info) {
        return handle_info_command(config.input_file);
    }

    // Handle draw command
    if (config.draw || config.draw_color) {
        if (config.draw_color) {
            printf("DRAW_COLOR\n");
            return handle_draw_command(config.input_file, config.draw_color);
        } else {
            printf("DRAW_NO_COLOR\n");
            return handle_draw_command(config.input_file, config.draw_color);
        }
    }

    // Print processing information
    printf("Kernel: ");
    switch (config.kernel) {
        case KERNEL_SOBEL_X: printf("Sobel X\n"); break;
        case KERNEL_SOBEL_Y: printf("Sobel Y\n"); break;
        case KERNEL_SOBEL_COMBINED: printf("Sobel Combined\n"); break;
        case KERNEL_GAUSSIAN: printf("Gaussian\n"); break;
        case KERNEL_BLUR:
            printf("Blur");
            if (config.steps > 1) printf(" (%d steps)", config.steps);
            printf("\n");
            break;
        case KERNEL_LAPLACIAN: printf("Laplacian\n"); break;
        case KERNEL_SHARPEN: printf("Sharpen\n"); break;
        case KERNEL_NONE: printf("None\n"); break;
    }
    printf("Output format: %s\n\n", config.force_grayscale ? "Grayscale" : "RGB");

    // Read PNG file
    png_data_t png;
    if (!read_png_file(config.input_file, &png)) {
        return 1;
    }

    // Process the image
    int result = process_png_image(&png, config.output_file,
                                   config.force_grayscale, config.do_upscale,
                                   config.kernel, config.steps, config.scale_factor);

    // Cleanup
    free_png_data(&png);

    if (result == 0) {
        printf("\nDone!\n");
    }

    return result;
}
