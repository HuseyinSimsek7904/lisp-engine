#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "lisp_value.h"

LispValue parse(Token *tokens, size_t token_count);

#endif
