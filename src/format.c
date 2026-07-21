#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "format.h"
#include "lisp_value.h"

void lisp_format_items(StringBuilder *builder, LispValue value,
                       FormattingType formatting) {
  if (lisp_is_nil(value))
    return;

  assert(lisp_is_non_empty_list(value));

  LispIter items_iter;
  lisp_iter_init(&items_iter, value);

  lisp_value_format(builder, lisp_iter_next(&items_iter), formatting);

  while (lisp_iter_has_next(&items_iter)) {
    sb_append_static_str(builder, " ");
    lisp_value_format(builder, lisp_iter_next(&items_iter), formatting);
  }
}

void lisp_value_format(StringBuilder *builder, LispValue value,
                       FormattingType formatting) {

  // TODO: A flag that prints values as their producer should be implemented.
  // That way, when the formatted string is evaluated, the result should be
  // precisely the input.
  // So this should always hold (given the builtin `format`, returns the
  // formatted string with this flag on):
  // (= (eval (parse-lisp (format x))) x)

  switch (lisp_get_type(value)) {
  case LISP_NIL_T:
    // Producers: nil, ()

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "<");

    sb_append_static_str(builder, "nil");

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, ">");

    break;

  case LISP_TYPE_T:
    // Producers: TYPE_t

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "<type `");

    sb_append_cstr(builder, lisp_get_ref_type_sym(value));

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "`>");

    break;

  case LISP_BOOL_T:
    // Producers: true, false

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "<");

    sb_append_cstr(builder, lisp_get_bool(value) ? "true" : "false");

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, ">");

    break;

  case LISP_ERROR_T:
    // Producers: (error ITEM)

    // TODO: An error printing option could be implemented that only allows
    // printing errors with better descriptions. Or a new
    // `lisp_format_desc_error` function could be implemented.

    da_append(builder, formatting == FORMAT_PRODUCER ? '(' : '<');
    sb_append_static_str(builder, "error ");
    lisp_value_format(builder, value->item, formatting);
    da_append(builder, formatting == FORMAT_PRODUCER ? ')' : '>');

    break;

  case LISP_STRING_T:
    // Producers: "STRING"

    da_append(builder, '"');
    sb_append_view_printable(builder, lisp_get_str(value), true);
    da_append(builder, '"');

    break;

  case LISP_SYMBOL_T:
    // Producers: (quote SYMBOL)

    if (formatting == FORMAT_PRODUCER)
      sb_append_static_str(builder, "(quote ");

    sb_append_cstr(builder, lisp_get_sym(value));

    if (formatting == FORMAT_PRODUCER)
      sb_append_static_str(builder, ")");

    break;

  case LISP_INT64_T:
    // Producers: INT

    // TODO: Flags that print the integer as an unsigned number and in any base
    // should be implemented.

    sb_append_decimal(builder, lisp_get_int64(value));

    break;

  case LISP_BUILTIN_FUNC_T: {
    // Producers: BUILTIN_SYM

    _LispBuiltinFunction *builtin = lisp_get_builtin(value);
    if (formatting == FORMAT_PRODUCER) {
      sb_append_cstr(builder, builtin->name);

    } else {
      if (builtin->is_macro)
        sb_append_static_str(builder, "<builtin-macro `");
      else
        sb_append_static_str(builder, "<builtin-fn `");

      sb_append_cstr(builder, builtin->name);
      sb_append_static_str(builder, "`>");
    }
  } break;

  case LISP_FUNC_T:
    // Producers: (fn ARGS BODY...)

    da_append(builder, formatting == FORMAT_PRODUCER ? '(' : '<');

    _LispCustomFunction *custom = lisp_get_custom(value);

    if (custom->is_macro)
      sb_append_static_str(builder, "macro ");
    else
      sb_append_static_str(builder, "fn ");

    sb_append_static_str(builder, "(");
    if (!lisp_is_nil(custom->args)) {
      LispIter args_iter;
      lisp_iter_init(&args_iter, custom->args);

      while (lisp_iter_has_next_next(&args_iter)) {
        sb_append_cstr(builder, lisp_get_sym(lisp_iter_next(&args_iter)));
        sb_append_static_str(builder, " ");
      }

      if (custom->variadic)
        sb_append_static_str(builder, "(rest ");

      sb_append_cstr(builder, lisp_get_sym(lisp_iter_next(&args_iter)));

      if (custom->variadic)
        sb_append_static_str(builder, ")");
    }
    sb_append_static_str(builder, ")");

    if (formatting == FORMAT_PRODUCER) {
      sb_append_static_str(builder, " ");

      LispIter body_iter;
      lisp_iter_init(&body_iter, custom->body);

      lisp_value_format(builder, lisp_iter_next(&body_iter), FORMAT_BASIC);

      while (lisp_iter_has_next(&body_iter)) {
        sb_append_static_str(builder, " ");
        lisp_value_format(builder, lisp_iter_next(&body_iter), FORMAT_BASIC);
      }
    }

    da_append(builder, formatting == FORMAT_PRODUCER ? ')' : '>');
    break;

  case LISP_UNPACKED_T:
    // Producers: (unpack ITEM)

    da_append(builder, formatting == FORMAT_PRODUCER ? '(' : '<');
    if (formatting)
      sb_append_static_str(builder, "unpack ");
    else
      sb_append_static_str(builder, "unpacked ");

    lisp_value_format(builder, value->item, formatting);
    da_append(builder, formatting == FORMAT_PRODUCER ? ')' : '>');
    break;

  case LISP_LIST_T:
    // Producers: (list [LIST_ITEMS...])

    sb_append_static_str(builder, "(");

    if (formatting)
      sb_append_static_str(builder, "list ");

    lisp_format_items(builder, value, formatting);

    sb_append_static_str(builder, ")");
    break;

  case LISP_TYPE_COUNT:
    lisp_fprintf(stderr, "reached unreachable\n");
    assert(false);
    break;
  }
}

char *lisp_vformat(const char *format, va_list va) {
  StringBuilder builder;
  da_init(&builder);
  char c;

  while ((c = *format++) != '\0') {
    if (c != '%') {
      da_append(&builder, c);
      continue;
    }

    switch (c = *format++) {
    case '%':
      da_append(&builder, c);
      break;
    case 'l':
      lisp_value_format(&builder, va_arg(va, LispValue), FORMAT_BASIC);
      break;
    case 'L':
      lisp_value_format(&builder, va_arg(va, LispValue), FORMAT_PRODUCER);
      break;
    case 's': {
      char *arg = va_arg(va, char *);
      sb_append_cstr(&builder, arg);
    } break;
    default:
      da_free(&builder);
      return NULL;
    }
  }

  da_append(&builder, '\0');
  return builder.array;
}

void lisp_fprintf(FILE *file, const char *format, ...) {
  va_list va;
  va_start(va, format);

  char *buffer = lisp_vformat(format, va);
  if (!buffer)
    buffer = strdup("%ERROR%");

  va_end(va);

  fputs(buffer, file);
  free(buffer);
}

void lisp_printf(const char *format, ...) {
  va_list va;
  va_start(va, format);

  char *buffer = lisp_vformat(format, va);
  if (!buffer)
    buffer = strdup("%ERROR%");

  va_end(va);

  fputs(buffer, stdout);
  free(buffer);
}
