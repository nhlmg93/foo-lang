#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef char *TokenType;

struct Token {
  TokenType type;
  char literal;
} Token;

struct Token newToken(TokenType type, char literal) {
  struct Token t;
  t.type = type;
  t.literal = literal;
  return t;
}

void printToken(struct Token t) {
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

struct Lexer {
  char *input;
  int position;     // current pos in input
  int readPosition; // current reading pos
  byte ch;          // current char
} Lexer;

void printLexer(struct Lexer l) {
  printf(
      "Lexer {.input= \"%s\", .position= %d, .readPosition= %d, .ch= '%c'}\n",
      l.input, l.position, l.readPosition, l.ch);
}

struct Lexer newLexer(char *input) {
  struct Lexer l;
  memset(&l, 0, sizeof(struct Lexer));
  l.input = input;
  return l;
}

void readChar(struct Lexer *l) {
  if (l->readPosition >= (int)(strlen(l->input))) {
    l->ch = 0;
  } else {
    l->ch = l->input[l->readPosition];
  }
  l->position = l->readPosition;
  l->readPosition++;
}

struct Token nextToken(struct Lexer *l) {
  struct Token t;
  switch (l->ch) {
  case '=':
    t = newToken(ASSIGN, l->ch);
    break;
  case '+':
    t = newToken(PLUS, l->ch);
    break;
  case ',':
    t = newToken(COMMA, l->ch);
    break;
  case ';':
    t = newToken(SEMICOLON, l->ch);
    break;
  case '(':
    t = newToken(LPAREN, l->ch);
    break;
  case ')':
    t = newToken(RPAREN, l->ch);
    break;
  case '{':
    t = newToken(LBRACE, l->ch);
    break;
  case '}':
    t = newToken(RBRACE, l->ch);
    break;
  case 0:
    t.literal = '\0';
    t.type = EOF_TOKEN;
    break;
  default:
    t = newToken(ILLEGAL, l->ch);
  }
  readChar(l);
  return t;
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <input_string>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *input = argv[1];
  struct Lexer l = newLexer(input);

  readChar(&l);
  //printLexer(l);

  struct Token t;
  while (true) {
    t = nextToken(&l);
    //printLexer(l);

    if (strcmp(t.type, EOF_TOKEN) == 0) {
      printToken(t);
      break;
    } else {
      printToken(t);
    }
  }

  return EXIT_SUCCESS;
}
