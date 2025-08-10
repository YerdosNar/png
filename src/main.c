#include <stdio.h>
#include "../include/utils.h"
#include "../include/png_io.h"
#include "../include/processor.h"

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
    bool conflict = false; 
    bool conflict_kernel = false;
    kernel_type kernel = KERNEL_NONE;
    uint8_t steps = 0;
    
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
                // steps for blur
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

    // validate args 
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

    // step for blur default 1
    if(steps == 0 && kernel != KERNEL_NONE) {
        steps = 1;
    }

    // edge detection work better on gray
    if((kernel == KERNEL_SOBEL_X || kernel == KERNEL_SOBEL_Y || 
       kernel == KERNEL_SOBEL_COMBINED || kernel == KERNEL_LAPLACIAN) && !force_grayscale) {
        printf("Note: Edge detection typically works better on grayscale images.\n");
        printf("Consider adding --grayscale flag.\n\n");
    }

    FILE*input_fp = fopen(input_file, "rb");
    if(!input_fp) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", input_file);
        return 1;
    }

    // Is it PNG
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
                idat_data = realloc(idat_data, idat_capacity);
                if (!idat_data) {
                    fprintf(stderr, "ERROR: Could not reallocate memory for IDAT\n");
                    exit(1);
                }
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

    //now we need to create a new image
    if(idat_data && idat_size > 0) {
        printf("\nProcessing image data...\n");
        image_t *image = process_idat_chunks(&ihdr, idat_data, idat_size);

        if(image) {
            if(force_grayscale) {
                // grayscale
                uint8_t **grayscale = rgb_to_grayscale(image);
                uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                uint8_t **temp = NULL;

                if(steps > 1) {
                    temp = allocate_pixel_matrix(image->height, image->width);
                }


                // convolution apply
                printf("Applying filter...\n");
                if(steps > 1) printf(" (%d steps)", steps);
                printf("...\n");

                uint8_t **input = grayscale;
                uint8_t **output = processed;

                for(uint8_t i = 0; i < steps; i++) {
                    apply_convolution(input, output, image->height, image->width, kernel);
                    if(i < steps - 1) {
                        uint8_t **swap = input;
                        input = output;
                        output = (swap == grayscale && temp) ? temp : grayscale;
                    }
                }

                // save img
                save_png(output_file, output, image->width, image->height, 0, 1);

                //cleaning
                if(grayscale != image->pixels) {
                    free_pixel_matrix(grayscale, image->height);
                }
                free_pixel_matrix(processed, image->height);
                if(temp) {
                    free_pixel_matrix(temp, image->height);
                }
            }
            else if(kernel != KERNEL_NONE){
                // for RGB
                if(image->channels >= 3) {
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width * image->channels);

                    printf("Applying filter");
                    if(steps > 1) printf(" (%d steps)", steps);
                    printf("...\n");

                    //kernel to each channel
                    for(uint32_t ch = 0; ch < 3; ch++) {
                        uint8_t **channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **proc_channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **temp_channel = NULL;

                        if(steps > 1) {
                            temp_channel = allocate_pixel_matrix(image->height, image->width);
                        }

                        // extract channel
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                channel[y][x] = image->pixels[y][x * image->channels + ch];
                            }
                        }

                        //apply kernel
                        uint8_t **input = channel;
                        uint8_t **output = proc_channel;

                        for(uint8_t i = 0; i < steps; i++) {
                            apply_convolution(input, output, image->height, image->width, kernel);

                            //swap
                            if(i < steps - 1) {
                                uint8_t **swap = input;
                                input = output;
                                output = (swap == channel && temp_channel) ? temp_channel : channel;
                            }
                        }

                        //copy 
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                processed[y][x * image->channels + ch] = output[y][x];
                                // copy alpha
                                if(image->channels == 4 && ch == 0) {
                                    processed[y][x * image->channels + 3] = image->pixels[y][x * image->channels + 3];
                                }
                            }
                        }
                        free_pixel_matrix(channel, image->height);
                        free_pixel_matrix(proc_channel, image->height);
                        if(temp_channel) {
                            free_pixel_matrix(temp_channel, image->height);
                        }
                    }

                    //save rgb
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, processed, image->width, image->height, color_type, image->channels);
                    free_pixel_matrix(processed, image->height);
                }
                else {
                    //if it is gray
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                    uint8_t **temp = NULL;

                    if(steps > 1) {
                        temp = allocate_pixel_matrix(image->height, image->width);
                    }

                    uint8_t **input = image->pixels;
                    uint8_t **output = processed;

                    for(uint8_t i = 0; i < steps; i++) {
                        apply_convolution(input, output, image->height, image->width, kernel);

                        if(i < steps - 1) {
                            uint8_t **swap = input;
                            input = output;
                            output = (swap == image->pixels && temp) ? temp : image->pixels;
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
                if(image->channels >= 3) {
                    //save rgb/rgba img
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, image->pixels, image->width, image->height, color_type, image->channels);
                }
                else {
                    //save grayscale img
                    save_png(output_file, image->pixels, image->width, image->height, 0, 1);
                }
            }

            free_pixel_matrix(image->pixels, image->height);
            free(image);
        }

        free(idat_data);
    }
    printf("\nDone!\n");
    return 0;
}

