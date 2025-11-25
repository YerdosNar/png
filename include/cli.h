#ifndef CLI_H
#define CLI_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "processor.h"

typedef struct {
    char *input_file;
    char *output_file;
    bool force_grayscale;
    bool do_upscale;
    bool draw;
    bool draw_color;
    kernel_type kernel;
    uint8_t steps;
    float scale_factor;
    bool show_info;
    bool steg_mode;
    char *steg_operation;  // "find", "inject", or "delete"
} cli_config_t;

// Parse command-line arguments into config structure
// Returns true on success, false on error
bool parse_arguments(int argc, char **argv, cli_config_t *config);

// Display usage information
void usage(char *exec_name);

// Handle steganography commands (hidden feature)
int handle_steg_command(int argc, char **argv);

// Handle info command
int handle_info_command(const char *filename);

// Handle draw command
int handle_draw_command(const char *filename, bool color);

#endif
