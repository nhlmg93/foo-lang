#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_TABLE                                                            \
  X(ILLEGAL, "ILLEGAL")                                                        \
  X(END, "EOF")                                                                \
  X(IDENT, "IDENT")                                                            \
  X(INT, "INT")                                                                \
  X(ASSIGN, "=")                                                               \
  X(EQUAL, "==")                                                               \
  X(NOT_EQUAL, "!=")                                                           \
  X(PLUS, "+")                                                                 \
  X(MINUS, "-")                                                                \
  X(BANG, "!")                                                                 \
  X(ASTERISK, "*")                                                             \
  X(SLASH, "/")                                                                \
  X(LT, "<")                                                                   \
  X(GT, ">")                                                                   \
  X(COMMA, ",")                                                                \
  X(SEMICOLON, ";")                                                            \
  X(LPAREN, "(")                                                               \
  X(RPAREN, ")")                                                               \
  X(LBRACE, "{")                                                               \
  X(RBRACE, "}")                                                               \
  X(FUNCTION, "FUNCTION")                                                      \
  X(LET, "LET")                                                                \
  X(TRUE, "TRUE")                                                              \
  X(FALSE, "FALSE")                                                            \
  X(IF, "IF")                                                                  \
  X(ELSE, "ELSE")                                                              \
  X(RETURN, "RETURN")

typedef enum : uint8_t {
#define X(kind, str) kind,
  TOKEN_TABLE
#undef X
} TokenType;

static const char *const TOKEN_STRINGS[] = {
#define X(kind, str) str,
    TOKEN_TABLE
#undef X
};

const char *token_type_to_str(TokenType t) { return TOKEN_STRINGS[t]; }

typedef struct Token {
  TokenType type;
  const char *v;
} Token;

typedef struct Expr Expr; // Forward declaration

typedef enum {
  STMT_LET,
  STMT_RETURN,
  STMT_EXPR,
} StmtType;

typedef struct Stmt {
  StmtType type;
  union {
    struct {
      const char *name;
      Expr *value;
    } let; // let x = 5;
    struct {
      Expr *value;
    } ret; // return 5;
    struct {
      Expr *expr;
    } expr_stmt; // 5 + 5;
  } data;
} Stmt;

typedef struct Program {
  Stmt *stmts;
  int count;
} Program;

// Minimal expression (placeholder)
struct Expr {
  int placeholder;
};

// Flat Interpreter struct (everything in one place)
typedef struct Interpreter {
  // Tokenizer
  Token *token_buf;
  int token_count;

  // Parser
  int pos;
  Token curr;
  Token next;

  // Program
  Stmt *stmts;
  int stmt_count;
} Interpreter;

/* ============================================================================
 * UTILS
 * ============================================================================
 */

void print_usage(const char *prog) {
  fprintf(stderr, "usage: %s [file | repl]\n", prog);
  fprintf(stderr, "  file    tokenize the specified file\n");
  fprintf(stderr, "  repl    start interactive read-eval-print loop\n");
}

/* ----------------------------------------------------------------------------
 * Arena Allocator
 * ----------------------------------------------------------------------------
 */

#define MB (1024 * 1024)
#define ARENA_SIZE (1 * MB)
#define MAX_TOKENS (32 * 1024) // 32k tokens ~512KB
#define MAX_STMTS (4 * 1024) // 4k statements

static size_t arena_offset = 0;
static unsigned char arena_buf[ARENA_SIZE];

unsigned char *align_ptr(unsigned char *p, size_t alignment) {
  uintptr_t addr = (uintptr_t)p;
  return (unsigned char *)((addr + (alignment - 1)) &
                           ~(uintptr_t)(alignment - 1));
}

void *arena_alloc(size_t size) {
  unsigned char *p = align_ptr(&arena_buf[arena_offset], sizeof(void *));
  size_t new_offset = (size_t)(p - arena_buf) + size;
  assert(new_offset <= ARENA_SIZE && "arena out of memory");
  arena_offset = new_offset;
  return p;
}

/* ----------------------------------------------------------------------------
 * Keywords
 * ----------------------------------------------------------------------------
 */

typedef struct {
  const char *key;
  TokenType value;
} KeywordEntry;

static const KeywordEntry KEYWORDS[] = {
    {"fn", FUNCTION}, {"let", LET},   {"true", TRUE},     {"false", FALSE},
    {"if", IF},       {"else", ELSE}, {"return", RETURN},
};

TokenType keyword_lookup(const char *key) {
  for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
    if (strcmp(KEYWORDS[i].key, key) == 0) {
      return KEYWORDS[i].value;
    }
  }
  return IDENT;
}

/* ----------------------------------------------------------------------------
 * Debug
 * ----------------------------------------------------------------------------
 */

#if defined(__GNUC__) || defined(__clang__)
#define FORMAT_PRINTF(fmt_idx, arg_idx)                                        \
  __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define FORMAT_PRINTF(fmt_idx, arg_idx)
#endif

FORMAT_PRINTF(1, 2) void debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
}

/* ============================================================================
 * UTILS END
 * ============================================================================
 */

Token *push_token(Interpreter *intpr) {
  assert(intpr->token_count < MAX_TOKENS && "too many tokens");
  return &intpr->token_buf[intpr->token_count++];
}

int read_ident(Interpreter *intpr, char *input, int pos) {
  int start = pos;
  while (isalpha(input[pos]))
    pos++;

  int len = pos - start;
  char *ident = arena_alloc(len + 1);
  memcpy(ident, &input[start], len);
  ident[len] = '\0';

  Token *tok = push_token(intpr);
  tok->v = ident;
  tok->type = keyword_lookup(tok->v);

  return pos - 1;
}

int read_number(Interpreter *intpr, char *input, int pos) {
  int start = pos;
  while (isdigit(input[pos]))
    pos++;

  int len = pos - start;
  char *num = arena_alloc(len + 1);
  memcpy(num, &input[start], len);
  num[len] = '\0';

  Token *tok = push_token(intpr);
  tok->v = num;
  tok->type = INT;

  return pos - 1;
}

char peek_char(const char *input, int pos) { return input[pos + 1]; }

void tokenize(Interpreter *intpr, char *input) {
  int i = 0;
  while (true) {
    char ch = input[i];
    if (ch == '\0')
      break;
    if (isspace(ch)) {
      i++;
      continue;
    }

    Token *tok;

    switch (ch) {
    case '+':
      tok = push_token(intpr);
      tok->v = "+";
      tok->type = PLUS;
      break;
    case '=':
      tok = push_token(intpr);
      if (peek_char(input, i) == '=') {
        i++;
        tok->v = "==";
        tok->type = EQUAL;
      } else {
        tok->v = "=";
        tok->type = ASSIGN;
      }
      break;
    case ';':
      tok = push_token(intpr);
      tok->v = ";";
      tok->type = SEMICOLON;
      break;
    case '{':
      tok = push_token(intpr);
      tok->v = "{";
      tok->type = LBRACE;
      break;
    case '}':
      tok = push_token(intpr);
      tok->v = "}";
      tok->type = RBRACE;
      break;
    case '(':
      tok = push_token(intpr);
      tok->v = "(";
      tok->type = LPAREN;
      break;
    case ')':
      tok = push_token(intpr);
      tok->v = ")";
      tok->type = RPAREN;
      break;
    case ',':
      tok = push_token(intpr);
      tok->v = ",";
      tok->type = COMMA;
      break;
    case '-':
      tok = push_token(intpr);
      tok->v = "-";
      tok->type = MINUS;
      break;
    case '!':
      tok = push_token(intpr);
      if (peek_char(input, i) == '=') {
        i++;
        tok->v = "!=";
        tok->type = NOT_EQUAL;
      } else {
        tok->v = "!";
        tok->type = BANG;
      }
      break;
    case '/':
      tok = push_token(intpr);
      tok->v = "/";
      tok->type = SLASH;
      break;
    case '*':
      tok = push_token(intpr);
      tok->v = "*";
      tok->type = ASTERISK;
      break;
    case '<':
      tok = push_token(intpr);
      tok->v = "<";
      tok->type = LT;
      break;
    case '>':
      tok = push_token(intpr);
      tok->v = ">";
      tok->type = GT;
      break;
    default:
      if (isalpha(ch)) {
        i = read_ident(intpr, input, i);
        tok = &intpr->token_buf[intpr->token_count - 1];
      } else if (isdigit(ch)) {
        i = read_number(intpr, input, i);
        tok = &intpr->token_buf[intpr->token_count - 1];
      } else {
        tok = push_token(intpr);
        static char illegal_buf[2];
        illegal_buf[0] = ch;
        illegal_buf[1] = '\0';
        tok->v = illegal_buf;
        tok->type = ILLEGAL;
      }
      break;
    }

    debug("token_type= \"%s\" token= \"%s\"\n", token_type_to_str(tok->type),
          tok->v);
    i++;
  }
}

char *read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "error: could not open file: %s\n", path);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size < 0) {
    fclose(f);
    fprintf(stderr, "error: could not determine file size: %s\n", path);
    return NULL;
  }

  char *buf = arena_alloc((size_t)size + 1);
  size_t read = fread(buf, 1, (size_t)size, f);
  buf[read] = '\0';
  fclose(f);

  return buf;
}

int main(int argc, char *argv[]) {

  if (argc > 2) {
    fprintf(stderr, "error: too many arguments\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (argc == 1) {
    fprintf(stderr, "error: no input file specified\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "repl") == 0) {
    char line[1024];
    Token *tokens = arena_alloc(sizeof(Token) * MAX_TOKENS);
    Interpreter intpr = {.token_buf = tokens, .token_count = 0};

    printf(">>> ");
    while (fgets(line, sizeof(line), stdin) != NULL) {
      intpr.token_count = 0;

      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
      }

      tokenize(&intpr, line);

      printf(">>> ");
    }
    return EXIT_SUCCESS;
  }

  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
    print_usage(argv[0]);
    return EXIT_SUCCESS;
  }

  char *source = read_file(argv[1]);
  if (!source) {
    return EXIT_FAILURE;
  }

  Token *tokens = arena_alloc(sizeof(Token) * MAX_TOKENS);
  Interpreter intpr = {.token_buf = tokens, .token_count = 0};
  tokenize(&intpr, source);

  return EXIT_SUCCESS;
}
