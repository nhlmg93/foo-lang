#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/arena.c"

#define STBDS_REALLOC(context, ptr, size) arena_realloc((ptr), (size))
#define STBDS_FREE(context, ptr) ((void)(ptr))
#include "vendor/stb_ds.h"

#include "lib/ast.c"
#include "lib/compiler/compiler.c"
#include "lib/compiler/vm.c"
#include "lib/interpreter/evaluator.c"
#include "lib/lexer.c"
#include "lib/parser.c"

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

typedef enum : uint8_t {
  MODE_INTERPRETER,
  MODE_COMPILER,
} ExecutionMode;

typedef struct {
  ExecutionMode mode;
  const char *path;
} Options;

static Options parse_options(int argc, char *argv[]) {
  PANIC(argc <= 3,
        "usage: foo [--interpreter|--compiler] [source-file]");

  Options options = {.mode = MODE_INTERPRETER};
  uint32_t index = 1;
  if (index < (uint32_t)argc &&
      (strcmp(argv[index], "--interpreter") == 0 ||
       strcmp(argv[index], "-i") == 0)) {
    index++;
  } else if (index < (uint32_t)argc &&
             (strcmp(argv[index], "--compiler") == 0 ||
              strcmp(argv[index], "-c") == 0)) {
    options.mode = MODE_COMPILER;
    index++;
  }

  if (index < (uint32_t)argc) {
    PANIC(argv[index][0] != '-', "unknown option: %s", argv[index]);
    options.path = argv[index];
    index++;
  }
  PANIC(index == (uint32_t)argc,
        "usage: foo [--interpreter|--compiler] [source-file]");
  return options;
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

  char *const source = arena_alloc((size_t)size + 1);
  const size_t read = fread(source, 1, (size_t)size, f);
  PANIC(read == (size_t)size, "could not read file: %s", path);
  source[(size_t)size] = '\0';
  fclose(f);
  return source;
}

static Stmt *parse_source(Lexer *lexer, Parser *parser, char *source) {
  lexer_clear(lexer);
  lexer_tokenize(lexer, source);
  parser_parse(parser, lexer->tokens);
  return parser->ast.stmts;
}

static void run_interpreter_file(Lexer *lexer, Parser *parser,
                                 const char *path) {
  Evaluator evaluator;
  evaluator_init(&evaluator);
  Stmt *const stmts = parse_source(lexer, parser, read_file(path));
  value_print(evaluator_evaluate(&evaluator, stmts));
}

static void run_compiler_file(Lexer *lexer, Parser *parser,
                              const char *path) {
  Compiler compiler;
  Vm vm;
  compiler_init(&compiler);
  vm_init(&vm);

  Stmt *const stmts = parse_source(lexer, parser, read_file(path));
  vm_value_print(vm_run(&vm, compiler_compile(&compiler, stmts)));
}

static void run_interpreter_repl(Lexer *lexer, Parser *parser) {
  Evaluator evaluator;
  evaluator_init(&evaluator);

  char line[1024];
  printf(">>> ");
  while (fgets(line, sizeof(line), stdin) != NULL) {
    const size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\n')
      line[length - 1] = '\0';

    Stmt *const stmts = parse_source(lexer, parser, line);
    value_print(evaluator_evaluate(&evaluator, stmts));
    printf(">>> ");
  }
}

static void run_compiler_repl(Lexer *lexer, Parser *parser) {
  Compiler compiler;
  Vm vm;
  compiler_init(&compiler);
  vm_init(&vm);

  char line[1024];
  printf(">>> ");
  while (fgets(line, sizeof(line), stdin) != NULL) {
    const size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\n')
      line[length - 1] = '\0';

    Stmt *const stmts = parse_source(lexer, parser, line);
    vm_value_print(vm_run(&vm, compiler_compile(&compiler, stmts)));
    printf(">>> ");
  }
}

int main(int argc, char *argv[]) {
  const Options options = parse_options(argc, argv);
  Lexer lexer;
  Parser parser;
  lexer_init(&lexer);
  parser_init(&parser);

  if (options.path != NULL) {
    if (options.mode == MODE_COMPILER)
      run_compiler_file(&lexer, &parser, options.path);
    else
      run_interpreter_file(&lexer, &parser, options.path);
    return EXIT_SUCCESS;
  }

  if (options.mode == MODE_COMPILER)
    run_compiler_repl(&lexer, &parser);
  else
    run_interpreter_repl(&lexer, &parser);
  return EXIT_SUCCESS;
}

#define ARENA_IMPLEMENTATION
#include "lib/arena.c"

#define STB_DS_IMPLEMENTATION
#include "vendor/stb_ds.h"

#define LEXER_IMPLEMENTATION
#include "lib/lexer.c"

#define AST_IMPLEMENTATION
#include "lib/ast.c"

#define PARSER_IMPLEMENTATION
#include "lib/parser.c"

#define EVALUATOR_IMPLEMENTATION
#include "lib/interpreter/evaluator.c"

#define BYTECODE_IMPLEMENTATION
#include "lib/compiler/bytecode.c"

#define COMPILER_IMPLEMENTATION
#include "lib/compiler/compiler.c"

#define VM_IMPLEMENTATION
#include "lib/compiler/vm.c"
