#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include "core.h"
#include "lexer.h"

static inline bool is_decimal(char c) { return c >= '0' && c <= '9'; }

static inline bool is_separator(char c) {
  return c == ')' || c == ' ' || c == '\n' || c == '\t';
}

static inline bool is_symbol_tail(char c) {
  return c == '!' || (c >= '#' && c <= '&') || (c >= '*' && c <= ':') ||
         (c >= '<' && c <= '~');
}

static inline int hex_digit_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  return -1;
}

// Read the next signed decimal int64 and advance the string view.
// If some part of the number is already parsed, then this function can be used
// to parse the rest.
// If the number is negative, the result
LexerResult read_int64_dec(char **lisp_str, int64_t *number,
                           bool negative_mode) {
  while (is_decimal(**lisp_str)) {
    if (ckd_mul(number, 10, *number))
      return LEXER_INTEGER_TOO_LARGE;

    if (negative_mode) {
      if (ckd_sub(number, *number, *(*lisp_str)++ - '0'))
        return LEXER_INTEGER_TOO_LARGE;
    } else {
      if (ckd_add(number, *number, *(*lisp_str)++ - '0'))
        return LEXER_INTEGER_TOO_LARGE;
    }
  }

  return LEXER_SUCCESS;
}

LexerResult generate_tokens(char *lisp_str, Tokens *tokens) {
  while (*lisp_str) {
    char c = *lisp_str++;
    switch (c) {
    case ' ':
    case '\n':
    case '\t':
      continue;

    case '(':
      da_append(tokens, (Token){.type = T_L_PAREN});
      break;

    case ')':
      da_append(tokens, (Token){.type = T_R_PAREN});
      break;

    case '\'':
      da_append(tokens, (Token){.type = T_QUOTE});
      break;

    case '0' ... '9': {
      int64_t number = c - '0';

      LexerResult result = read_int64_dec(&lisp_str, &number, false);
      if (result != LEXER_SUCCESS)
        return result;

      if (*lisp_str != '\0' && !is_separator(*lisp_str))
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_INT64, .integer = number};
      da_append(tokens, token);
    } break;

    case '"': {
      StringBuilder sb;
      da_init(&sb);
      while (*lisp_str != '"') {
        if (*lisp_str == '\0')
          return LEXER_UNTERMINATED_STRING;

        if (*lisp_str == '\\') {
          // Escape sequences.
          lisp_str++;

          switch (*lisp_str++) {
          case '\0':
            return LEXER_UNTERMINATED_STRING;

          case '\\':
            da_append(&sb, '\\');
            break;

          case '\'':
            da_append(&sb, '\'');
            break;

          case '\"':
            da_append(&sb, '\"');
            break;

          case 'a':
            da_append(&sb, '\a');
            break;

          case 'b':
            da_append(&sb, '\b');
            break;

          case 'e':
            da_append(&sb, '\e');
            break;

          case 't':
            da_append(&sb, '\t');
            break;

          case 'n':
            da_append(&sb, '\n');
            break;

          case 'v':
            da_append(&sb, '\v');
            break;

          case 'f':
            da_append(&sb, '\f');
            break;

          case 'r':
            da_append(&sb, '\r');
            break;

          case 'x':
            if (*lisp_str == '\0')
              return LEXER_UNTERMINATED_STRING;

            int tens = hex_digit_to_int(*lisp_str++);
            if (*lisp_str == '\0')
              return LEXER_UNTERMINATED_STRING;

            int ones = hex_digit_to_int(*lisp_str++);

            if (tens < 0 || ones < 0)
              return LEXER_INVALID_ESCAPE;

            da_append(&sb, tens * 16 + ones);
            break;

          default:
            return LEXER_INVALID_ESCAPE;
          }
        } else
          da_append(&sb, *lisp_str++);
      }
      lisp_str++;

      if (*lisp_str != '\0' && !is_separator(*lisp_str))
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_STRING, .string = sb.array};
      da_append(tokens, token);
    } break;

    case '-':
      if (is_decimal(*lisp_str)) {
        int64_t number = -(*lisp_str++ - '0');

        LexerResult result = read_int64_dec(&lisp_str, &number, true);
        if (result != LEXER_SUCCESS)
          return result;

        if (*lisp_str != '\0' && !is_separator(*lisp_str))
          return LEXER_EXPECTED_SEPARATOR;

        Token token = {.type = T_INT64, .integer = number};
        da_append(tokens, token);
        break;
      }

    case '!':
    case '#' ... '&':
    case '*' ... ',':
    case '.' ... '/':
    case ':':
    case '<' ... '~': {
      StringBuilder sb;
      da_init(&sb);
      da_append(&sb, c);
      while (is_symbol_tail(*lisp_str)) {
        da_append(&sb, *lisp_str++);
      }

      if (*lisp_str != '\0' && !is_separator(*lisp_str))
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_SYMBOL, .string = sb.array};
      da_append(tokens, token);
    } break;

    case ';':
      while (*lisp_str != '\0' && *lisp_str != '\n') {
        lisp_str++;
      }
      break;

    default:
      return LEXER_UNKNOWN_CHAR;
    }
  }

  return LEXER_SUCCESS;
}
