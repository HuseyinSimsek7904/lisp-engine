#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DA_INIT_CAP 0x100
#define da_item_size(da) (sizeof((da)->array[0]))
#define da_avail(da) ((da)->capacity - (da)->size)
#define da_resize(da)                                                          \
  (da)->array = reallocarray((da)->array, (da)->capacity, da_item_size(da));
#define da_init(da)                                                            \
  do {                                                                         \
    (da)->array = calloc(DA_INIT_CAP, da_item_size(da));                       \
    (da)->capacity = DA_INIT_CAP;                                              \
    (da)->size = 0;                                                            \
  } while (0)
#define da_free(da) free((da)->array)
#define da_append(da, item)                                                    \
  do {                                                                         \
    if (!da_avail(da)) {                                                       \
      (da)->capacity *= 2;                                                     \
      da_resize(da);                                                           \
    }                                                                          \
    (da)->array[(da)->size++] = (item);                                        \
  } while (0)
#define da_append_many(da, items, n)                                           \
  do {                                                                         \
    if (n > da_avail(da)) {                                                    \
      while (n > da_avail(da)) {                                               \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      da_resize(da);                                                           \
    }                                                                          \
    memcpy(&(da)->array[(da)->size], (items), da_item_size(da) * (n));         \
    (da)->size += (n);                                                         \
  } while (0)
#define da_pop(da) ((da)->array[--(da)->size])

typedef struct {
  char *array;
  size_t size;
  size_t capacity;
} StringBuilder;

#define sb_append_static_str(da, s)                                            \
  da_append_many((da), (s), sizeof((s)) / sizeof(char) - 1)
#define sb_append_cstr(da, s) da_append_many((da), (s), strlen((s)))

#define TODO()                                                                 \
  do {                                                                         \
    fprintf(stderr, __FILE__ ":%u: not implemented yet\n", __LINE__);          \
  } while (0)

void sb_append_decimal(StringBuilder *builder, int64_t number);
void sb_append_printable(StringBuilder *builder, char *str, bool use_char_escapes);

char *read_file(char *file_name);

#endif
