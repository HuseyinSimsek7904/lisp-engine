#include "parser.h"
#include "lisp_value.h"

#include <assert.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

    case '`': {
      Token token = {.type = T_SYMBOL, .symbol = "`"};
      da_append(tokens, token);
    } break;

    case '~': {
      Token token;
      if (sv_head(view) == '@') {
        sv_pop_left(&view);
        token = (Token){.type = T_SYMBOL, .symbol = "~@"};
      } else {
        token = (Token){.type = T_SYMBOL, .symbol = "~"};
      }
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
    case '<' ... '_':
    case 'a' ... '}': {
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

LispValue parse_next_node(Token **tokens, size_t *token_count) {
  if (!(*token_count)--)
    return lisp_make_error(lisp_make_list_1(lisp_make_sym(PARSER_REACHED_END)));

  Token token = *(*tokens)++;

  switch (token.type) {
  case T_L_PAREN: {
    LispGen gen;
    lisp_gen_init(&gen);

    while (*token_count) {
      if ((*tokens)->type == T_R_PAREN) {
        (*tokens)++;
        (*token_count)--;
        return lisp_gen_end(&gen);
      }

      LispValue parsed = parse_next_node(tokens, token_count);

      if (lisp_is_type(parsed, LISP_ERROR_T))
        return parsed;

      lisp_gen_appendr(&gen, parsed);
    }

    return lisp_make_error(
        lisp_make_list_1(lisp_make_sym(PARSER_TOO_FEW_R_PAREN)));

  } break;

  case T_R_PAREN:
    return lisp_make_error(
        lisp_make_list_1(lisp_make_sym(PARSER_TOO_MANY_R_PAREN)));

  case T_INT64:
    return lisp_make_int64(token.integer);

  case T_STRING:
    return lisp_make_str(token.string);

  case T_SYMBOL:
    if (!strcmp(token.symbol, "'")) {
      LispValue next_node = parse_next_node(tokens, token_count);

      if (lisp_is_type(next_node, LISP_ERROR_T))
        return lisp_make_error(
            lisp_make_list_1(lisp_make_sym(PARSER_QUOTE_END)));

      return lisp_make_list_2(lisp_make_sym("quote"), next_node);
    }

    if (!strcmp(token.symbol, "`")) {
      LispValue next_node = parse_next_node(tokens, token_count);

      if (lisp_is_type(next_node, LISP_ERROR_T))
        return lisp_make_error(
            lisp_make_list_1(lisp_make_sym(PARSER_BACKTICK_END)));

      return lisp_make_list_2(lisp_make_sym("quasi-quote"), next_node);
    }

    if (!strcmp(token.symbol, "~")) {
      LispValue next_node = parse_next_node(tokens, token_count);

      if (lisp_is_type(next_node, LISP_ERROR_T))
        return lisp_make_error(
            lisp_make_list_1(lisp_make_sym(PARSER_TILDE_END)));

      return lisp_make_list_2(lisp_make_sym("unquote"), next_node);
    }

    if (!strcmp(token.symbol, "~@")) {
      LispValue next_node = parse_next_node(tokens, token_count);

      if (lisp_is_type(next_node, LISP_ERROR_T))
        return lisp_make_error(
            lisp_make_list_1(lisp_make_sym(PARSER_TILDEAT_END)));

      return lisp_make_list_2(lisp_make_sym("unquote-expand"), next_node);
    }

    return lisp_make_sym(token.symbol);
  }

  fprintf(stderr, "reached unreachable\n");
  assert(false);
  return NULL;
}

LispValue parse(Token *tokens, size_t token_count) {
  LispGen gen;
  lisp_gen_init(&gen);

  while (token_count) {
    LispValue parsed = parse_next_node(&tokens, &token_count);

    if (lisp_is_type(parsed, LISP_ERROR_T))
      return parsed;

    lisp_gen_appendr(&gen, parsed);
  }

  return lisp_gen_end(&gen);
}
