#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Token type definitions
#define ILLEGAL "ILLEGAL"
#define EOF_TOKEN "EOF"
// Identifiers + Literals
#define IDENT "IDENT"
#define INT "INT"
// Operators
#define ASSIGN "="
#define PLUS "+"
#define COMMA ","
// Delimiters
#define SEMICOLON ";"
#define LPAREN "("
#define RPAREN ")"
#define LBRACE "{"
#define RBRACE "}"
// Keywords
#define FUNCTION "FUNCTION"
#define LET "LET"

#define MB (1024 * 1024)
#define MAX_TOKENS 1024
static size_t arena_offset = 0;

static unsigned char arena_buf[1 * MB];

static inline void *align(void *p, size_t alignment) {
  uintptr_t addr = (uintptr_t)p;
  return (void *)((addr + (alignment - 1)) & ~(uintptr_t)(alignment - 1));
}

static void *arena_alloc(size_t size) {
  void *p = align(&arena_buf[arena_offset], sizeof(void *));
  size_t new_offset = (size_t)((unsigned char *)p - arena_buf) + size;
  assert(new_offset <= sizeof(arena_buf));
  arena_offset = new_offset;
  return p;
}

#include <stdarg.h>

__attribute__((format(printf, 1, 2)))
static void debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *tmp;
  vasprintf(&tmp, fmt, args);
  va_end(args);
  size_t len = strlen(tmp) + 1;
  char *p = arena_alloc(len);
  memcpy(p, tmp, len);
  free(tmp);
  printf("%s", p);
}

typedef struct Token {
  char *type;
  char *v;
} Token;

typedef struct Interpreter {
  Token *token_buf;
  int token_count;
} Interpreter;

int read_ident(Interpreter *intpr, const char *input, int pos) {
  assert(intpr->token_count < MAX_TOKENS);
  int start = pos++;
  while (isalpha(input[pos])) {
    pos++;
  }
  int len = pos - start;
  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = arena_alloc(len + 1);
  memcpy(tok->v, &input[start], len);
  tok->v[len] = '\0';
  tok->type = IDENT;
  return --pos;
}

int read_number(Interpreter *intpr, const char *input, int pos) {
  assert(intpr->token_count < MAX_TOKENS);
  int start = pos;
  while (isdigit(input[pos])) {
    pos++;
  }
  int len = pos - start;
  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = arena_alloc(len + 1);
  memcpy(tok->v, &input[start], len);
  tok->v[len] = '\0';
  tok->type = INT;
  return --pos;
}

int main() {
  const char *input = R"(
let five = 5;
let ten = 10;
let add = fn(x, y) {
    x + y;
};

let result = add(five, ten);
)";
  Interpreter intpr = {0};
  intpr.token_buf = arena_alloc(MAX_TOKENS * sizeof(Token));
  for (int i = 0; i < strlen(input); i++) {
    char ch = input[i];

    if (isspace(ch))
      continue;

    assert(intpr.token_count < MAX_TOKENS);

    Token *tok = &intpr.token_buf[intpr.token_count];
    tok->v = arena_alloc(2);
    tok->v[0] = ch;
    tok->v[1] = '\0';

    switch (ch) {
    case '+':
      tok->type = PLUS;
      break;
    case '=':
      tok->type = ASSIGN;
      break;
    case ';':
      tok->type = SEMICOLON;
      break;
    case '{':
      tok->type = LBRACE;
      break;
    case '}':
      tok->type = RBRACE;
      break;
    case '(':
      tok->type = LPAREN;
      break;
    case ')':
      tok->type = RPAREN;
      break;
    case ',':
      tok->type = COMMA;
      break;
    default:
      if (isalpha(ch)) {
        i = read_ident(&intpr, input, i);
        break;
      } else if (isdigit(ch)) {
        i = read_number(&intpr, input, i);
        break;
      } else {
        tok->type = ILLEGAL;
        break;
      }
    }
    debug("token_type= \"%s\" token= \"%s\"\n", tok->type, tok->v);
  }

  return EXIT_SUCCESS;
}
