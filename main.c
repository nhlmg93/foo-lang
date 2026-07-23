#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.c"

#define STBDS_REALLOC(context, ptr, size) arena_realloc((ptr), (size))
#define STBDS_FREE(context, ptr) ((void)(ptr))
#include "vendor/stb_ds.h"

#include "ast.c"
#include "lexer.c"
#include "parser.c"

#define MAX_ENVIRONMENTS ((uint32_t)32 * 1024)
#define MAX_BINDINGS ((uint32_t)32 * 1024)
#define MAX_EVAL_DEPTH ((uint32_t)256)
#define MAX_CALL_DEPTH ((uint32_t)256)

[[noreturn]] static void panic_impl(const char *file, uint32_t line,
                                    const char *fmt, ...) {
  fprintf(stderr, "%s:%u: panic: ", file, line);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
  abort();
}

#define PANIC(condition, ...)                                                  \
  do {                                                                         \
    if (!(condition))                                                          \
      panic_impl(__FILE__, __LINE__, __VA_ARGS__);                             \
  } while (0)

typedef struct Environment Environment;

typedef enum : uint8_t {
  VALUE_NULL,
  VALUE_INT,
  VALUE_BOOL,
  VALUE_FUNCTION,
} ValueType;

typedef struct {
  ValueType type;
  union {
    int64_t integer;
    bool boolean;
    struct {
      const char **parameters;
      Stmt *body;
      Environment *environment;
    } function;
  } data;
} Value;

typedef struct {
  const char *key;
  Value value;
} Binding;

struct Environment {
  Binding *bindings;
  Environment *outer;
};

typedef struct {
  Environment *global;
  uint32_t environment_count;
  uint32_t binding_count;
  uint32_t eval_depth;
  uint32_t call_depth;
} Evaluator;

typedef struct {
  Value value;
  bool returned;
} EvalResult;

static Value null_value(void) { return (Value){.type = VALUE_NULL}; }

static Value int_value(int64_t value) {
  return (Value){
      .type = VALUE_INT,
      .data.integer = value,
  };
}

static Value bool_value(bool value) {
  return (Value){
      .type = VALUE_BOOL,
      .data.boolean = value,
  };
}

static EvalResult eval_value(Value value) {
  return (EvalResult){
      .value = value,
      .returned = false,
  };
}

static const char *value_type_to_string(ValueType type) {
  switch (type) {
  case VALUE_NULL:
    return "NULL";
  case VALUE_INT:
    return "INTEGER";
  case VALUE_BOOL:
    return "BOOLEAN";
  case VALUE_FUNCTION:
    return "FUNCTION";
  }
  PANIC(false, "unknown value type: %u", type);
}

static Environment *new_environment(Evaluator *evaluator, Environment *outer) {
  PANIC(evaluator->environment_count < MAX_ENVIRONMENTS,
        "environment capacity exceeded (capacity=%u)", MAX_ENVIRONMENTS);

  Environment *const environment = arena_alloc(sizeof(*environment));
  *environment = (Environment){.outer = outer};
  evaluator->environment_count++;
  return environment;
}

static void environment_set(Evaluator *evaluator, Environment *environment,
                            const char *name, Value value) {
  const ptrdiff_t index = shgeti(environment->bindings, name);
  if (index >= 0) {
    environment->bindings[index].value = value;
    return;
  }

  PANIC(evaluator->binding_count < MAX_BINDINGS,
        "binding capacity exceeded (capacity=%u)", MAX_BINDINGS);

  shput(environment->bindings, name, value);
  evaluator->binding_count++;
}

static Value environment_get(Environment *environment, const char *name,
                             Token token) {
  for (uint32_t depth = 0; depth < MAX_ENVIRONMENTS; depth++) {
    PANIC(environment != NULL, "%u:%u: identifier not found: %s", token.line,
          token.column, name);

    const ptrdiff_t index = shgeti(environment->bindings, name);
    if (index >= 0)
      return environment->bindings[index].value;
    environment = environment->outer;
  }
  PANIC(false, "environment chain exceeds capacity (capacity=%u)",
        MAX_ENVIRONMENTS);
}

static void evaluator_init(Evaluator *evaluator) {
  *evaluator = (Evaluator){0};
  evaluator->global = new_environment(evaluator, NULL);
}

static bool is_truthy(Value value) {
  switch (value.type) {
  case VALUE_NULL:
    return false;
  case VALUE_BOOL:
    return value.data.boolean;
  default:
    return true;
  }
}

static Value eval_prefix(Token token, TokenType op, Value right) {
  if (op == BANG)
    return bool_value(!is_truthy(right));

  PANIC(op == MINUS && right.type == VALUE_INT,
        "%u:%u: unsupported prefix operator %s%s", token.line, token.column,
        token_type_to_str(op), value_type_to_string(right.type));
  PANIC(right.data.integer != INT64_MIN, "%u:%u: integer overflow", token.line,
        token.column);
  return int_value(-right.data.integer);
}

static Value eval_integer_infix(Token token, TokenType op, int64_t left,
                                int64_t right) {
  int64_t result;
  switch (op) {
  case PLUS:
    PANIC(!__builtin_add_overflow(left, right, &result),
          "%u:%u: integer overflow", token.line, token.column);
    return int_value(result);
  case MINUS:
    PANIC(!__builtin_sub_overflow(left, right, &result),
          "%u:%u: integer overflow", token.line, token.column);
    return int_value(result);
  case ASTERISK:
    PANIC(!__builtin_mul_overflow(left, right, &result),
          "%u:%u: integer overflow", token.line, token.column);
    return int_value(result);
  case SLASH:
    PANIC(right != 0, "%u:%u: division by zero", token.line, token.column);
    PANIC(left != INT64_MIN || right != -1, "%u:%u: integer overflow",
          token.line, token.column);
    return int_value(left / right);
  case LT:
    return bool_value(left < right);
  case GT:
    return bool_value(left > right);
  case EQUAL:
    return bool_value(left == right);
  case NOT_EQUAL:
    return bool_value(left != right);
  default:
    PANIC(false, "%u:%u: unsupported integer operator: %s", token.line,
          token.column, token_type_to_str(op));
  }
  __builtin_unreachable();
}

static Value eval_infix(Token token, TokenType op, Value left, Value right) {
  if (left.type == VALUE_INT && right.type == VALUE_INT)
    return eval_integer_infix(token, op, left.data.integer, right.data.integer);

  PANIC(left.type == right.type, "%u:%u: type mismatch: %s %s %s", token.line,
        token.column, value_type_to_string(left.type), token_type_to_str(op),
        value_type_to_string(right.type));

  if (left.type == VALUE_BOOL && (op == EQUAL || op == NOT_EQUAL)) {
    const bool equal = left.data.boolean == right.data.boolean;
    return bool_value(op == EQUAL ? equal : !equal);
  }
  if (left.type == VALUE_NULL && (op == EQUAL || op == NOT_EQUAL))
    return bool_value(op == EQUAL);

  PANIC(false, "%u:%u: unsupported operator: %s %s %s", token.line,
        token.column, value_type_to_string(left.type), token_type_to_str(op),
        value_type_to_string(right.type));
}

static EvalResult eval_expression(Evaluator *evaluator,
                                  Environment *environment, Expr *expr);

static EvalResult eval_statements(Evaluator *evaluator,
                                  Environment *environment, Stmt *stmts) {
  EvalResult result = eval_value(null_value());
  for (size_t i = 0; i < arrlenu(stmts); i++) {
    const Stmt *const stmt = &stmts[i];
    switch (stmt->type) {
    case STMT_LET:
      result = eval_expression(evaluator, environment, stmt->data.let.value);
      if (result.returned)
        return result;
      environment_set(evaluator, environment, stmt->data.let.name,
                      result.value);
      result = eval_value(null_value());
      break;
    case STMT_RETURN:
      result = eval_expression(evaluator, environment, stmt->data.ret.value);
      result.returned = true;
      return result;
    case STMT_EXPR:
      result = eval_expression(evaluator, environment, stmt->data.expr.value);
      if (result.returned)
        return result;
      break;
    }
  }
  return result;
}

static EvalResult eval_if_expression(Evaluator *evaluator,
                                     Environment *environment, Expr *expr) {
  EvalResult condition =
      eval_expression(evaluator, environment, expr->data.if_expr.condition);
  if (condition.returned)
    return condition;
  if (is_truthy(condition.value))
    return eval_statements(evaluator, environment,
                           expr->data.if_expr.consequence);
  if (expr->data.if_expr.alternative != NULL)
    return eval_statements(evaluator, environment,
                           expr->data.if_expr.alternative);
  return eval_value(null_value());
}

static EvalResult eval_call_expression(Evaluator *evaluator,
                                       Environment *environment, Expr *expr) {
  EvalResult function =
      eval_expression(evaluator, environment, expr->data.call.function);
  if (function.returned)
    return function;
  PANIC(function.value.type == VALUE_FUNCTION,
        "%u:%u: value is not callable: %s", expr->token.line,
        expr->token.column, value_type_to_string(function.value.type));

  const size_t parameter_count =
      arrlenu(function.value.data.function.parameters);
  const size_t argument_count = arrlenu(expr->data.call.arguments);
  PANIC(parameter_count == argument_count,
        "%u:%u: expected %zu arguments, got %zu", expr->token.line,
        expr->token.column, parameter_count, argument_count);

  Environment *const call_environment =
      new_environment(evaluator, function.value.data.function.environment);
  for (size_t i = 0; i < argument_count; i++) {
    EvalResult argument =
        eval_expression(evaluator, environment, expr->data.call.arguments[i]);
    if (argument.returned)
      return argument;
    environment_set(evaluator, call_environment,
                    function.value.data.function.parameters[i], argument.value);
  }

  PANIC(evaluator->call_depth < MAX_CALL_DEPTH,
        "call depth exceeds capacity (capacity=%u)", MAX_CALL_DEPTH);
  evaluator->call_depth++;
  EvalResult result = eval_statements(evaluator, call_environment,
                                      function.value.data.function.body);
  evaluator->call_depth--;
  result.returned = false;
  return result;
}

static EvalResult eval_expression(Evaluator *evaluator,
                                  Environment *environment, Expr *expr) {
  PANIC(evaluator->eval_depth < MAX_EVAL_DEPTH,
        "%u:%u: evaluation depth exceeds capacity (capacity=%u)",
        expr->token.line, expr->token.column, MAX_EVAL_DEPTH);

  evaluator->eval_depth++;
  EvalResult result;
  switch (expr->type) {
  case EXPR_IDENT:
    result =
        eval_value(environment_get(environment, expr->data.ident, expr->token));
    break;
  case EXPR_INT:
    result = eval_value(int_value(expr->data.integer));
    break;
  case EXPR_BOOL:
    result = eval_value(bool_value(expr->data.boolean));
    break;
  case EXPR_PREFIX: {
    EvalResult right =
        eval_expression(evaluator, environment, expr->data.prefix.right);
    result = right.returned
                 ? right
                 : eval_value(eval_prefix(expr->token, expr->data.prefix.op,
                                          right.value));
    break;
  }
  case EXPR_INFIX: {
    EvalResult left =
        eval_expression(evaluator, environment, expr->data.infix.left);
    if (left.returned) {
      result = left;
      break;
    }
    EvalResult right =
        eval_expression(evaluator, environment, expr->data.infix.right);
    result = right.returned
                 ? right
                 : eval_value(eval_infix(expr->token, expr->data.infix.op,
                                         left.value, right.value));
    break;
  }
  case EXPR_IF:
    result = eval_if_expression(evaluator, environment, expr);
    break;
  case EXPR_FUNCTION:
    result = eval_value((Value){
        .type = VALUE_FUNCTION,
        .data.function =
            {
                .parameters = expr->data.function.parameters,
                .body = expr->data.function.body,
                .environment = environment,
            },
    });
    break;
  case EXPR_CALL:
    result = eval_call_expression(evaluator, environment, expr);
    break;
  }

  evaluator->eval_depth--;
  return result;
}

static Value evaluate(Evaluator *evaluator, Stmt *stmts) {
  EvalResult result = eval_statements(evaluator, evaluator->global, stmts);
  result.returned = false;
  return result.value;
}

static void print_value(Value value) {
  switch (value.type) {
  case VALUE_NULL:
    puts("null");
    break;
  case VALUE_INT:
    printf("%" PRId64 "\n", value.data.integer);
    break;
  case VALUE_BOOL:
    puts(value.data.boolean ? "true" : "false");
    break;
  case VALUE_FUNCTION:
    puts("<function>");
    break;
  }
}

static char *read_file(const char *path) {
  FILE *f = fopen(path, "r");
  PANIC(f != NULL, "could not open file: %s", path);
  PANIC(fseek(f, 0, SEEK_END) == 0, "could not seek file: %s", path);

  const long size = ftell(f);
  PANIC(fseek(f, 0, SEEK_SET) == 0, "could not seek file: %s", path);
  PANIC(size >= 0, "could not determine file size: %s", path);
  PANIC((size_t)size <= MAX_SOURCE_BYTES,
        "source exceeds capacity (capacity=%zu)", MAX_SOURCE_BYTES);

  char *const buf = arena_alloc((size_t)size + 1);
  const size_t read = fread(buf, 1, (size_t)size, f);
  PANIC(read == (size_t)size, "could not read file: %s", path);
  buf[read] = '\0';
  fclose(f);
  return buf;
}

int main(int argc, char *argv[]) {
  PANIC(argc <= 2, "too many arguments");

  Lexer lexer;
  Parser parser;
  Evaluator evaluator;
  lexer_init(&lexer);
  parser_init(&parser);
  evaluator_init(&evaluator);

  if (argc == 1) {
    char line[1024];
    printf(">>> ");
    while (fgets(line, sizeof(line), stdin) != NULL) {
      lexer_clear(&lexer);

      const size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n')
        line[len - 1] = '\0';

      lexer_tokenize(&lexer, line);
      parser_parse(&parser, lexer.tokens);
      print_value(evaluate(&evaluator, parser.ast.stmts));
      printf(">>> ");
    }
    return EXIT_SUCCESS;
  }

  char *const source = read_file(argv[1]);
  lexer_tokenize(&lexer, source);
  parser_parse(&parser, lexer.tokens);
  print_value(evaluate(&evaluator, parser.ast.stmts));
  return EXIT_SUCCESS;
}

#define ARENA_IMPLEMENTATION
#include "arena.c"

#define STB_DS_IMPLEMENTATION
#include "vendor/stb_ds.h"

#define AST_IMPLEMENTATION
#include "ast.c"

#define LEXER_IMPLEMENTATION
#include "lexer.c"

#define PARSER_IMPLEMENTATION
#include "parser.c"
