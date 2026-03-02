#ifndef LEXER_C_
#define LEXER_C_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t byte;

typedef struct Lexer {
  const char *input;
  int position;     // current pos in input
  int readPosition; // current reading pos
  byte ch;          // current char
} Lexer;

void print_lexer(Lexer l);
Lexer lexer_init(const char *input);
void read_char(Lexer *l);
void skip_whitespace(Lexer *l);
char *read_identifier(Lexer *l, Arena *arena);
Token next_token(Lexer *l, Arena *arena);

#ifdef LEXER_IMPLEMENTATION

void print_lexer(Lexer l) {
  printf(
      "Lexer {.input= \"%s\", .position= %d, .readPosition= %d, .ch= '%c'}\n",
      l.input, l.position, l.readPosition, l.ch);
}

Lexer lexer_init(const char *input) {
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

static bool is_letter(char ch) {
  return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}

static inline bool is_whitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

void skip_whitespace(Lexer *l) {
  while (is_whitespace(l->ch)) {
    read_char(l);
  }
}

char *read_identifier(Lexer *l, Arena *arena) {
  int start = l->position;
  while (is_letter(l->ch)) {
    read_char(l);
  }
  int len = l->position - start;
  char *ident = arena_alloc(arena, len + 1);
  if (ident) {
    memcpy(ident, l->input + start, len);
    ident[len] = '\0';
  }
  return ident;
}

Token next_token(Lexer *l, Arena *arena) {
  skip_whitespace(l);

  Token t;
  switch (l->ch) {
  case '=':
    t = token_init(ASSIGN, "=");
    break;
  case '+':
    t = token_init(PLUS, "+");
    break;
  case ',':
    t = token_init(COMMA, ",");
    break;
  case ';':
    t = token_init(SEMICOLON, ";");
    break;
  case '(':
    t = token_init(LPAREN, "(");
    break;
  case ')':
    t = token_init(RPAREN, ")");
    break;
  case '{':
    t = token_init(LBRACE, "{");
    break;
  case '}':
    t = token_init(RBRACE, "}");
    break;
  case 0:
    t.literal[0] = '\0';
    t.type = EOF_TOKEN;
    break;
  default:
    if (is_letter(l->ch)) {
      char *ident = read_identifier(l, arena);
      strncpy(t.literal, ident, 255);
      t.literal[255] = '\0';
      t.type = lookup_ident(t.literal);
      return t;
    } else {
      char illegal_str[2] = {(char)l->ch, '\0'};
      t = token_init(ILLEGAL, illegal_str);
    }
  }
  read_char(l);
  return t;
}

#endif // LEXER_IMPLEMENTATION

#endif // LEXER_C_
