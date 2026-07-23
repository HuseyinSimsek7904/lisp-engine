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
LexerResult read_int64_dec(StringView *view, int64_t *number,
                           bool negative_mode) {
  while (view->size && is_decimal(sv_head(*view))) {
    if (ckd_mul(number, 10, *number))
      return LEXER_INTEGER_TOO_LARGE;

    if (negative_mode) {
      if (ckd_sub(number, *number, sv_pop_left(view) - '0'))
        return LEXER_INTEGER_TOO_LARGE;
    } else {
      if (ckd_add(number, *number, sv_pop_left(view) - '0'))
        return LEXER_INTEGER_TOO_LARGE;
    }
  }

  return LEXER_SUCCESS;
}

LexerResult generate_tokens(StringView view, Tokens *tokens) {
  while (view.size) {
    char c = sv_pop_left(&view);

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

    case '\'': {
      Token token = {.type = T_SYMBOL, .symbol = "'"};
      da_append(tokens, token);
    } break;

    case '0' ... '9': {
      int64_t number = c - '0';

      LexerResult result = read_int64_dec(&view, &number, false);
      if (result != LEXER_SUCCESS)
        return result;

      if (view.size && !is_separator(sv_head(view)))
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_INT64, .integer = number};
      da_append(tokens, token);
    } break;

    case '"': {
      StringBuilder sb;
      da_init(&sb);
      while (true) {
        if (!view.size)
          return LEXER_UNTERMINATED_STRING;

        c = sv_pop_left(&view);

        if (c == '"')
          break;

        if (c != '\\') {
          da_append(&sb, c);
          continue;
        }

        // Escape sequences.
        if (!view.size)
          return LEXER_UNTERMINATED_STRING;

        switch (sv_pop_left(&view)) {
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
          if (!view.size)
            return LEXER_UNTERMINATED_STRING;

          int tens = hex_digit_to_int(sv_pop_left(&view));

          if (!view.size)
            return LEXER_UNTERMINATED_STRING;

          int ones = hex_digit_to_int(sv_pop_left(&view));

          if (tens < 0 || ones < 0)
            return LEXER_INVALID_ESCAPE;

          da_append(&sb, tens * 16 + ones);
          break;

        default:
          return LEXER_INVALID_ESCAPE;
        }
      }

      if (view.size && !is_separator(sv_head(view)))
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_STRING, .string = sb_to_sv(&sb)};
      da_append(tokens, token);
    } break;

    case '-':
      if (is_decimal(sv_head(view))) {
        int64_t number = -(sv_pop_left(&view) - '0');

        LexerResult result = read_int64_dec(&view, &number, true);
        if (result != LEXER_SUCCESS)
          return result;

        if (view.size && !is_separator(sv_head(view)))
          return LEXER_EXPECTED_SEPARATOR;

        Token token = {.type = T_INT64, .integer = number};
        da_append(tokens, token);
        break;
      }
      // Pass through to symbol reading.

    case '!':
    case '#' ... '&':
    case '*' ... ',':
    case '.' ... '/':
    case ':':
    case '<' ... '~': {
      StringBuilder sb;
      da_init(&sb);
      da_append(&sb, c);
      while (view.size && is_symbol_tail(sv_head(view))) {
        da_append(&sb, sv_pop_left(&view));
      }

      if (view.size && !is_separator(sv_head(view)))
        return LEXER_EXPECTED_SEPARATOR;

      // Symbols are null-terminated.
      da_append(&sb, '\0');

      Token token = {.type = T_SYMBOL, .symbol = sb.array};
      da_append(tokens, token);
    } break;

    case ';':
      while (view.size && sv_head(view) != '\n') {
        sv_pop_left(&view);
      }
      break;

    default:
      return LEXER_UNKNOWN_CHAR;
    }
  }

  return LEXER_SUCCESS;
}
