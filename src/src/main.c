#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../include/png_io.h"
#include "../include/image.h"
#include "../include/utils.h"

void usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions: \n");
    printf("  -o, --output <file>    Output filename (default=out.png)\n");
    printf("  -i, --info <file>      Show information about PNG file\n");
    printf("  -g, --gray             Convert to grayscale\n");
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
    printf("Author: YerdosNar github.com/YerdosNar/png.git\n");
}

int main(int argc, char **argv) {
    if(argc < 2) {
        usage(argv[0]);
        return 1;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    bool force_grayscale = false;
    bool conflict_color = false;
    bool conflict_kernel = false;
    kernel_type kernel = KERNEL_NONE;
    uint16_t steps = 0;

    for(int i = 1; i < argc; i++) {
        if((!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) && i == 1) {
            usage(argv[0]);
            return 0;
        }
        else if(!strcmp(argv[i], "-i") || !strcmp(argv[i], "--info")) {
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
            print_info(file);
            fclose(file);
            return 0;
        }
        else if(!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if(i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "ERROR: -o requires an argument after it\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gray")) {
            if(!conflict_color) {
                force_grayscale = true;
                conflict_color = true;
            } else {
                fprintf(stderr, "ERROR: RGB and GRAYSCALE both cannot be set\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--rgb")) {
            if(!conflict_color) {
                force_grayscale = false;
                conflict_color = true;
            } else {
                fprintf(stderr, "ERROR: RGB and GRAYSCALE both cannot be set\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel-x")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_X;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel-y")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_Y;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_COMBINED;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--gaussian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_GAUSSIAN;
                if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                    steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--blur")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_BLUR;
                if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                    steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--laplacian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_LAPLACIAN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sharpen")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SHARPEN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--none")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_NONE;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(strstr(argv[i], ".png") != NULL && input_file == NULL) {
            input_file = argv[i];
        }
    }

    // validate args
    if(!input_file) {
        fprintf(stderr, "ERROR: No input file specified\n");
        usage(argv[0]);
        return 1;
    }
    if(!output_file) {
        printf("No output filename was set\n");
        printf("Dfault: out.png");
        output_file = "out.png";
    }

    if(steps == 0 && kernel != KERNEL_NONE) steps = 1;

    // warn user
    if((kernel == KERNEL_SOBEL_COMBINED || kernel == KERNEL_SOBEL_X || kernel == KERNEL_SOBEL_Y || kernel == KERNEL_LAPLACIAN) && !force_grayscale) {
        printf("Node: Edge detection typically works better on grayscale image.\n");
        printf("Consider adding --gray flag.\n\n");
    }

    FILE *input_f = fopen(input_file, "rb");
    if(!input_f) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", input_file);
        return 1;
    }

    // Check if it is png
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_f, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_signature, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file);
        printf("SIGNATURE: ");
        print_bytes(signature, PNG_SIG_SIZE);
        fclose(input_f);
        return 1;
    }

    printf("Processing: %s\n", input_file);
    printf("Kernel: ");
    switch(kernel) {
        case KERNEL_SOBEL_X: printf("Sobel X\n"); break;
        case KERNEL_SOBEL_Y: printf("Sobel Y\n"); break;
        case KERNEL_SOBEL_COMBINED: printf("Sobel combined\n"); break;
        case KERNEL_GAUSSIAN: printf("Gaussian");
            if(steps > 1) printf(" (%d steps)", steps);
            printf("\n");
            break;
        case KERNEL_BLUR: printf("Blur\n"); break;
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
    uint64_t idat_cap = 0;
    bool quit = false;

    while(!quit) {
        uint32_t chunk_size = read_chunk_size(input_f);
        uint8_t chunk_type[4];
        read_chunk_type(input_f, chunk_type);

        if(memcmp(chunk_type, "IHDR", sizeof(chunk_type)) == 0) {
            read_bytes(input_f, &ihdr.width, 4);
            read_bytes(input_f, &ihdr.height, 4);
            read_bytes(input_f, &ihdr.bit_depth, 1);
            read_bytes(input_f, &ihdr.color_type, 1);
            read_bytes(input_f, &ihdr.compression, 1);
            read_bytes(input_f, &ihdr.filter, 1);
            read_bytes(input_f, &ihdr.interlace, 1);

            reverse_bytes(&ihdr.width, sizeof(ihdr.width));
            reverse_bytes(&ihdr.height, sizeof(ihdr.height));

            printf("Image dimensions: %u x %u px\n", ihdr.width, ihdr.height);
            printf("Bit depth: %u, Color type: %u\n", ihdr.bit_depth, ihdr.color_type);
        }
        // TODO: Re-learn this block of code
        else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            if(idat_cap < idat_size + chunk_size) {
                idat_cap = (idat_size + chunk_size) * 2;
                idat_data = realloc(idat_data, idat_cap); // FREE <- FLAG
                if(!idat_data) {
                    fprintf(stderr, "ERROR: Could not reallocate memory for IDAT\n");
                    exit(1);
                }
            }
            read_bytes(input_f, idat_data + idat_size, chunk_size);
            idat_size += chunk_size;
        }
        else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        }
        else {
            fseek(input_f, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(input_f);
    }
    fclose(input_f);

    // create new PNG file
    if(idat_data && idat_size > 0) {
        printf("\nProcessing image data...\n");
        image_t *image = process_idat_chunks(&ihdr, idat_data, idat_size);

        if(image) {
            if(force_grayscale) {
                uint8_t **gray = rgb_to_grayscale(image);
                uint8_t **output = allocate_pixel_matrix(image->height, image->width);
                uint8_t **temp = NULL;

                if(steps > 1) {
                    temp = allocate_pixel_matrix(image->height, image->width);
                }

                printf("Applying filter...\n");
                if(steps > 1) printf(" (%d steps)", steps);
                printf("...\n");

                for(uint8_t i = 0; i < steps; i++) {
                    apply_convolution(gray, output, image->height, image->width, kernel);
                    if(i < steps-1) {
                        uint8_t **swap = gray;
                        gray = output;
                        output = (swap == gray && temp) ? temp : gray; // TODO: Re-learn this part
                    }
                }

                save_png(output_file, output, ihdr, 1); // 1 - GRAY
                free_pixel_matrix(gray, image->height);
                free_pixel_matrix(output, image->height);
                free_pixel_matrix(temp, image->height);
            }
            else if(kernel != KERNEL_NONE) {
                if(image->channels >= 3) { // RGB
                    uint8_t **output = allocate_pixel_matrix(image->height, image->width * image->channels);

                    printf("Applying filter..\n");
                    if(steps > 1) printf(" (%d steps)", steps);
                    printf("...\n");
                }
            }
        }
    }

    return 0;
}
