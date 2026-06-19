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
    if (n > da_avail(da)) {                                                    \
      while (n > da_avail(da)) {                                               \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
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

// -- Lisp Value --

typedef enum {
  LISP_NIL = 0,
  LISP_T,
  LISP_ERROR,
  LISP_STRING,
  LISP_INT64,
  LISP_SYMBOL,
  LISP_BUILTIN_FUNC,
  LISP_FUNC,
  LISP_TUPLE,
  LISP_UNPACKED,
} LispType;

typedef struct {
  bool is_macro;
  bool tail_call;
  struct LispValue (*callback)(struct LispValue *, size_t, void *);
} _LispBuiltinFunction;

typedef struct {
  bool is_macro;
  char **args;
  size_t arg_count;
  struct LispValue *body;
  size_t body_size;
  char *variadic;
} _LispCustomFunction;

typedef struct {
  struct LispValue *array;
  size_t size;
} _LispView;

typedef struct LispValue {
  LispType type;
  union {
    char *string;
    char *error_message;
    char *symbol;
    int64_t integer;
    _LispBuiltinFunction builtin_function;
    _LispCustomFunction custom_function;
    _LispView items;
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
    printf("<error(%s)>", value.error_message);
    break;

  case LISP_STRING:
    printf("\"%s\"", value.string);
    break;

  case LISP_SYMBOL:
    printf("%s", value.symbol);
    break;

  case LISP_INT64:
    printf("%ld", value.integer);
    break;

  case LISP_BUILTIN_FUNC:
    printf(value.builtin_function.is_macro ? "<builtin-macro>"
                                           : "<builtin-function>");
    break;

  case LISP_FUNC:
    printf(value.custom_function.is_macro ? "<macro(" : "<function(");

    if (value.custom_function.arg_count > 0)
      printf("%s", value.custom_function.args[0]);

    for (size_t i = 1; i < value.custom_function.arg_count; i++) {
      printf(" %s", value.custom_function.args[i]);
    }

    if (value.custom_function.variadic) {
      if (value.custom_function.arg_count > 0)
        printf(" ");
      printf("(rest %s)", value.custom_function.variadic);
    }

    printf(")>");
    break;

  case LISP_UNPACKED:
    printf("unpacked");

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

  case LISP_BUILTIN_FUNC:
    return a.builtin_function.is_macro == b.builtin_function.is_macro &&
           a.builtin_function.callback == b.builtin_function.callback;

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

// -- Utils --

char *read_file(char *file_name) {
  FILE *file = fopen(file_name, "r");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(size + 1);
  if (!buffer)
    return NULL;

  if (fread(buffer, 1, size, file) != size)
    return NULL;

  buffer[size] = '\0';
  return buffer;
}

// -- END OF Utils --

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

void roll_env_back(Environment *env, Environment *original) {
  while (env != original && env->other) {
    env_free(env);
    Environment *other = env->other;
    free(env);
    env = other;
  }
}

LispValue evaluate_lisp(LispValue lisp, Environment *env) {
  Environment *original = env;

  do {
    switch (lisp.type) {
    case LISP_STRING:
    case LISP_INT64:
    case LISP_NIL:
    case LISP_T:
    case LISP_BUILTIN_FUNC:
    case LISP_FUNC:
    case LISP_ERROR:
      roll_env_back(env, original);
      return lisp;

    case LISP_SYMBOL: {
      EnvironmentEntry *entry = env_find(env, lisp.symbol);

      if (entry) {
        roll_env_back(env, original);
        return entry->value;
      }

      roll_env_back(env, original);
      return ERROR_TO_LISP("undefined symbol");
    }

    case LISP_TUPLE:
      if (!lisp.items.size) {
        roll_env_back(env, original);
        return NIL_SINGLETON_LISP;
      }

      LispValue callable = evaluate_lisp(lisp.items.array[0], env);

      switch (callable.type) {
      case LISP_ERROR:
        roll_env_back(env, original);
        return callable;

      case LISP_BUILTIN_FUNC: {
        if (callable.builtin_function.is_macro) {
          lisp = callable.builtin_function.callback(lisp.items.array + 1,
                                                    lisp.items.size - 1, env);
        } else {
          LispValues args;
          da_init(&args);

          for (size_t i = 1; i < lisp.items.size; i++) {
            LispValue arg = evaluate_lisp(lisp.items.array[i], env);
            if (arg.type == LISP_UNPACKED)
              da_append_many(&args, arg.items.array, arg.items.size);
            else
              da_append(&args, arg);
          }

          lisp = callable.builtin_function.callback(args.array, args.size, env);
        }

        if (!callable.builtin_function.tail_call) {
          roll_env_back(env, original);
          return lisp;
        }
      } break;

      case LISP_FUNC: {
        LispValues args;
        da_init(&args);

        if (callable.custom_function.is_macro) {
          for (size_t i = 1; i < lisp.items.size; i++)
            da_append(&args, lisp.items.array[i]);
        } else {
          for (size_t i = 1; i < lisp.items.size; i++) {
            LispValue arg = evaluate_lisp(lisp.items.array[i], env);
            if (arg.type == LISP_UNPACKED)
              da_append_many(&args, arg.items.array, arg.items.size);
            else
              da_append(&args, arg);
          }
        }

        if ((callable.custom_function.variadic &&
             args.size < callable.custom_function.arg_count) ||
            (!callable.custom_function.variadic &&
             args.size != callable.custom_function.arg_count)) {

          roll_env_back(env, original);
          return ERROR_TO_LISP("incorrect number of parameters");
        }

        Environment *new_env = malloc(sizeof(Environment));
        *new_env = (Environment){.other = env};
        da_init(new_env);

        for (size_t i = 0; i < callable.custom_function.arg_count; i++) {
          env_set(new_env, callable.custom_function.args[i], args.array[i]);
        }

        if (callable.custom_function.variadic) {
          LispValue list = {
              .type = LISP_TUPLE,
              .items = {
                  .array =
                      calloc(args.size - callable.custom_function.arg_count,
                             sizeof(LispValue)),
                  .size = args.size - callable.custom_function.arg_count}};

          memcpy(list.items.array,
                 &args.array[callable.custom_function.arg_count],
                 sizeof(LispValue) *
                     (args.size - callable.custom_function.arg_count));

          env_set(new_env, callable.custom_function.variadic, list);
        }

        assert(callable.custom_function.body_size > 0);

        for (size_t i = 0; i < callable.custom_function.body_size - 1; i++) {
          evaluate_lisp(callable.custom_function.body[i], new_env);
        }

        lisp = callable.custom_function
                   .body[callable.custom_function.body_size - 1];
        env = new_env;
      } break;

      default:
        roll_env_back(env, original);
        return ERROR_TO_LISP("tried to call non-callable");
      }
      break;

    default:
      TODO();
      roll_env_back(env, original);
      return ERROR_TO_LISP("unknown node while trying to evaluate");
    }
  } while (true);
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

LispValue builtin_setg(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`setg` requires at least 2 arguments");

  if (args[0].type != LISP_SYMBOL)
    return ERROR_TO_LISP("first argument to `setg` must be a symbol");

  char *symbol = args[0].symbol;
  LispValue value = evaluate_lisp(args[1], env);

  Environment *global = env;
  while (global->other)
    global = global->other;

  env_set(global, symbol, value);

  return value;
}

// TODO: `let` should be able to do tail calls, but tail calls cannot use new
// environments currently.
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
    return args[1];

  for (size_t i = 2; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }
  return args[arg_count - 1];
}

LispValue define_fn_or_macro(LispValue *args, size_t arg_count,
                             Environment *env, bool is_macro) {
  if (arg_count < 2)
    return ERROR_TO_LISP("`fn` requires at least 2 arguments");

  if (args[0].type != LISP_TUPLE)
    return ERROR_TO_LISP("first argument to `fn` must be a tuple");

  char *variadic = NULL;
  char **new_args = malloc(sizeof(char *) * arg_count);
  for (size_t i = 0; i < args[0].items.size; i++) {
    LispValue arg = args[0].items.array[i];

    if (arg.type == LISP_SYMBOL) {
      if (args[0].items.array[i].type != LISP_SYMBOL)
        return ERROR_TO_LISP("`fn` argument list must be a tuple of symbols");

      for (size_t j = 0; j < i; j++) {
        if (strcmp(new_args[j], arg.symbol) == 0)
          return ERROR_TO_LISP("repeated argument");
      }

      new_args[i] = arg.symbol;

    } else if (arg.type == LISP_TUPLE) {
      if (i != args[0].items.size - 1) {
        TODO();
        return ERROR_TO_LISP(
            "only the last item in `fn` arguments list can be a tuple");
      }

      if (arg.items.size != 2)
        return ERROR_TO_LISP(
            "tuple inside `fn` arguments list must be of size 2");

      if (arg.items.array[0].type != LISP_SYMBOL ||
          strcmp(arg.items.array[0].string, "rest"))
        return ERROR_TO_LISP("first item in the tuple in the `fn` arguments "
                             "list must be a `rest` symbol");

      if (arg.items.array[1].type != LISP_SYMBOL)
        return ERROR_TO_LISP("`rest` argument must be a symbol");

      assert(!variadic);

      variadic = arg.items.array[1].symbol;
      for (size_t j = 0; j < i; j++)
        if (strcmp(new_args[j], variadic) == 0)
          return ERROR_TO_LISP("repeated `rest` argument");
    }
  }

  return (LispValue){
      .type = LISP_FUNC,
      .custom_function = {.is_macro = is_macro,
                          .args = new_args,
                          .arg_count = args[0].items.size - (variadic ? 1 : 0),
                          .body = args + 1,
                          .body_size = arg_count - 1,
                          .variadic = variadic}};
}

LispValue builtin_fn(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, false);
}

LispValue builtin_macro(LispValue *args, size_t arg_count, Environment *env) {
  return define_fn_or_macro(args, arg_count, env, true);
}

// TODO: Inside `do`, if any of the items return error, the control flow should
// be stopped and the error value should be returned.
LispValue builtin_do(LispValue *args, size_t arg_count, Environment *env) {
  for (size_t i = 0; i < arg_count - 1; i++) {
    evaluate_lisp(args[i], env);
  }

  return args[arg_count - 1];
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

LispValue builtin_read_file(LispValue *args, size_t arg_count,
                            Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`read-file` expects 1 arguments");

  if (args[0].type != LISP_STRING)
    return ERROR_TO_LISP("`read-file` expects strings");

  char *buffer = read_file(args[0].string);

  if (!buffer)
    return ERROR_TO_LISP("could not read file contents");

  return STR_TO_LISP(buffer);
}

LispValue builtin_parse_lisp(LispValue *args, size_t arg_count,
                             Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`read-file` expects 1 arguments");

  if (args[0].type != LISP_STRING)
    return ERROR_TO_LISP("`read-file` expects strings");

  Tokens tokens;
  da_init(&tokens);
  TokenizeResult tokenize_result = tokenize(args[0].string, &tokens);

  if (tokenize_result)
    return ERROR_TO_LISP("could not tokenize lisp");

  LispValues nodes;
  da_init(&nodes);

  ParseResult parser_result = parse(tokens.array, tokens.size, &nodes);
  da_free(&tokens);

  if (parser_result)
    return ERROR_TO_LISP("could not parse");

  return (LispValue){.type = LISP_TUPLE,
                     .items = {.array = nodes.array, .size = nodes.size}};
}

LispValue builtin_eval(LispValue *args, size_t arg_count, Environment *env) {
  LispValue return_value = NIL_SINGLETON_LISP;
  for (size_t i = 0; i < arg_count; i++) {
    return_value = evaluate_lisp(args[i], env);
  }
  return return_value;
}

LispValue builtin_unpack(LispValue *args, size_t arg_count, Environment *env) {
  size_t item_count = 0;
  for (size_t i = 0; i < arg_count; i++) {
    if (args[i].type != LISP_TUPLE)
      return ERROR_TO_LISP("`unpack` expects tuples");

    item_count += args[i].items.size;
  }

  LispValue *array = calloc(item_count, sizeof(LispValue));
  size_t j = 0;
  for (size_t i = 0; i < arg_count; i++) {
    memcpy(&array[j], args[i].items.array,
           args[i].items.size * sizeof(LispValue));
    j += args[i].items.size;
  }

  return (LispValue){.type = LISP_UNPACKED,
                     .items = {.array = array, .size = item_count}};
}

LispValue builtin_quote(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`quote` expects exactly 1 arguments");

  return args[0];
}

LispValue builtin_head(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`head` expects exactly 1 arguments");

  if (args[0].type != LISP_TUPLE)
    return ERROR_TO_LISP("`head` expects a tuple");

  if (!args[0].items.size)
    return ERROR_TO_LISP("`head` expects a non-empty tuple");

  return args[0].items.array[0];
}

LispValue builtin_tail(LispValue *args, size_t arg_count, Environment *env) {
  if (arg_count != 1)
    return ERROR_TO_LISP("`tail` expects exactly 1 arguments");

  if (args[0].type != LISP_TUPLE)
    return ERROR_TO_LISP("`tail` expects a tuple");

  if (!args[0].items.size)
    return ERROR_TO_LISP("`tail` expects a non-empty tuple");

  return (LispValue){.type = LISP_TUPLE,
                     .items = {.array = args[0].items.array + 1,
                               .size = args[0].items.size - 1}};
}

#define DEFINE_BUILTIN_FUNCTION(n, c, m, t)                                    \
  env_set(&global_env, n,                                                      \
          (LispValue){.type = LISP_BUILTIN_FUNC,                               \
                      .builtin_function = {.callback = (LispValue (*)(         \
                                               LispValue *, size_t, void *))c, \
                                           .is_macro = m,                      \
                                           .tail_call = t}});

int main(void) {
  Environment global_env = {.other = NULL};
  da_init(&global_env);

  // Singletons
  env_set(&global_env, "nil", NIL_SINGLETON_LISP);
  env_set(&global_env, "t", T_SINGLETON_LISP);

  // Builtin functions
  DEFINE_BUILTIN_FUNCTION("print", builtin_print, false, false);
  DEFINE_BUILTIN_FUNCTION("+", builtin_sum, false, false);
  DEFINE_BUILTIN_FUNCTION("-", builtin_neg_or_sub, false, false);
  DEFINE_BUILTIN_FUNCTION("list", builtin_list, false, false);
  DEFINE_BUILTIN_FUNCTION("=", builtin_equal, false, false);
  DEFINE_BUILTIN_FUNCTION("<", builtin_less_than, false, false);
  DEFINE_BUILTIN_FUNCTION("read-file", builtin_read_file, false, false);
  DEFINE_BUILTIN_FUNCTION("parse-lisp", builtin_parse_lisp, false, false);
  DEFINE_BUILTIN_FUNCTION("eval", builtin_eval, false, false);
  DEFINE_BUILTIN_FUNCTION("unpack", builtin_unpack, false, false);
  DEFINE_BUILTIN_FUNCTION("head", builtin_head, false, false);
  DEFINE_BUILTIN_FUNCTION("tail", builtin_tail, false, false);

  // Builtin macros
  DEFINE_BUILTIN_FUNCTION("set", builtin_set, true, false);
  DEFINE_BUILTIN_FUNCTION("setg", builtin_setg, true, false);
  DEFINE_BUILTIN_FUNCTION("quote", builtin_quote, true, false);
  DEFINE_BUILTIN_FUNCTION("fn", builtin_fn, true, false);
  DEFINE_BUILTIN_FUNCTION("macro", builtin_macro, true, false);

  // Builtin macros with tail call optimization
  DEFINE_BUILTIN_FUNCTION("let", builtin_let, true, false);
  DEFINE_BUILTIN_FUNCTION("if", builtin_if, true, true);
  DEFINE_BUILTIN_FUNCTION("do", builtin_do, true, true);

  char *startup[] = {"core.l"};

  for (size_t i = 0; i < sizeof(startup) / sizeof(startup[0]); i++) {
    char *buffer = read_file(startup[i]);
    if (!buffer)
      fprintf(stderr, "warning: could not read contents of %s\n", startup[i]);

    repl(buffer, &global_env);
  }

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
