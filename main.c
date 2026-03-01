#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct Arena {
  uint8_t *base;
  size_t cap;
  size_t off;
} Arena;

static inline uintptr_t align_forward_uintptr(uintptr_t p, size_t align) {
  return (p + (align - 1)) & ~(uintptr_t)(align - 1);
}

void arena_init(Arena *a, void *backing, size_t cap) {
  a->base = (uint8_t *)backing;
  a->cap = cap;
  a->off = 0;
}

void *arena_alloc_align(Arena *a, size_t size, size_t align) {
  uintptr_t cur = (uintptr_t)a->base + (uintptr_t)a->off;
  uintptr_t aligned = align_forward_uintptr(cur, align);
  size_t newOff = (size_t)(aligned - (uintptr_t)a->base) + size;
  if (newOff > a->cap)
    return NULL;
  a->off = newOff;
  return (void *)aligned;
}

typedef char *TokenType;

typedef struct Token {
  TokenType type;
  char literal;
} Token;

struct Token token_init(TokenType type, char literal) {
  struct Token t;
  t.type = type;
  t.literal = literal;
  return t;
}

void print_token(Token t) {
  printf("Token {.type= \"%s\", .literal= '%c'}\n", t.type, t.literal);
}

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

typedef uint8_t byte;

typedef struct Lexer {
  char *input;
  int position;     // current pos in input
  int readPosition; // current reading pos
  byte ch;          // current char
} Lexer;

void print_lexer(Lexer l) {
  printf(
      "Lexer {.input= \"%s\", .position= %d, .readPosition= %d, .ch= '%c'}\n",
      l.input, l.position, l.readPosition, l.ch);
}

struct Lexer lexer_init(char *input) {
  struct Lexer l;
  memset(&l, 0, sizeof(Lexer));
  l.input = input;
  return l;
}

void read_char(Lexer *l) {
  if (l->readPosition >= (int)(strlen(l->input))) {
    l->ch = 0;
  } else {
    l->ch = l->input[l->readPosition];
  }
  l->position = l->readPosition;
  l->readPosition++;
}

Token next_token(Lexer *l) {
  Token t;
  switch (l->ch) {
  case '=':
    t = token_init(ASSIGN, l->ch);
    break;
  case '+':
    t = token_init(PLUS, l->ch);
    break;
  case ',':
    t = token_init(COMMA, l->ch);
    break;
  case ';':
    t = token_init(SEMICOLON, l->ch);
    break;
  case '(':
    t = token_init(LPAREN, l->ch);
    break;
  case ')':
    t = token_init(RPAREN, l->ch);
    break;
  case '{':
    t = token_init(LBRACE, l->ch);
    break;
  case '}':
    t = token_init(RBRACE, l->ch);
    break;
  case 0:
    t.literal = '\0';
    t.type = EOF_TOKEN;
    break;
  default:
    t = token_init(ILLEGAL, l->ch);
  }
  read_char(l);
  return t;
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <input_string>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *input = argv[1];
  Lexer l = lexer_init(input);

  read_char(&l);
  // print_lexer(l);

  Token t;
  while (true) {
    t = next_token(&l);
    // print_lexer(l);

    if (strcmp(t.type, EOF_TOKEN) == 0) {
      print_token(t);
      break;
    } else {
      print_token(t);
    }
  }

  return EXIT_SUCCESS;
}
