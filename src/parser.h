#ifndef PARSER_H
#define PARSER_H

#include "lisp_value.h"

typedef enum {
  T_L_PAREN,
  T_R_PAREN,
  T_INT64,
  T_STRING,
  T_SYMBOL,
} TokenType;

typedef struct {
  TokenType type;
  union {
    StringView string;
    int64_t integer;
    const char *symbol;
  };
} Token;

typedef struct {
  Token *array;
  size_t size;
  size_t capacity;
} Tokens;

typedef enum {
  LEXER_SUCCESS = 0,
  LEXER_UNTERMINATED_STRING,
  LEXER_UNKNOWN_CHAR,
  LEXER_EXPECTED_SEPARATOR,
  LEXER_INVALID_ESCAPE,
  LEXER_INTEGER_TOO_LARGE,
} LexerResult;

LexerResult generate_tokens(StringView lisp_str, Tokens *tokens);
LispValue parse(Token *tokens, size_t token_count);

#endif
