#include <stdio.h>
#include "../include/utils.h"
#include "../include/png_io.h"
#include "../include/processor.h"
#include "../include/steganography.h"

void usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions: \n");
    printf("  -o, --output <file>    Output filename (default=out.png)\n");
    printf("  -i, --info <file>      Show information about PNG file\n");
    printf("  -g, --grayscale        Convert to grayscale\n");
    printf("  -c, --color            Keep RGB format (default)\n");
    printf("  --sobel-x              Apply Sobel X edge detection\n");
    printf("  --sobel-y              Apply Sobel Y edge detection\n");
    printf("  --sobel                Apply combined Sobel edge detection\n");
    printf("  --gaussian [steps]     Apply Gaussian blur (optional: number of iterations, default=1)\n");
    printf("  --blur [steps]         Apply box blur (optional: number of iterations, default=1)\n");
    printf("  --laplacian            Apply Laplacian edge detection\n");
    printf("  --sharpen              Apply sharpening filter\n");
    printf("  --none                 No filter (default)\n");
    printf("  -h, --help             Show this HELP message\n");
    printf("\nExamples:\n");
    printf("  %s input.png -o edges.png --sobel --grayscale\n", exec_name);
    printf("  %s photo.png -o blurred.png --gaussian\n", exec_name);

    printf("\n\n");
    printf("Author: YerdosNar github.com/YerdosNar/PNG.git\n");
}

int main(int argc, char **argv) {
    if(argc < 2) {
        usage(argv[0]);
        return 1;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    bool force_grayscale = false;
    bool conflict = false;
    bool conflict_kernel = false;
    kernel_type kernel = KERNEL_NONE;
    uint8_t steps = 0;

    // Hidde option, not listed in the usage info
    if(!strcmp(argv[1], "--steg")) {
        // "--help" help for steg
        if(argc < 3 || (!strcmp(argv[2], "--help") || !strcmp(argv[2], "-h"))) {
            printf("Usage: %s --steg [options] <filename.png>\n", argv[0]);
            printf("\nOptions:\n");
            printf("  -f                        Finds a hidden injected chunks with its content\n");
            printf("  -i                        Injects a hidden chunk into the file\n");
            printf("  -d                        Delete a chunk by chunk name\n");
            printf("  -h/--help                 See this message\n");
            printf("Example: %s --steg -f injected.png\n", argv[0]);

            return 0;
        }
        // "-f" finding hidden steganography
        else if(!strcmp(argv[2], "--find") || !strcmp(argv[2], "-f")) {
            if(argc < 4) {
                fprintf(stderr, "ERROR: Filename is not provided!\n");
                return 1;
            }
            FILE *file = fopen(argv[3], "rb");
            if(!file) {
                fprintf(stderr, "ERROR: Could not open file %s\n", argv[3]);
                return 1;
            }
            detect(file);

            fclose(file);
            return 0;
        }
        // "-i" injecting a custom chunk
        else if (!strcmp(argv[2], "-i") || !strcmp(argv[2], "--inject")) {
            if(argc < 4) {
                fprintf(stderr, "ERROR: Filename is not provided!\n");
                return 1;
            }
            FILE *file = fopen(argv[3], "rb+");
            if(!file) {
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
        // -d
        else if(!strcmp(argv[2], "--delete-chunk")|| !strcmp(argv[2], "-d")) {
            if(argc < 4) {
                fprintf(stderr, "ERROR: Filename is not provided!\n");
                return 1;
            }
            FILE *file = fopen(argv[3], "rb+");
            printf("You chose to delete a chunk...\n");
            printf("Enter the chunk's name: ");
            char type[6];
            fgets(type, sizeof(type), stdin);
            delete_chunk(file, type);

            fclose(file);
            return 0;
        }
    }
    else {
        for(int i = 1; i < argc; i++) {
            if((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) && i == 1) {
                usage(argv[0]);
                return 0;
            } else if(!strcmp(argv[i], "-i") || !strcmp(argv[i], "--info")) {
                if(argc < 3) {
                    fprintf(stderr, "ERROR: Invalid number of arguments for --info flag\n");
                    return 1;
                }

                if(!input_file) {
                    input_file = argv[2];
                    if(strstr(input_file, ".png") == NULL) {
                        fprintf(stderr, "ERROR: Input file not provided for --info\n");
                        return 1;
                    }
                }

                FILE *file = fopen(input_file, "rb");
                if(!file) {
                    fprintf(stderr, "ERROR: Could not open file %s\n", input_file);
                    return 1;
                }
                print_info(file);
                fclose(file);
                return 0;
            } else if(!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
                if(i + 1 < argc) {
                    output_file = argv[++i];
                } else {
                    fprintf(stderr, "ERROR: -o requires an argument\n");
                    return 1;
                }
            } else if((!strcmp(argv[i], "-g") || !strcmp(argv[i], "--grayscale")) && !conflict) {
                if(!conflict) {
                    force_grayscale = true;
                    conflict = true;
                } else {
                    fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--rgb")) {
                if(!conflict) {
                    force_grayscale = false;
                    conflict = true;
                } else {
                    fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--sobel-x")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_SOBEL_X;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--sobel-y")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_SOBEL_Y;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--sobel")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_SOBEL_COMBINED;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--gaussian")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_GAUSSIAN;
                    if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                        steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                    }
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--blur")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_BLUR;
                    if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                        steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                    }
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--laplacian")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_LAPLACIAN;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--sharpen")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_SHARPEN;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(!strcmp(argv[i], "--none")) {
                if(!conflict_kernel) {
                    conflict_kernel = true;
                    kernel = KERNEL_NONE;
                } else {
                    fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                    return 1;
                }
            } else if(strstr(argv[i], ".png") != NULL && input_file == NULL) {
                input_file = argv[i];
            }
        }
    }

    // Validate args
    if(!input_file) {
        fprintf(stderr, "ERROR: No input file specified\n");
        usage(argv[0]);
        return 1;
    }
    if(!output_file) {
        printf("No output filename was set\n");
        printf("Default: out.png\n");
        output_file = "out.png";
    }

    // Default steps to 1 if kernel is specified but steps is 0
    if(steps == 0 && kernel != KERNEL_NONE) {
        steps = 1;
    }

    // Edge detection works better on grayscale
    if((kernel == KERNEL_SOBEL_X || kernel == KERNEL_SOBEL_Y ||
        kernel == KERNEL_SOBEL_COMBINED || kernel == KERNEL_LAPLACIAN) && !force_grayscale) {
        printf("Note: Edge detection typically works better on grayscale images.\n");
        printf("Consider adding --grayscale flag.\n\n");
    }

    FILE *input_fp = fopen(input_file, "rb");
    if(!input_fp) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", input_file);
        return 1;
    }

    // Check PNG signature
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_fp, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file);
        fclose(input_fp);
        return 1;
    }

    printf("Processing: %s\n", input_file);
    printf("Kernel: ");
    switch(kernel) {
        case KERNEL_SOBEL_X: printf("Sobel X\n"); break;
        case KERNEL_SOBEL_Y: printf("Sobel Y\n"); break;
        case KERNEL_SOBEL_COMBINED: printf("Sobel Combined\n"); break;
        case KERNEL_GAUSSIAN: printf("Gaussian\n"); break;
        case KERNEL_BLUR: printf("Blur");
            if(steps > 1) printf(" (%d steps)", steps);
            printf("\n");
            break;
        case KERNEL_LAPLACIAN: printf("Laplacian\n"); break;
        case KERNEL_SHARPEN: printf("Sharpen\n"); break;
        case KERNEL_NONE: printf("None\n"); break;
    }
    printf("Output format: %s\n\n", force_grayscale ? "Grayscale" : "RGB");

    ihdr_t ihdr;
    uint8_t *idat_data = NULL;
    uint64_t idat_size = 0;
    uint64_t idat_capacity = 0;
    bool quit = false;

    while(!quit) {
        uint32_t chunk_size = read_chunk_size(input_fp);
        uint8_t chunk_type[4];
        read_chunk_type(input_fp, chunk_type);

        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            read_bytes(input_fp, &ihdr.width, 4);
            read_bytes(input_fp, &ihdr.height, 4);
            read_bytes(input_fp, &ihdr.bit_depth, 1);
            read_bytes(input_fp, &ihdr.color_type, 1);
            read_bytes(input_fp, &ihdr.compression, 1);
            read_bytes(input_fp, &ihdr.filter, 1);
            read_bytes(input_fp, &ihdr.interlace, 1);

            reverse(&ihdr.width, sizeof(ihdr.width));
            reverse(&ihdr.height, sizeof(ihdr.height));

            printf("Image dimensions: %u x %u\n", ihdr.width, ihdr.height);
            printf("Bit depth: %u, Color type: %u\n", ihdr.bit_depth, ihdr.color_type);
        } else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            if(idat_capacity < idat_size + chunk_size) {
                idat_capacity = (idat_size + chunk_size) * 2;
                uint8_t *new_idat_data = realloc(idat_data, idat_capacity);
                if (!new_idat_data) {
                    fprintf(stderr, "ERROR: Could not reallocate memory for IDAT\n");
                    free(idat_data);
                    fclose(input_fp);
                    return 1;
                }
                idat_data = new_idat_data;
            }
            read_bytes(input_fp, idat_data + idat_size, chunk_size);
            idat_size += chunk_size;
        } else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        } else {
            fseek(input_fp, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(input_fp);
    }
    fclose(input_fp);

    // Process the image
    if(idat_data && idat_size > 0) {
        printf("\nProcessing image data...\n");
        image_t *image = process_idat_chunks(&ihdr, idat_data, idat_size);

        if(image) {
            if(force_grayscale || image->channels == 1) {
                // Convert to grayscale if needed
                uint8_t **grayscale = rgb_to_grayscale(image);
                uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                uint8_t **temp = NULL;

                // Initialize processed matrix
                for(uint32_t y = 0; y < image->height; y++) {
                    memcpy(processed[y], grayscale[y], image->width);
                }

                if(steps > 1) {
                    temp = allocate_pixel_matrix(image->height, image->width);
                }

                // Apply convolution
                if(kernel != KERNEL_NONE) {
                    printf("Applying filter");
                    if(steps > 1) printf(" (%d steps)", steps);
                    printf("...\n");

                    uint8_t **input = processed;
                    uint8_t **output = processed;

                    for(uint8_t i = 0; i < steps; i++) {
                        if(i > 0) {
                            // Swap buffers for multiple steps
                            output = (input == processed) ? temp : processed;
                        }
                        apply_convolution(input, output, image->height, image->width, kernel);
                        input = output;
                    }

                    // Ensure final result is in processed
                    if(output != processed && temp) {
                        for(uint32_t y = 0; y < image->height; y++) {
                            memcpy(processed[y], output[y], image->width);
                        }
                    }
                }

                // Save grayscale image
                save_png(output_file, processed, image->width, image->height, 0, 1);

                // Cleanup
                if(grayscale != image->pixels) {
                    free_pixel_matrix(grayscale, image->height);
                }
                free_pixel_matrix(processed, image->height);
                if(temp) {
                    free_pixel_matrix(temp, image->height);
                }
            }
            else if(kernel != KERNEL_NONE) {
                // Process RGB/RGBA image
                if(image->channels >= 3) {
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width * image->channels);

                    // Initialize with original data
                    for(uint32_t y = 0; y < image->height; y++) {
                        memcpy(processed[y], image->pixels[y], image->width * image->channels);
                    }

                    printf("Applying filter");
                    if(steps > 1) printf(" (%d steps)", steps);
                    printf("...\n");

                    // Apply kernel to each channel
                    for(uint32_t ch = 0; ch < 3; ch++) {
                        uint8_t **channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **proc_channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **temp_channel = NULL;

                        if(steps > 1) {
                            temp_channel = allocate_pixel_matrix(image->height, image->width);
                        }

                        // Extract channel
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                channel[y][x] = image->pixels[y][x * image->channels + ch];
                            }
                        }

                        // Apply kernel
                        uint8_t **input = channel;
                        uint8_t **output = proc_channel;

                        for(uint8_t i = 0; i < steps; i++) {
                            apply_convolution(input, output, image->height, image->width, kernel);

                            if(i < steps - 1) {
                                uint8_t **swap = input;
                                input = output;
                                output = (swap == channel && temp_channel) ? temp_channel : channel;
                            }
                        }

                        // Copy processed channel back
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                processed[y][x * image->channels + ch] = output[y][x];
                            }
                        }

                        free_pixel_matrix(channel, image->height);
                        free_pixel_matrix(proc_channel, image->height);
                        if(temp_channel) {
                            free_pixel_matrix(temp_channel, image->height);
                        }
                    }

                    // Copy alpha channel if present
                    if(image->channels == 4) {
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                processed[y][x * image->channels + 3] = image->pixels[y][x * image->channels + 3];
                            }
                        }
                    }

                    // Save RGB/RGBA image
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, processed, image->width, image->height, color_type, image->channels);
                    free_pixel_matrix(processed, image->height);
                }
                else {
                    // Grayscale with kernel
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                    uint8_t **temp = NULL;

                    // Initialize with original data
                    for(uint32_t y = 0; y < image->height; y++) {
                        memcpy(processed[y], image->pixels[y], image->width);
                    }

                    if(steps > 1) {
                        temp = allocate_pixel_matrix(image->height, image->width);
                    }

                    uint8_t **input = processed;
                    uint8_t **output = processed;

                    for(uint8_t i = 0; i < steps; i++) {
                        if(i > 0) {
                            output = (input == processed) ? temp : processed;
                        }
                        apply_convolution(input, output, image->height, image->width, kernel);
                        input = output;
                    }

                    // Ensure final result is in processed
                    if(output != processed && temp) {
                        for(uint32_t y = 0; y < image->height; y++) {
                            memcpy(processed[y], output[y], image->width);
                        }
                    }

                    save_png(output_file, processed, image->width, image->height, 0, 1);
                    free_pixel_matrix(processed, image->height);
                    if(temp) {
                        free_pixel_matrix(temp, image->height);
                    }
                }
            }
            else {
                // No kernel applied - just save original or convert format
                if(image->channels >= 3) {
                    // Save RGB/RGBA image
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, image->pixels, image->width, image->height, color_type, image->channels);
                }
                else {
                    // Save grayscale image
                    save_png(output_file, image->pixels, image->width, image->height, 0, 1);
                }
            }

            free_pixel_matrix(image->pixels, image->height);
            free(image);
        } else {
            fprintf(stderr, "ERROR: Failed to process image data\n");
            free(idat_data);
            return 1;
        }

        free(idat_data);
    } else {
        fprintf(stderr, "ERROR: No IDAT chunks found or empty image data\n");
        return 1;
    }

    printf("\nDone!\n");
    return 0;
}
