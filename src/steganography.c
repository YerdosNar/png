#include "../include/steganography.h"
#include "../include/png_io.h"
#include <string.h>
#include <unistd.h>

/**
 * @brief Checks if the given file has a valid PNG signature.
 * * @param file The file stream to check.
 * @return true if the file is a PNG, false otherwise.
 */
bool isPNG(FILE *file) {
    uint8_t first_8_bytes[8];
    // This read moves the file pointer, so it must be reset.
    long original_pos = ftell(file);
    rewind(file);
    read_bytes(file, first_8_bytes, 8);
    fseek(file, original_pos, SEEK_SET); // Restore original file position

    if (memcmp(first_8_bytes, png_sig, 8) != 0) {
        fprintf(stderr, "ERROR: Not a valid PNG file\n");
        return false;
    }
    return true;
}

/**
 * @brief Detects and prints custom ancillary chunks in a PNG file.
 *
 * @param file A pointer to the opened PNG file stream (must be readable).
 * @return true if a hidden chunk was found, false otherwise.
 */
bool detect(FILE *file) {
    if(!file) {
        fprintf(stderr, "ERROR: Invalid file pointer passed to detect()\n");
        return false;
    }
    rewind(file); // Ensure we start from the beginning
    if(!isPNG(file)) {
        return false;
    }

    // isPNG moves the pointer, so we must skip the signature again
    fseek(file, 8, SEEK_SET);

    printf("Searching for hidden chunks...\n");
    bool found_hidden_chunk = false;
    while(!feof(file)) {
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[5] = {0};
        read_chunk_type(file, chunk_type);

        // Check for private, ancillary chunk (first letter is lowercase)
        if(chunk_type[0] >= 'a' && chunk_type[0] <= 'z' && memcmp(chunk_type, "tRNS", 4) != 0) {
            found_hidden_chunk = true;
            printf("\n✅ Found hidden chunk: \033[31m%s\033[0m\n", chunk_type);
            printf("   Length: %u bytes\n", chunk_size);

            if(chunk_size > 0) {
                char *data = (char *)malloc(chunk_size + 1);
                if(!data) {
                    fprintf(stderr, "ERROR: Failed to allocate memory for data\n");
                    fseek(file, chunk_size, SEEK_CUR);
                } else {
                    read_bytes(file, data, chunk_size);
                    data[chunk_size] = '\0';
                    printf("   Message: \"\033[31m%s\033[0m\"\n", data);
                    free(data);
                }
            } else {
                printf("   Message: (empty)\n");
            }
        } else {
            fseek(file, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(file);
        if(memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }
    }

    if(!found_hidden_chunk) {
        printf("No hidden chunks found!\n");
        printf("\033[32mFile is clean.\033[0m\n");
    }
    rewind(file);
    return found_hidden_chunk;
}

/**
 * @brief Injects a custom data chunk into a PNG file before the IEND chunk.
 *
 * @param file A pointer to the opened PNG file stream.
 * @param type The 4-character type for the custom chunk.
 * @param message The string message to hide in the chunk.
 */
void inject_chunk(FILE *file, char type[], char message[]) {
    if(!file) {
        fprintf(stderr, "ERROR: Invalid file pointer passed to inject()\n");
        return;
    }
    rewind(file);
    if(!isPNG(file)) {
        return;
    }

    type[strcspn(type, "\n")] = 0;
    message[strcspn(message, "\n")] = 0;

    // Chunk validation
    if(strlen(type) != 4 || !(type[0] >= 'a' && type[0] <= 'z')) {
        fprintf(stderr, "ERROR: Chunk type must be 4 characters and start with lowercase\n");
        return;
    }

    // Is the file empty?
    fseek(file, 0, SEEK_END);
    if(ftell(file) < 20) {
        fprintf(stderr, "ERROR: File is too small to be a valid PNG.\n");
        return;
    }
    rewind(file);

    fseek(file, 8, SEEK_SET); // Skip signature
    long iend_pos = -1;
    while(!feof(file)) {
        long curr_pos = ftell(file);
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[4];
        read_chunk_type(file, chunk_type);

        if(memcmp(chunk_type, "IEND", 4) == 0) {
            iend_pos = curr_pos;
            break;
        }
        fseek(file, chunk_size, SEEK_CUR);
        read_chunk_crc(file);
    }

    if(iend_pos == -1) {
        fprintf(stderr, "ERROR: Could not find IEND chunk. Injection failed...\n");
        return;
    }

    uint8_t iend_chunk_data[12];
    fseek(file, iend_pos, SEEK_SET);
    read_bytes(file, iend_chunk_data, sizeof(iend_chunk_data));

    // Go to IEND start
    fseek(file, iend_pos, SEEK_SET);

    uint32_t msg_len = strlen(message);
    write_chunk(file, type, (uint8_t *)message, msg_len);

    // Write IEND back
    write_bytes(file, iend_chunk_data, sizeof(iend_chunk_data));

    printf("\n🚀 Successfully injected chunk '%s' with a %u-byte message.\n", type, msg_len);
}

/**
* @brief Deletes a specific ancillary chunk from a PNG file.
*
* This function work by finding the target chunk, reading all subsequent file
* dta into a buffer, writing that buffer back over the target chunk's location,
* and finally truncating the file to its new, shorter size.
*
* @param file A pointer in "r+b" mode
* @param type the 4-character type for chunk_type
*/
void delete_chunk(FILE *file, char type[]) {
    if(!file) {
        fprintf(stderr, "ERROR: Invalid file pointer passed to delete_chunk()\n");
        return;
    }
    rewind(file);

    type[strcspn(type, "\n")] = 0;
    // Chunk validation
    if(strlen(type) != 4 || !(type[0] >= 'a' && type[0] <= 'z')) {
        fprintf(stderr, "ERROR: Chunk type must be 4 characters and start with lowercase\n");
        return;
    }

    // find the chunk
    long chunk_pos = -1;
    uint32_t chunk_size = 0;
    fseek(file, 8, SEEK_SET); // signature skipped
    while(!feof(file)) {
        long curr_pos = ftell(file);
        if(curr_pos == -1) break;

        uint32_t curr_chunk_size = read_chunk_size(file);
        uint8_t curr_chunk_type[4];
        read_chunk_type(file, curr_chunk_type);

        if(memcmp(type, curr_chunk_type, 4) == 0) {
            chunk_pos = curr_pos;
            // total size = 4length+4type+data+4crc
            chunk_size = curr_chunk_size + 12;
            break;
        }
        fseek(file, curr_chunk_size+4, SEEK_CUR);

        if(memcmp(curr_chunk_type, "IEND", 4) == 0) break;
    }

    if(chunk_pos == -1) {
        fprintf(stderr, "ERROR: Given chunk '%s' is not found!\n", type);
        return;
    }

    printf("Found chunk '%s' at position %ld (total size: %u bytes)\n", type, chunk_pos, chunk_size);

    // Copying the rest
    long after_chunk_pos = chunk_pos + chunk_size;
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    long after_data_size = file_size - after_chunk_pos;

    // if the chunk at the end, just truncate
    if(after_data_size <= 0) {
        printf("Chunk is at the end of the file. Truncating...\n");
        if(ftruncate(fileno(file), chunk_pos) != 0) {
            fprintf(stderr, "ERROR: Could not truncate file\n");
        }
        printf("✅ Successfully deleted the chunk '%s'.\n", type);
        return;
    }

    uint8_t *buffer = malloc(after_data_size);
    if(!buffer) {
        fprintf(stderr, "ERROR: Could not allocate memory for buffer\n");
        return;
    }

    // Read the data after the chunk
    fseek(file, after_chunk_pos, SEEK_SET);
    read_bytes(file, buffer, after_data_size);

    // Write the buffer after truncate
    fseek(file, chunk_pos, SEEK_SET);
    write_bytes(file, buffer, after_data_size);

    // truncate the file to its new, smaller size
    if(ftruncate(fileno(file), file_size - chunk_size) != 0) {
        fprintf(stderr, "ERROR: Could not truncate the file\n");
        return;
    }

    free(buffer);
    printf("✅ Successfully deleted chunk '%s'.\n", type);
}
