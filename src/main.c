#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include <readline/chardefs.h>
#include <readline/readline.h>

// -- Dynamic Array --
#define DA_INIT_CAP 0x100
#define da_item_size(da) (sizeof((da)->array[0]))
#define da_avail(da) ((da)->capacity - (da)->size)
#define da_resize(da)                                                          \
  (da)->array = reallocarray((da)->array, (da)->capacity, da_item_size(da));
#define da_init(da)                                                            \
  do {                                                                         \
    (da)->array = calloc(DA_INIT_CAP, da_item_size(da));                       \
    (da)->capacity = DA_INIT_CAP;                                              \
    (da)->size = 0;                                                            \
  } while (0)
#define da_free(da) free((da)->array)
#define da_append(da, item)                                                    \
  do {                                                                         \
    if (!da_avail(da)) {                                                       \
      (da)->capacity *= 2;                                                     \
      da_resize(da);                                                           \
    }                                                                          \
    (da)->array[(da)->size++] = (item);                                        \
  } while (0)
#define da_append_many(da, items, n)                                           \
  do {                                                                         \
    while (n - da_avail(da) > 0) {                                             \
      (da)->capacity *= 2;                                                     \
      da_resize(da);                                                           \
    }                                                                          \
    memcpy(&(da)->array[(da)->size], items, da_item_size(da) * n);             \
    (da)->size += n;                                                           \
  } while (0)
#define da_pop(da) ((da)->array[--(da)->size])
// -- END OF Dynamic Array --

#define TODO()                                                                 \
  do {                                                                         \
    fprintf(stderr, __FILE__ ":%u: not implemented yet\n", __LINE__);          \
  } while (0)

// -- Tokenizer --

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

typedef struct {
  char *array;
  size_t size;
  size_t capacity;
} StringBuilder;

typedef enum {
  TOKENIZE_SUCCESS = 0,
  TOKENIZE_UNTERMINATED_STRING,
  TOKENIZE_UNKNOWN_CHAR,
} TokenizeResult;

TokenizeResult tokenize(char *lisp_str, Tokens *tokens) {
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
          return TOKENIZE_UNTERMINATED_STRING;

        da_append(&sb, *lisp_str++);
      }
      lisp_str++;
      Token token = {.type = T_STRING, .string = sb.array};
      da_append(tokens, token);
    } break;

    case 'a' ... 'z':
    case 'A' ... 'Z': {
      // TODO: This does not accept some tokens as symbols even though they
      // should be accepted.
      StringBuilder sb;
      da_init(&sb);
      da_append(&sb, c);
      while (*lisp_str >= '!' && *lisp_str <= '~' && *lisp_str != '(' &&
             *lisp_str != ')') {
        da_append(&sb, *lisp_str++);
      }
      Token token = {.type = T_SYMBOL, .string = sb.array};
      da_append(tokens, token);
    } break;

    default:
      TODO();
      return TOKENIZE_UNKNOWN_CHAR;
    }
  }

  return TOKENIZE_SUCCESS;
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

// -- END OF Tokenizer --

// -- Parser --

typedef enum {
  PN_INT64,
  PN_STRING,
  PN_SYMBOL,
  PN_TUPLE,
} ParserNodeType;

typedef struct ParserNode {
  ParserNodeType type;
  union {
    int64_t integer;
    char *string;
    char *symbol;
    struct {
      struct ParserNode *items;
      size_t size;
    };
  };
} ParserNode;

typedef struct {
  ParserNode *array;
  size_t capacity;
  size_t size;
} ParserNodes;

typedef struct {
  ParserNodes *array;
  size_t capacity;
  size_t size;
} ParserNodesArray;

typedef enum {
  PARSER_SUCCESS = 0,
  PARSER_TOO_MANY_CLOSING,
  PARSER_TOO_MANY_OPENING,
  PARSER_UNKNOWN_TOKEN,
} ParseResult;

ParseResult parse(Token *tokens, size_t token_count, ParserNodes *nodes) {
  ParserNodesArray stack;
  da_init(&stack);

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    ParserNode pn;
    switch (t.type) {
    case T_L_PAREN:
      da_append(&stack, (ParserNodes){});
      da_init(&stack.array[stack.size - 1]);
      continue;

    case T_R_PAREN:
      if (!stack.size)
        return PARSER_TOO_MANY_CLOSING;

      ParserNodes *new_nodes = &stack.array[stack.size - 1];
      pn = (ParserNode){
          .type = PN_TUPLE, .items = new_nodes->array, .size = new_nodes->size};
      stack.size--;
      break;

    case T_INT64:
      pn = (ParserNode){.type = PN_INT64, .integer = t.integer};
      break;

    case T_STRING:
      pn = (ParserNode){.type = PN_STRING, .string = t.string};
      break;

    case T_SYMBOL:
      pn = (ParserNode){.type = PN_SYMBOL, .symbol = t.symbol};
      break;

    default:
      TODO();
      return PARSER_UNKNOWN_TOKEN;
    }

    if (stack.size) {
      da_append(&stack.array[stack.size - 1], pn);
    } else {
      da_append(nodes, pn);
    }
  }

  if (stack.size)
    return PARSER_TOO_MANY_OPENING;

  return PARSER_SUCCESS;
}

void print_parser_nodes(ParserNode *nodes, size_t n) {
  for (size_t i = 0; i < n; i++) {
    ParserNode pn = nodes[i];

    switch (pn.type) {
    case PN_INT64:
      printf("%lu", pn.integer);
      break;

    case PN_STRING:
      printf("\"%s\"", pn.string);
      break;

    case PN_SYMBOL:
      printf("%s", pn.symbol);
      break;

    case PN_TUPLE:
      printf("(");
      print_parser_nodes(pn.items, pn.size);
      printf(")");
      break;

    default:
      TODO();
      break;
    }

    if (i < n - 1)
      printf(" ");
  }
}

// -- END OF Parser --

int main(void) {
  while (true) {
    char *lisp = readline("> ");

    Tokens tokens;
    da_init(&tokens);
    TokenizeResult tokenize_result = tokenize(lisp, &tokens);

    free(lisp);

    if (tokenize_result) {
      fprintf(stderr, "could not tokenize lisp\n");
      continue;
    }

    ParserNodes nodes;
    da_init(&nodes);

    ParseResult parser_result = parse(tokens.array, tokens.size, &nodes);

    da_free(&tokens);

    if (parser_result) {
      fprintf(stderr, "could not parse lisp\n");
      continue;
    }

    print_parser_nodes(nodes.array, nodes.size);
    printf("\n");
  }
}
