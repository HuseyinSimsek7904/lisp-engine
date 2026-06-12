#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  T_AMPERSAND,
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

    case '&': {
      Token token = {.type = T_AMPERSAND};
      da_append(tokens, token);
    } break;

    case '!':
    case '#' ... '%':
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

// -- Lisp Value --

typedef enum {
  LISP_NIL,
  LISP_T,
  LISP_ERROR,
  LISP_STRING,
  LISP_INT64,
  LISP_SYMBOL,
  LISP_BUILTIN,
  LISP_BUILTIN_FUNC,
  LISP_FUNC,
  LISP_TUPLE,
  LISP_AMPERSAND_SYMBOL,
} LispType;

typedef struct LispValue {
  LispType type;
  union {
    char *string;
    char *error_message;
    char *symbol;
    int64_t integer;
    struct LispValue (*builtin)(struct LispValue *, size_t, void *);
    struct LispValue (*builtin_function)(struct LispValue *, size_t, void *);
    struct {
      char **args;
      size_t arg_count;
      struct LispValue *body;
      size_t body_size;
      char *variadic;
    } function;
    struct {
      struct LispValue *array;
      size_t size;
    } items;
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

  case LISP_T:
    printf("t");
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

  case LISP_BUILTIN:
    printf("<builtin>");
    break;

  case LISP_BUILTIN_FUNC:
    printf("<builtin_function>");
    break;

  case LISP_FUNC:
    printf("<function(");

    if (value.function.arg_count > 0)
      printf("%s", value.function.args[0]);

    for (size_t i = 1; i < value.function.arg_count; i++) {
      printf(" %s", value.function.args[i]);
    }

    if (value.function.variadic) {
      if (value.function.arg_count > 0)
        printf(" ");
      printf("& %s", value.function.variadic);
    }

    printf(")>");
    break;

  case LISP_TUPLE:
    printf("(");
    if (value.items.size > 0)
      print_lisp(value.items.array[0]);

    for (size_t i = 1; i < value.items.size; i++) {
      printf(" ");
      print_lisp(value.items.array[i]);
    }
    printf(")");
    break;

  default:
    printf("%d\n", value.type);
    TODO();
  }
}

bool identical_lisp(LispValue a, LispValue b) {
  if (a.type != b.type)
    return false;

  switch (a.type) {
  case LISP_NIL:
  case LISP_T:
    return true;

  case LISP_ERROR:
    return a.error_message == b.error_message;

  case LISP_INT64:
    return a.integer == b.integer;

  case LISP_STRING:
    return strcmp(a.string, b.string);

  case LISP_BUILTIN:
    return a.builtin == b.builtin;

  case LISP_BUILTIN_FUNC:
    return a.builtin_function == b.builtin_function;

  case LISP_TUPLE:
    if (a.items.size != b.items.size)
      return false;

    for (size_t i = 0; i < a.items.size; i++)
      if (!identical_lisp(a.items.array[i], b.items.array[i]))
        return false;

    return true;

  default:
    TODO();
    return false;
  }
}

#define BOOL_TO_LISP(x)                                                        \
  (LispValue) { .type = x ? LISP_T : LISP_NIL }
#define INT64_TO_LISP(x)                                                       \
  (LispValue) { .type = LISP_INT64, .integer = x }
#define ERROR_TO_LISP(x)                                                       \
  (LispValue) { .type = LISP_ERROR, .error_message = x }
#define STR_TO_LISP(x)                                                         \
  (LispValue) { .type = LISP_STRING, .string = x }
#define NIL_SINGLETON_LISP                                                     \
  (LispValue) { .type = LISP_NIL }
#define T_SINGLETON_LISP                                                       \
  (LispValue) { .type = LISP_T }

// -- END OF Lisp Value --

// -- Parser --

typedef enum {
  PARSER_SUCCESS = 0,
  PARSER_TOO_MANY_CLOSING,
  PARSER_TOO_MANY_OPENING,
  PARSER_UNKNOWN_TOKEN,
} ParseResult;

typedef struct {
  LispValues *array;
  size_t size;
  size_t capacity;
} LispValuesArray;

ParseResult parse(Token *tokens, size_t token_count, LispValues *nodes) {
  LispValuesArray stack;
  da_init(&stack);

  for (; token_count > 0; token_count--) {
    Token t = *tokens++;

    LispValue pn;
    switch (t.type) {
    case T_L_PAREN:
      da_append(&stack, (LispValues){});
      da_init(&stack.array[stack.size - 1]);
      continue;

    case T_R_PAREN:
      if (!stack.size) {
        da_free(&stack);
        return PARSER_TOO_MANY_CLOSING;
      }

      LispValues *new_nodes = &stack.array[stack.size - 1];
      pn = (LispValue){
          .type = LISP_TUPLE,
          .items = {.array = new_nodes->array, .size = new_nodes->size}};
      stack.size--;
      break;

    case T_INT64:
      pn = (LispValue){.type = LISP_INT64, .integer = t.integer};
      break;

    case T_STRING:
      pn = (LispValue){.type = LISP_STRING, .string = t.string};
      break;

    case T_SYMBOL:
      pn = (LispValue){.type = LISP_SYMBOL, .symbol = t.symbol};
      break;

    case T_AMPERSAND:
      pn = (LispValue){.type = LISP_AMPERSAND_SYMBOL};
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

// -- END OF Parser --

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

void env_free(Environment *env) {
  for (size_t i = 0; i < env->size; i++) {
    free(env->array[i].symbol);
  }

  da_free(env);
}

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
  EnvironmentEntry *entry = env_find_shallow(env, symbol);

  if (!entry) {
    EnvironmentEntry entry =
        (EnvironmentEntry){.symbol = strdup(symbol), .value = value};
    da_append(env, entry);
    return &env->array[env->size - 1];
  }

  entry->value = value;
  return entry;
}

// -- END OF Environment --

// -- Evaluate --

LispValue evaluate_lisp(LispValue lisp, Environment *env) {
  switch (lisp.type) {
  case LISP_STRING:
  case LISP_INT64:
    return lisp;

  case LISP_SYMBOL: {
    EnvironmentEntry *entry = env_find(env, lisp.symbol);

    if (entry)
      return entry->value;

    return ERROR_TO_LISP("undefined symbol");
  }

  case LISP_TUPLE:
    if (!lisp.items.size)
      return NIL_SINGLETON_LISP;

    LispValue callable = evaluate_lisp(lisp.items.array[0], env);

    switch (callable.type) {
    case LISP_ERROR:
      return callable;

    case LISP_BUILTIN:
      return callable.builtin(lisp.items.array + 1, lisp.items.size - 1, env);

    case LISP_BUILTIN_FUNC: {
      LispValue *args = calloc(lisp.items.size - 1, sizeof(LispValue));

      for (size_t i = 1; i < lisp.items.size; i++) {
        args[i - 1] = evaluate_lisp(lisp.items.array[i], env);
      }

      return callable.builtin_function(args, lisp.items.size - 1, env);
    }

    case LISP_FUNC: {
      LispValues args;
      da_init(&args);

      for (size_t i = 1; i < lisp.items.size; i++) {
        da_append(&args, evaluate_lisp(lisp.items.array[i], env));
      }

      if ((callable.function.variadic &&
           args.size < callable.function.arg_count) ||
          (!callable.function.variadic &&
           args.size != callable.function.arg_count)) {

        return ERROR_TO_LISP("incorrect number of parameters");
      }

      Environment new_env = {.other = env};
      da_init(&new_env);

      for (size_t i = 0; i < callable.function.arg_count; i++) {
        env_set(&new_env, callable.function.args[i], args.array[i]);
      }

      if (callable.function.variadic) {
        LispValue list = {
            .type = LISP_TUPLE,
            .items = {.array = calloc(args.size - callable.function.arg_count,
                                      sizeof(LispValue)),
                      .size = args.size - callable.function.arg_count}};

        memcpy(list.items.array, &args.array[callable.function.arg_count],
               sizeof(LispValue) * (args.size - callable.function.arg_count));

        env_set(&new_env, callable.function.variadic, list);
      }

      LispValue result = {.type = LISP_NIL};
      for (size_t i = 0; i < callable.function.body_size; i++) {
        result = evaluate_lisp(callable.function.body[i], &new_env);
      }

      env_free(&new_env);

      return result;
    }

    default:
      return ERROR_TO_LISP("tried to call non-callable");
    }

  default:
    TODO();
    return ERROR_TO_LISP("unknown node while trying to evaluate");
  }
}

LispValue repl(char *lisp, Environment *env) {
  Tokens tokens;
  da_init(&tokens);
  TokenizeResult tokenize_result = tokenize(lisp, &tokens);

  if (tokenize_result)
    return ERROR_TO_LISP("could not tokenize lisp");

  LispValues nodes;
  da_init(&nodes);

  ParseResult parser_result = parse(tokens.array, tokens.size, &nodes);
  da_free(&tokens);

  if (parser_result)
    return ERROR_TO_LISP("could not parse");

  LispValue return_value = {.type = LISP_NIL};
  for (size_t i = 0; i < nodes.size; i++) {
    return_value = evaluate_lisp(nodes.array[i], env);
  }

  return return_value;
}

// -- END OF Evaluate --

LispValue builtin_set(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`set` requires at least 2 arguments");

  if (args[0].type != LISP_SYMBOL)
    return ERROR_TO_LISP("first argument to `set` must be a symbol");

  char *symbol = args[0].symbol;
  LispValue value = evaluate_lisp(args[1], env);

  env_set(env, symbol, value);

  return value;
}

LispValue builtin_let(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`let` requires at least 2 arguments");

  LispValue defs_node = args[0];

  if (defs_node.type != LISP_TUPLE)
    return ERROR_TO_LISP("first argument to `let` must be a tuple");

  Environment new_env = {.other = env};
  da_init(&new_env);

  for (size_t i = 0; i < defs_node.items.size; i++) {
    LispValue def_node = defs_node.items.array[i];

    if (def_node.type != LISP_TUPLE)
      return ERROR_TO_LISP("first argument to `let` must be a tuple of tuples");

    if (def_node.items.size != 2)
      return ERROR_TO_LISP(
          "first argument to `let` must be a tuple of tuples with sizes 2");

    if (def_node.items.array[0].type != LISP_SYMBOL)
      return ERROR_TO_LISP("first argument to `let` must be a tuple of tuples "
                           "with first element being a symbol");

    char *symbol = def_node.items.array[0].symbol;

    if (env_find_shallow(&new_env, symbol))
      return ERROR_TO_LISP("redefined symbol");

    // TODO: Maybe the passed environment should be the new environment.
    // That way one variable defined can be used in the other one.
    LispValue value = evaluate_lisp(def_node.items.array[1], env);

    env_set(&new_env, symbol, value);
  }

  LispValue return_value = {.type = LISP_NIL};
  for (size_t i = 1; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], &new_env);
  }

  env_free(&new_env);

  return return_value;
}

LispValue builtin_if(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`if` requires at least 2 arguments");

  LispValue condition = evaluate_lisp(args[0], env);

  if (condition.type == LISP_ERROR)
    return condition;

  if (condition.type != LISP_NIL)
    return evaluate_lisp(args[1], env);

  LispValue result = {.type = LISP_NIL};
  for (size_t i = 2; i < arg_count; i++) {
    result = evaluate_lisp(args[i], env);
  }
  return result;
}

LispValue builtin_fn(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`fn` requires at least 2 arguments");

  if (args[0].type != LISP_TUPLE)
    return ERROR_TO_LISP("first argument to `fn` must be a tuple");

  char *variadic = NULL;
  char **new_args = malloc(sizeof(char *) * arg_count);
  for (size_t i = 0; i < args[0].items.size; i++) {
    if (args[0].items.array[i].type == LISP_AMPERSAND_SYMBOL) {
      if (i != args[0].items.size - 2)
        return ERROR_TO_LISP("exactly one symbol expected after `&`");

      if (args[0].items.array[args[0].items.size - 1].type != LISP_SYMBOL)
        return ERROR_TO_LISP("symbol expected after `&`");

      variadic = args[0].items.array[args[0].items.size - 1].symbol;
      for (size_t j = 0; j < i; j++)
        if (strcmp(new_args[j], variadic) == 0)
          return ERROR_TO_LISP("repeated argument");

      break;
    }

    if (args[0].items.array[i].type != LISP_SYMBOL)
      return ERROR_TO_LISP("first argument to `fn` must be a tuple of symbols");

    for (size_t j = 0; j < i; j++) {
      if (strcmp(new_args[j], args[0].items.array[i].symbol) == 0)
        return ERROR_TO_LISP("repeated argument");
    }

    new_args[i] = args[0].items.array[i].symbol;
  }

  return (LispValue){
      .type = LISP_FUNC,
      .function = {.args = new_args,
                   .arg_count = args[0].items.size - (variadic ? 2 : 0),
                   .body = args + 1,
                   .body_size = arg_count - 1,
                   .variadic = variadic}};
}

LispValue builtin_do(LispValue *args, size_t arg_count, Environment *env) {
  LispValue result = {.type = LISP_NIL};
  for (size_t i = 0; i < arg_count; i++) {
    result = evaluate_lisp(args[i], env);

    if (result.type == LISP_ERROR)
      return result;
  }
  return result;
}

LispValue builtin_print(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i].type != LISP_STRING)
      return ERROR_TO_LISP("invalid type for argument to `print`");

  for (size_t i = 0; i < arg_count; i++) {
    printf("%s\n", args[i].string);
  }

  return NIL_SINGLETON_LISP;
}

LispValue builtin_sum(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count; i++)
    if (args[i].type != LISP_INT64)
      return ERROR_TO_LISP("invalid type for argument to `+`");

  int64_t sum = 0;
  for (size_t i = 0; i < arg_count; i++) {
    sum += args[i].integer;
  }

  return INT64_TO_LISP(sum);
}

LispValue builtin_neg_or_sub(LispValue *args, size_t arg_count,
                             Environment *env) {
  if (arg_count < 1 || arg_count > 2)
    return ERROR_TO_LISP("`-` expectes either 1 or 2 arguments");

  for (size_t i = 0; i < arg_count; i++)
    if (args[i].type != LISP_INT64)
      return ERROR_TO_LISP("invalid type for argument to `-`");

  return INT64_TO_LISP(arg_count == 1 ? -args[0].integer
                                      : args[0].integer - args[1].integer);
}

LispValue builtin_list(LispValue *args, size_t arg_count, Environment *env) {
  return (LispValue){.type = LISP_TUPLE,
                     .items = {.array = args, .size = arg_count}};
}

LispValue builtin_equal(LispValue *args, size_t arg_count, Environment *env) {
  // NOTE: Later, we can change this function to accept any number of arguments.
  if (arg_count != 2)
    return ERROR_TO_LISP("`=` expects 2 arguments");

  return BOOL_TO_LISP(identical_lisp(args[0], args[1]));
}

LispValue builtin_less_than(LispValue *args, size_t arg_count,
                            Environment *env) {
  // NOTE: Later, we can change this function to accept any number of arguments.
  if (arg_count != 2)
    return ERROR_TO_LISP("`<` expects 2 arguments");

  if (args[0].type != LISP_INT64 || args[1].type != LISP_INT64)
    return ERROR_TO_LISP("`<` expects 2 integers");

  return BOOL_TO_LISP(args[0].integer < args[1].integer);
}

LispValue builtin_include(LispValue *args, size_t arg_count, Environment *env) {
  LispValue return_value = {.type = LISP_NIL};

  for (size_t i = 0; i < arg_count; i++) {
    if (args[i].type != LISP_STRING)
      return ERROR_TO_LISP("`include` expects strings");

    FILE *file = fopen(args[i].string, "r");
    if (!file)
      return ERROR_TO_LISP("could not open file");

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
      return ERROR_TO_LISP("could not allocate buffer");

    if (fread(buffer, 1, size, file) != size)
      return ERROR_TO_LISP("could not read the whole file");

    buffer[size] = '\0';
    return_value = repl(buffer, env);
    if (return_value.type == LISP_ERROR)
      return return_value;
  }

  return return_value;
}

int main(void) {
  Environment global_env = {.other = NULL};
  da_init(&global_env);

  env_set(&global_env, "nil", NIL_SINGLETON_LISP);

  env_set(&global_env, "t", T_SINGLETON_LISP);

  env_set(&global_env, "print",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(
                          LispValue *, size_t, void *))builtin_print});

  env_set(&global_env, "+",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(LispValue *, size_t,
                                                         void *))builtin_sum});

  env_set(&global_env, "-",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(
                          LispValue *, size_t, void *))builtin_neg_or_sub});

  env_set(&global_env, "let",
          (LispValue){.type = LISP_BUILTIN,
                      .builtin = (LispValue (*)(LispValue *, size_t,
                                                void *))builtin_let});

  env_set(&global_env, "set",
          (LispValue){.type = LISP_BUILTIN,
                      .builtin = (LispValue (*)(LispValue *, size_t,
                                                void *))builtin_set});

  env_set(&global_env, "if",
          (LispValue){.type = LISP_BUILTIN,
                      .builtin = (LispValue (*)(LispValue *, size_t,
                                                void *))builtin_if});

  env_set(&global_env, "do",
          (LispValue){.type = LISP_BUILTIN,
                      .builtin = (LispValue (*)(LispValue *, size_t,
                                                void *))builtin_do});

  env_set(&global_env, "fn",
          (LispValue){.type = LISP_BUILTIN,
                      .builtin = (LispValue (*)(LispValue *, size_t,
                                                void *))builtin_fn});

  env_set(&global_env, "list",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(LispValue *, size_t,
                                                         void *))builtin_list});

  env_set(&global_env, "=",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(
                          LispValue *, size_t, void *))builtin_equal});

  env_set(&global_env, "<",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(
                          LispValue *, size_t, void *))builtin_less_than});

  env_set(&global_env, "include",
          (LispValue){.type = LISP_BUILTIN_FUNC,
                      .builtin_function = (LispValue (*)(
                          LispValue *, size_t, void *))builtin_include});

  while (true) {
    char *lisp = readline("> ");

    if (!lisp)
      exit(0);

    LispValue return_value = repl(lisp, &global_env);

    free(lisp);

    print_lisp(return_value);
    printf("\n");
  }
}
