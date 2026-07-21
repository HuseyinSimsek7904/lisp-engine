#ifndef CORE_H
#define CORE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DA_INIT_CAP 0x100
#define da_item_size(da) (sizeof((da)->array[0]))
#define da_avail(da) ((da)->capacity - (da)->size)
#define da_resize(da)                                                          \
  (da)->array = reallocarray((da)->array, (da)->capacity, da_item_size(da));   \
  assert((da)->array);
#define da_init(da)                                                            \
  do {                                                                         \
    (da)->array = calloc(DA_INIT_CAP, da_item_size(da));                       \
    assert((da)->array);                                                       \
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
#define sb_append_view(da, v) da_append_many((da), (v.array), (v.size))

typedef struct {
  char *array;
  size_t size;
} StringView;

#define sb_to_sv(da)                                                           \
  (StringView) { .array = (da)->array, .size = (da)->size }
#define sv_head(v) (*(v).array)

static inline StringView sv_copy(StringView view) {
  char *buffer = malloc(view.size);
  assert(buffer);

  memcpy(buffer, view.array, view.size);
  return (StringView){.array = buffer, .size = view.size};
}

static inline char *sv_copy_cstr(StringView view) {
  char *buffer = malloc(view.size + 1);
  assert(buffer);

  memcpy(buffer, view.array, view.size);
  buffer[view.size] = '\0';
  return buffer;
}

static inline char sv_pop_left(StringView *view) {
  assert(view->size);
  view->size--;
  return *view->array++;
}

static inline bool sv_cmp(StringView a, StringView b) {
  if (a.size == b.size)
    return memcmp(a.array, b.array, a.size);

  int cmp = memcmp(a.array, b.array, a.size < b.size ? a.size : b.size);
  if (cmp != 0)
    return cmp;

  return a.size - b.size;
}

#define TODO()                                                                 \
  do {                                                                         \
    lisp_fprintf(stderr, __FILE__ ":%u: not implemented yet\n", __LINE__);     \
  } while (0)

void sb_append_decimal(StringBuilder *builder, int64_t number);
void sb_append_view_printable(StringBuilder *builder, StringView string,
                              bool use_char_escapes);

StringView read_file_sv(char *file_name);

#endif
