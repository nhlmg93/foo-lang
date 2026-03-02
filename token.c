#ifndef TOKEN_C_
#define TOKEN_C_

#include <stdio.h>
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

typedef char *TokenType;

typedef struct Token {
  TokenType type;
  char literal[256];
} Token;

TokenType lookup_ident(char *ident);

#ifdef TOKEN_IMPLEMENTATION

Token token_init(TokenType type, const char *str) {
  Token t;
  t.type = type;
  strncpy(t.literal, str, 255);
  t.literal[255] = '\0';
  return t;
}

void print_token(Token t) {
  printf("Token {.type= \"%s\", .literal= '%s'}\n", t.type, t.literal);
}

TokenType lookup_ident(char *ident) {
  if (strcmp(ident, "let") == 0)
    return LET;
  if (strcmp(ident, "fn") == 0)
    return FUNCTION;
  return IDENT;
}

#endif // TOKEN_IMPLEMENTATION

#endif // TOKEN_C_
