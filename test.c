#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    FILE *file = fopen(argv[1], "r");
    if(!file) {
        printf("Nothing happened\n");
        return 1;
    }

    printf("Print this\n");

    return 0;
}
