#ifndef STEGANOGRAPHY_H
#define STEGANOGRAPHY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

bool detect(FILE *file);
void inject_chunk(FILE *file, char type[], char message[]);
void delete_chunk(FILE *file, char type[]);

#endif
