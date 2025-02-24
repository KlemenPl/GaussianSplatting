//
// Created by Klemen Plestenjak on 2/20/25.
//

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>


const char *readFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char *buffer = malloc(size + 1);
    if (!buffer) {
        return NULL;
    }
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}
