#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core.h"
#include "lexer.h"

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
      Token token = {.type = T_INT64, .integer = number};
      da_append(tokens, token);
    } break;

    case '"': {
      // TODO: Add escapes.
      StringBuilder sb;
      da_init(&sb);
      while (*lisp_str != '"') {
        if (*lisp_str == '\0')
          return LEXER_UNTERMINATED_STRING;

        da_append(&sb, *lisp_str++);
      }
      lisp_str++;
      Token token = {.type = T_STRING, .string = sb.array};
      da_append(tokens, token);
    } break;

    case '!':
    case '#' ... '&':
    case '*' ... '/':
    case ':' ... '~': {
      StringBuilder sb;
      da_init(&sb);
      da_append(&sb, c);
      while (*lisp_str == '!' || (*lisp_str >= '#' && *lisp_str <= '&') ||
             (*lisp_str >= '*' && *lisp_str <= '~')) {
        da_append(&sb, *lisp_str++);
      }
      Token token = {.type = T_SYMBOL, .string = sb.array};
      da_append(tokens, token);
    } break;

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
