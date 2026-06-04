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

bool is_symbol_char(char c) {
  return c == '!' || (c >= '#' && c <= '&') || (c >= '*' && c <= '/') ||
         (c >= ':' && c <= '~');
}

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

    case '!':
    case '#' ... '&':
    case '*' ... '/':
    case ':' ... '~': {
      StringBuilder sb;
      da_init(&sb);
      da_append(&sb, c);
      while (is_symbol_char(*lisp_str)) {
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
      if (!stack.size) {
        da_free(&stack);
        return PARSER_TOO_MANY_CLOSING;
      }

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
      da_free(&stack);
      return PARSER_UNKNOWN_TOKEN;
    }

    if (stack.size) {
      da_append(&stack.array[stack.size - 1], pn);
    } else {
      da_append(nodes, pn);
    }
  }

  da_free(&stack);

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

// -- Lisp Value --

typedef enum {
  LISP_NIL,
  LISP_ERROR,
  LISP_STRING,
  LISP_INT64,
  LISP_SYMBOL,
  LISP_BUILTIN,
  LISP_BUILTIN_FUNC,
} LispType;

typedef struct LispValue {
  LispType type;
  union {
    char *string;
    char *error_message;
    char *symbol;
    int64_t integer;
    struct LispValue (*builtin)(ParserNode *, size_t, void *);
    struct LispValue (*builtin_function)(struct LispValue *, size_t, void *);
  };
} LispValue;

typedef struct {
  LispValue *array;
  size_t size;
  size_t capacity;
} LispValues;

void print_lisp(LispValue value) {
  switch (value.type) {
  case LISP_NIL:
    printf("NIL");
    break;

  case LISP_ERROR:
    printf("lisp error: %s\n", value.error_message);
    break;

  case LISP_STRING:
    printf("\"%s\"", value.string);
    break;

  case LISP_SYMBOL:
    printf("'%s", value.symbol);
    break;

  case LISP_INT64:
    printf("%ld", value.integer);
    break;

  default:
    TODO();
  }
}

// -- END OF Lisp Value --

// -- Environment --

typedef struct {
  char *symbol;
  LispValue value;
} EnvironmentEntry;

typedef struct Environment {
  EnvironmentEntry *array;
  size_t size;
  size_t capacity;
  struct Environment *other;
} Environment;

EnvironmentEntry *env_find_shallow(Environment *env, char *symbol) {
  for (size_t i = 0; i < env->size; i++) {
    if (strcmp(env->array[i].symbol, symbol) == 0)
      return &env->array[i];
  }

  return NULL;
}

EnvironmentEntry *env_find(Environment *env, char *symbol) {
  EnvironmentEntry *search = env_find_shallow(env, symbol);

  if (search)
    return search;

  if (!env->other)
    return NULL;

  return env_find(env->other, symbol);
}

EnvironmentEntry *env_set(Environment *env, char *symbol, LispValue value) {
  EnvironmentEntry *entry = env_find(env, symbol);

  if (!entry) {
    EnvironmentEntry entry =
        (EnvironmentEntry){.symbol = symbol, .value = value};
    da_append(env, entry);
    return &env->array[env->size - 1];
  }

  entry->value = value;
  return entry;
}

// -- END OF Environment --

// -- Evaluate --

LispValue evaluate_lisp(ParserNode node, Environment *env) {
  switch (node.type) {
  case PN_STRING:
    return (LispValue){.type = LISP_STRING, .string = node.string};

  case PN_INT64:
    return (LispValue){.type = LISP_INT64, .integer = node.integer};

  case PN_SYMBOL: {
    EnvironmentEntry *entry = env_find(env, node.symbol);
    free(node.symbol);

    if (entry)
      return entry->value;

    return (LispValue){.type = LISP_ERROR, .error_message = "undefined symbol"};
  }

  case PN_TUPLE:
    if (!node.size)
      return (LispValue){.type = LISP_NIL};

    LispValue callable = evaluate_lisp(node.items[0], env);

    if (callable.type == LISP_BUILTIN_FUNC) {
      LispValues args;
      da_init(&args);

      for (size_t i = 1; i < node.size; i++) {
        da_append(&args, evaluate_lisp(node.items[i], env));
      }

      free(node.items);

      LispValue result = callable.builtin_function(args.array, args.size, env);
      da_free(&args);

      return result;
    } else if (callable.type == LISP_BUILTIN) {
      LispValue result = callable.builtin(node.items + 1, node.size - 1, env);
      free(node.items);

      return result;
    }

    return (LispValue){.type = LISP_ERROR,
                       .error_message = "tried to call non-callable"};

  default:
    TODO();
    return (LispValue){.type = LISP_ERROR,
                       .error_message =
                           "unknown parser node while trying to evaluate"};
  }

  // dummy return
  return (LispValue){.type = LISP_NIL};
}

// -- END OF Evaluate --

LispValue builtin_let(ParserNode *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return (LispValue){.type = LISP_ERROR,
                       .error_message = "`let` requires at least 2 arguments"};

  ParserNode defs_node = args[0];

  if (defs_node.type != PN_TUPLE)
    return (LispValue){.type = LISP_ERROR,
                       .error_message =
                           "first argument to `let` must be a tuple"};

  Environment new_env = {.other = env};
  da_init(&new_env);

  for (size_t i = 0; i < defs_node.size; i++) {
    ParserNode def_node = defs_node.items[i];

    if (def_node.type != PN_TUPLE)
      return (LispValue){
          .type = LISP_ERROR,
          .error_message = "first argument to `let` must be a tuple of tuples"};

    if (def_node.size != 2)
      return (LispValue){
          .type = LISP_ERROR,
          .error_message =
              "first argument to `let` must be a tuple of tuples with sizes 2"};

    if (def_node.items[0].type != PN_SYMBOL)
      return (LispValue){.type = LISP_ERROR,
                         .error_message =
                             "first argument to `let` must be a tuple of "
                             "tuples with first element being a symbol"};

    char *symbol = def_node.items[0].symbol;

    if (env_find_shallow(&new_env, symbol))
      return (LispValue){.type = LISP_ERROR,
                         .error_message = "redefined symbol"};

    // TODO: Maybe the passed environment should be the new environment.
    // That way one variable defined can be used in the other one.
    LispValue value = evaluate_lisp(def_node.items[1], env);

    EnvironmentEntry entry =
        (EnvironmentEntry){.symbol = symbol, .value = value};
    da_append(&new_env, entry);
  }

  LispValue return_value = {.type = LISP_NIL};
  for (size_t i = 1; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], &new_env);
  }

  return return_value;
}

LispValue builtin_print(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i].type != LISP_STRING)
      return (LispValue){.type = LISP_ERROR,
                         .error_message = "invalid type for argument to print"};

  for (size_t i = 0; i < arg_count; i++) {
    printf("%s\n", args[i].string);
    free(args[i].string);
  }

  return (LispValue){.type = LISP_NIL};
}

LispValue builtin_sum(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i].type != LISP_INT64)
      return (LispValue){.type = LISP_ERROR,
                         .error_message = "invalid type for argument to print"};

  int64_t sum = 0;
  for (size_t i = 0; i < arg_count; i++) {
    sum += args[i].integer;
  }

  return (LispValue){.type = LISP_INT64, .integer = sum};
}

int main(void) {
  while (true) {
    char *lisp = readline("> ");

    if (!lisp)
      exit(0);

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

    Environment global_env = {.other = NULL};
    da_init(&global_env);

    env_set(&global_env, "print",
            (LispValue){.type = LISP_BUILTIN_FUNC,
                        .builtin_function = (LispValue (*)(
                            LispValue *, size_t, void *))builtin_print});

    env_set(&global_env, "+",
            (LispValue){.type = LISP_BUILTIN_FUNC,
                        .builtin_function = (LispValue (*)(
                            LispValue *, size_t, void *))builtin_sum});

    env_set(&global_env, "let",
            (LispValue){.type = LISP_BUILTIN,
                        .builtin = (LispValue (*)(ParserNode *, size_t,
                                                  void *))builtin_let});

    LispValue return_value;
    for (size_t i = 0; i < nodes.size; i++) {
      return_value = evaluate_lisp(nodes.array[i], &global_env);
    }

    print_lisp(return_value);
    printf("\n");
  }
}
