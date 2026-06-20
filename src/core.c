#include "core.h"

char *read_file(char *file_name) {
  FILE *file = fopen(file_name, "r");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(size + 1);
  if (!buffer)
    return NULL;

  if (fread(buffer, 1, size, file) != size)
    return NULL;

  buffer[size] = '\0';
  return buffer;
}
