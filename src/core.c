#include "core.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

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

void sb_append_decimal(StringBuilder *builder, int64_t number) {
  if (number == 0) {
    da_append(builder, '0');
    return;
  }

  uint64_t positive = number;
  if (number < 0) {
    da_append(builder, '-');
    positive = -number;
  }

  char buffer[64];
  char *ptr = buffer + 64;
  while (positive) {
    *(--ptr) = positive % 10 + '0';
    positive = positive / 10;
  }

  da_append_many(builder, ptr, buffer + 64 - ptr);
}

static inline unsigned char hex_char(int x) {
  if (x >= 0 && x <= 9)
    return x + '0';

  else
    return x - 10 + 'a';
}

void sb_append_printable(StringBuilder *builder, char *str,
                         bool use_char_escapes) {
  unsigned char c;
  while ((c = *str++) != '\0') {
    if ((c >= ' ' && c < 0x7f) && c != '\"' && c != '\'') {
      da_append(builder, c);
      continue;
    }

    if (use_char_escapes) {
      const char *escape = NULL;
      switch (c) {
      case '\a':
        escape = "\\a";
        break;
      case '\b':
        escape = "\\b";
        break;
      case '\t':
        escape = "\\t";
        break;
      case '\n':
        escape = "\\n";
        break;
      case '\v':
        escape = "\\v";
        break;
      case '\r':
        escape = "\\r";
        break;
      case '\e':
        escape = "\\e";
        break;
      case '\"':
        escape = "\\\"";
        break;
      case '\'':
        escape = "\\\"";
        break;
      }

      if (escape) {
        da_append_many(builder, escape, 2);
        continue;
      }
    }

    sb_append_static_str(builder, "\\x");
    da_append(builder, hex_char(c / 16));
    da_append(builder, hex_char(c % 16));
  }
}
