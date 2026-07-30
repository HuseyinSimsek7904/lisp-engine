#include "parser.h"
#include "lexer.h"
#include "lisp_value.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
