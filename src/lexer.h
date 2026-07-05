#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdint.h>

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
    int64_t integer;
    char *string;
    char *symbol;
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
} LexerResult;

LexerResult generate_tokens(char *lisp_str, Tokens *tokens);

#endif
