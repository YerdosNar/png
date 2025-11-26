#include "../include/cli.h"
#include "../include/png_io.h"
#include "../include/steganography.h"

void usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions: \n");
    printf("  -o,  --output <file>        Output filename (default=out.png)\n");
    printf("  -i,  --info <file>          Show information about PNG file\n");
    printf("  -g,  --grayscale            Convert to grayscale\n");
    printf("  -c,  --color                Keep RGB format (default)\n");
    printf("  -x,  --sobel-x              Apply Sobel X edge detection\n");
    printf("  -y,  --sobel-y              Apply Sobel Y edge detection\n");
    printf("  -s,  --sobel                Apply combined Sobel edge detection\n");
    printf("  --gaussian [steps]          Apply Gaussian blur (optional: number of iterations, default=1)\n");
    printf("  -b,  --blur [steps]         Apply box blur (optional: number of iterations, default=1)\n");
    printf("  -l,  --laplacian            Apply Laplacian edge detection\n");
    printf("  -sh, --sharpen              Apply sharpening filter\n");
    printf("  -u,  --upscale              Upscale the image\n");
    printf("  -d,  --draw [color]         Draw the input image in ASCII characters (default: color=true)\n");
    printf("  --none                      No filter (default)\n");
    printf("  -h, --help                  Show this HELP message\n");
    printf("\nExamples:\n");
    printf("  %s input.png -o edges.png --sobel --grayscale\n", exec_name);
    printf("  %s photo.png -o blurred.png --gaussian\n", exec_name);
    printf("  %s photo.png -o blurred.png --draw false\n", exec_name);

    printf("\n\n");
    printf("Author: YerdosNar github.com/YerdosNar/PNG.git\n");
}

bool parse_arguments(int argc, char **argv, cli_config_t *config) {
    // Initialize config with defaults
    config->input_file = NULL;
    config->output_file = NULL;
    config->force_grayscale = false;
    config->do_upscale = false;
    config->draw = false;
    config->draw_color = false;
    config->kernel = KERNEL_NONE;
    config->steps = 0;
    config->scale_factor = 0.0f;
    config->show_info = false;
    config->steg_mode = false;
    config->steg_operation = NULL;

    if (argc < 2) {
        usage(argv[0]);
        return false;
    }

    // Check for help flag first
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        usage(argv[0]);
        return false;
    }

    // Check for steganography mode (hidden feature)
    if (!strcmp(argv[1], "--steg")) {
        config->steg_mode = true;
        return true;  // Let handle_steg_command parse the rest
    }

    // Check for info mode
    if (!strcmp(argv[1], "-i") || !strcmp(argv[1], "--info")) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: Invalid number of arguments for --info flag\n");
            return false;
        }
        config->show_info = true;
        config->input_file = argv[2];
        if (strstr(config->input_file, ".png") == NULL) {
            fprintf(stderr, "ERROR: Input file not provided for --info\n");
            return false;
        }
        return true;
    }

    if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--draw")) {
        if (argc < 3) {
            fprintf(stderr, "ERROR: Invalid number of arguments for --draw flag\n");
            printf("Usage: %s -d/--draw [true/false] <input.png>\n", argv[0]);
            printf("       %s -d input.png\n", argv[0]);
            printf("       %s -d false input.png\n", argv[0]);
            return false;
        }
        if (!strcmp(argv[2], "false")) {
            printf("Set draw=true color=false\n");
            config->draw = true;
            config->draw_color = false;
            config->input_file = argv[3];
            if (strstr(config->input_file, ".png") == NULL) {
                fprintf(stderr, "ERROR: Input file not provided for --draw\n");
                return false;
            }
        } else if (!strcmp(argv[2], "true")) {
            config->draw = false;
            config->draw_color = true;
            config->input_file = argv[3];
            if (strstr(config->input_file, ".png") == NULL) {
                fprintf(stderr, "ERROR: Input file not provided for --draw\n");
                return false;
            }
        } else {
            config->input_file = argv[2];
            if (strstr(config->input_file, ".png") == NULL) {
                fprintf(stderr, "ERROR: Input file not provided for --draw\n");
                return false;
            }
            config->draw = false;
            config->draw_color = true;
        }
        return true;
    }

    bool conflict = false;
    bool conflict_kernel = false;

    // Parse remaining arguments
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if (i + 1 < argc) {
                config->output_file = argv[++i];
            } else {
                fprintf(stderr, "ERROR: -o requires an argument\n");
                return false;
            }
        } else if ((!strcmp(argv[i], "-g") || !strcmp(argv[i], "--grayscale"))) {
            if (!conflict) {
                config->force_grayscale = true;
                conflict = true;
            } else {
                fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                return false;
            }
        } else if (!strcmp(argv[i], "--rgb")) {
            if (!conflict) {
                config->force_grayscale = false;
                conflict = true;
            } else {
                fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-x") || !strcmp(argv[i], "--sobel-x")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_SOBEL_X;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-y") || !strcmp(argv[i], "--sobel-y")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_SOBEL_Y;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--sobel")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_SOBEL_COMBINED;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "--gaussian")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_GAUSSIAN;
                if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                    config->steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--blur")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_BLUR;
                if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                    config->steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--laplacian")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_LAPLACIAN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-sh") || !strcmp(argv[i], "--sharpen")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_SHARPEN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "--none")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->kernel = KERNEL_NONE;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return false;
            }
        } else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--upscale")) {
            if (!conflict_kernel) {
                conflict_kernel = true;
                config->do_upscale = true;
                config->kernel = KERNEL_NONE;
                if (i + 1 < argc && (argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')) {
                    config->scale_factor = strtof(argv[++i], NULL);
                    if (config->scale_factor <= 0.0f || config->scale_factor > 15.0f) {
                        fprintf(stderr, "ERROR: Invalid scale_factor...Must between [1.0 ~ 15.0]\n");
                        return false;
                    }
                }
            } else {
                fprintf(stderr, "ERROR: Upscale cannot be combined with other kernel.\n");
                return false;
            }
        } else if (strstr(argv[i], ".png") != NULL && config->input_file == NULL) {
            config->input_file = argv[i];
        }
    }

    // Validate input file
    if (!config->input_file) {
        fprintf(stderr, "ERROR: No input file specified\n");
        usage(argv[0]);
        return false;
    }

    // Set default output file
    if (!config->output_file) {
        printf("No output filename was set\n");
        printf("Default: out.png\n");
        config->output_file = "out.png";
    }

    // Default steps to 1 if kernel is specified but steps is 0
    if (config->steps == 0 && config->kernel != KERNEL_NONE) {
        config->steps = 1;
    }

    // Suggest grayscale for edge detection
    if ((config->kernel == KERNEL_SOBEL_X || config->kernel == KERNEL_SOBEL_Y ||
         config->kernel == KERNEL_SOBEL_COMBINED || config->kernel == KERNEL_LAPLACIAN) &&
        !config->force_grayscale) {
        printf("Note: Edge detection typically works better on grayscale images.\n");
        printf("Consider adding --grayscale flag.\n\n");
    }

    return true;
}

int handle_info_command(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "ERROR: Could not open file %s\n", filename);
        return 1;
    }
    print_info(file, (char *)filename);
    fclose(file);
    return 0;
}

int handle_draw_command(const char *filename, bool color) {
    draw_ascii(filename, color);
    return 0;
}

int handle_steg_command(int argc, char **argv) {
    // "--help" help for steg
    if (argc < 3 || (!strcmp(argv[2], "--help") || !strcmp(argv[2], "-h"))) {
        printf("Usage: %s --steg [options] <filename.png>\n", argv[0]);
        printf("\nOptions:\n");
        printf("  -f/--find                                Finds hidden injected chunks with its content\n");
        printf("  -i/--inject                              Injects a hidden chunk into the file\n");
        printf("  -d/--delete-chunk                        Delete a chunk by chunk name\n");
        printf("  -h/--help                 See this message\n");
        printf("\nExample: \n");
        printf("         %s --steg -f injected.png\n", argv[0]);
        return 0;
    }

    // "-f" finding hidden steganography
    if (!strcmp(argv[2], "--find") || !strcmp(argv[2], "-f")) {
        if (argc < 4) {
            fprintf(stderr, "ERROR: Filename is not provided!\n");
            return 1;
        }
        FILE *file = fopen(argv[3], "rb");
        if (!file) {
            fprintf(stderr, "ERROR: Could not open file %s\n", argv[3]);
            return 1;
        }
        detect(file);
        fclose(file);
        return 0;
    }

    // "-i" injecting a custom chunk
    if (!strcmp(argv[2], "-i") || !strcmp(argv[2], "--inject")) {
        if (argc < 4) {
            fprintf(stderr, "ERROR: Filename is not provided!\n");
            return 1;
        }
        FILE *file = fopen(argv[3], "rb+");
        if (!file) {
            fprintf(stderr, "ERROR: Could not open file %s\n", argv[3]);
            return 1;
        }
        printf("You chose to hide information...\n");
        printf("Name the chunk(start with lowercase): ");
        char type[6];
        fgets(type, sizeof(type), stdin);
        printf("You can hide up to 1KB(1023characters) message: ");
        char message[1024];
        fgets(message, sizeof(message), stdin);
        inject_chunk(file, type, message);
        fclose(file);
        return 0;
    }

    // "-d" deleting a chunk
    if (!strcmp(argv[2], "--delete-chunk") || !strcmp(argv[2], "-d")) {
        if (argc < 4) {
            fprintf(stderr, "ERROR: Filename is not provided!\n");
            return 1;
        }
        FILE *file = fopen(argv[3], "rb+");
        if (!file) {
            fprintf(stderr, "ERROR: Could not open file %s\n", argv[3]);
            return 1;
        }
        printf("You chose to delete a chunk...\n");
        printf("Enter the chunk's name: ");
        char type[6];
        fgets(type, sizeof(type), stdin);
        delete_chunk(file, type);
        fclose(file);
        return 0;
    }

    fprintf(stderr, "ERROR: Unknown steganography option\n");
    return 1;
}
