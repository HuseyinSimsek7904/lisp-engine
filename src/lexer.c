#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core.h"
#include "lexer.h"

static inline int hex_digit_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  return -1;
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

    case '0' ... '9': {
      uint64_t number = c - '0';
      while (*lisp_str >= '0' && *lisp_str <= '9') {
        number = 10 * number + *lisp_str++ - '0';
      }

      if (*lisp_str != ')' && *lisp_str != ' ' && *lisp_str != '\n' &&
          *lisp_str != '\t' && *lisp_str != '\0')
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

      if (*lisp_str != ')' && *lisp_str != ' ' && *lisp_str != '\n' &&
          *lisp_str != '\t' && *lisp_str != '\0')
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_STRING, .string = sb.array};
      da_append(tokens, token);
    } break;

    case '-':
      if (*lisp_str >= '0' && *lisp_str <= '9') {
        uint64_t number = 0;
        while (*lisp_str >= '0' && *lisp_str <= '9') {
          number = 10 * number + *lisp_str++ - '0';
        }

        if (*lisp_str != ')' && *lisp_str != ' ' && *lisp_str != '\n' &&
            *lisp_str != '\t' && *lisp_str != '\0')
          return LEXER_EXPECTED_SEPARATOR;

        Token token = {.type = T_INT64, .integer = -number};
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
      while (*lisp_str == '!' || (*lisp_str >= '#' && *lisp_str <= '&') ||
             (*lisp_str >= '*' && *lisp_str <= '~')) {
        da_append(&sb, *lisp_str++);
      }

      if (*lisp_str != ')' && *lisp_str != ' ' && *lisp_str != '\n' &&
          *lisp_str != '\t' && *lisp_str != '\0')
        return LEXER_EXPECTED_SEPARATOR;

      Token token = {.type = T_SYMBOL, .string = sb.array};
      da_append(tokens, token);
    } break;

    case ';':
      while (*lisp_str != '\0' && *lisp_str != '\n') {
        lisp_str++;
      }
      break;

    case '\'':
      TODO();
      return LEXER_UNKNOWN_CHAR;
    }
  }

  return LEXER_SUCCESS;
}

void print_lisp_token(Token token) {
  switch (token.type) {
  case T_L_PAREN:
    printf("(");
    break;

  case T_R_PAREN:
    printf(")");
    break;

  case T_INT64:
    printf("%lu", token.integer);
    break;

  case T_STRING:
    printf("string[%s]", token.string);
    break;

  case T_SYMBOL:
    printf("symbol[%s]", token.symbol);
    break;

  default:
    TODO();
  }
}
