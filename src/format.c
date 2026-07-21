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

  lisp_value_format(builder, value->cons.head, formatting);

  while (lisp_is_non_empty_list(value->cons.tail)) {
    value = value->cons.tail;

    sb_append_static_str(builder, " ");
    lisp_value_format(builder, value->cons.head, formatting);
  }
  assert(lisp_is_nil(value->cons.tail));
}

void lisp_value_format(StringBuilder *builder, LispValue value,
                       FormattingType formatting) {

  // TODO: A flag that prints values as their producer should be implemented.
  // That way, when the formatted string is evaluated, the result should be
  // precisely the input.
  // So this should always hold (given the builtin `format`, returns the
  // formatted string with this flag on):
  // (= (eval (parse-lisp (format x))) x)

  switch (value->type) {
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

    assert(lisp_is_valid_type(value->ref_type));

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "<type `");

    sb_append_cstr(builder, lisp_type_syms[value->ref_type]);

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "`>");

    break;

  case LISP_BOOL_T:
    // Producers: true, false

    if (formatting == FORMAT_BASIC)
      sb_append_static_str(builder, "<");

    sb_append_cstr(builder, value->boolean ? "true" : "false");

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
    sb_append_view_printable(builder, value->string, true);
    da_append(builder, '"');

    break;

  case LISP_SYMBOL_T:
    // Producers: (quote SYMBOL)

    if (formatting == FORMAT_PRODUCER)
      sb_append_static_str(builder, "(quote ");

    sb_append_cstr(builder, value->symbol);

    if (formatting == FORMAT_PRODUCER)
      sb_append_static_str(builder, ")");

    break;

  case LISP_INT64_T:
    // Producers: INT

    // TODO: Flags that print the integer as an unsigned number and in any base
    // should be implemented.

    sb_append_decimal(builder, value->integer);

    break;

  case LISP_BUILTIN_FUNC_T:
    // Producers: BUILTIN_SYM

    if (formatting == FORMAT_PRODUCER) {
      sb_append_cstr(builder, value->builtin_function->name);

    } else {
      if (value->builtin_function->is_macro)
        sb_append_static_str(builder, "<builtin-macro `");
      else
        sb_append_static_str(builder, "<builtin-fn `");

      sb_append_cstr(builder, value->builtin_function->name);
      sb_append_static_str(builder, "`>");
    }

    break;

  case LISP_FUNC_T:
    // Producers: (fn ARGS BODY...)

    da_append(builder, formatting == FORMAT_PRODUCER ? '(' : '<');

    if (value->custom_function->is_macro)
      sb_append_static_str(builder, "macro ");
    else
      sb_append_static_str(builder, "fn ");

    sb_append_static_str(builder, "(");
    if (!lisp_is_nil(value->custom_function->args)) {
      LispValue remaining_args = value->custom_function->args;

      while (lisp_is_non_empty_list(remaining_args->cons.tail)) {
        sb_append_cstr(builder, remaining_args->cons.head->symbol);
        sb_append_static_str(builder, " ");
        remaining_args = remaining_args->cons.tail;
      }
      assert(lisp_is_nil(remaining_args->cons.tail));

      if (value->custom_function->variadic)
        sb_append_static_str(builder, "(rest ");

      sb_append_cstr(builder, remaining_args->cons.head->symbol);

      if (value->custom_function->variadic)
        sb_append_static_str(builder, ")");
    }
    sb_append_static_str(builder, ")");

    if (formatting == FORMAT_PRODUCER) {
      value = value->custom_function->body;
      assert(lisp_is_non_empty_list(value));

      sb_append_static_str(builder, " ");
      lisp_value_format(builder, value->cons.head, FORMAT_BASIC);

      while (lisp_is_non_empty_list(value->cons.tail)) {
        value = value->cons.tail;

        sb_append_static_str(builder, " ");
        lisp_value_format(builder, value->cons.head, FORMAT_BASIC);
      }

      assert(lisp_is_nil(value->cons.tail));
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
