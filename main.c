#define STB_DS_IMPLEMENTATION
#include <assert.h>
#include <ctype.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vendor/stb_ds.h"

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
  } data;
} Stmt;

// Minimal expression (placeholder)
struct Expr {
  int placeholder;
};

typedef struct Interpreter {
  // Lexer
  Token *token_buf;

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

static stbds_string_arena string_arena = {0};

/* ----------------------------------------------------------------------------
 * Keywords
 * ----------------------------------------------------------------------------
 */

typedef struct {
  const char *key;
  TokenType value;
} KeywordEntry;

static KeywordEntry *keyword_map = NULL;

static void keyword_map_init(void) {
  if (keyword_map)
    return;

  static const KeywordEntry KEYWORDS[] = {
      {"fn", FUNCTION}, {"let", LET},   {"true", TRUE},     {"false", FALSE},
      {"if", IF},       {"else", ELSE}, {"return", RETURN},
  };

  shdefault(keyword_map, IDENT);
  for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++)
    shput(keyword_map, KEYWORDS[i].key, KEYWORDS[i].value);
}

TokenType keyword_lookup(const char *key) {
  keyword_map_init();
  return shget(keyword_map, key);
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

static Token *new_token(Interpreter *intpr) {
  Token tok = {0};
  arrput(intpr->token_buf, tok);
  return &intpr->token_buf[arrlen(intpr->token_buf) - 1];
}

int read_ident(Interpreter *intpr, char *input, int pos) {
  int start = pos;
  while (isalpha(input[pos]))
    pos++;

  int len = pos - start;
  char buf[len + 1];
  memcpy(buf, &input[start], len);
  buf[len] = '\0';
  char *ident = stralloc(&string_arena, buf);
  assert(ident && "out of memory");

  Token *tok = new_token(intpr);
  tok->v = ident;
  tok->type = keyword_lookup(tok->v);

  return pos - 1;
}

int read_number(Interpreter *intpr, char *input, int pos) {
  int start = pos;
  while (isdigit(input[pos]))
    pos++;

  int len = pos - start;
  char buf[len + 1];
  memcpy(buf, &input[start], len);
  buf[len] = '\0';
  char *num = stralloc(&string_arena, buf);
  assert(num && "out of memory");

  Token *tok = new_token(intpr);
  tok->v = num;
  tok->type = INT;

  return pos - 1;
}

static Token *push_token(Interpreter *intpr, TokenType type) {
  Token *tok = new_token(intpr);
  tok->type = type;
  tok->v = TOKEN_STRINGS[type];
  return tok;
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
      tok = push_token(intpr, PLUS);
      break;
    case '=':
      if (peek_char(input, i) == '=') {
        i++;
        tok = push_token(intpr, EQUAL);
      } else {
        tok = push_token(intpr, ASSIGN);
      }
      break;
    case ';':
      tok = push_token(intpr, SEMICOLON);
      break;
    case '{':
      tok = push_token(intpr, LBRACE);
      break;
    case '}':
      tok = push_token(intpr, RBRACE);
      break;
    case '(':
      tok = push_token(intpr, LPAREN);
      break;
    case ')':
      tok = push_token(intpr, RPAREN);
      break;
    case ',':
      tok = push_token(intpr, COMMA);
      break;
    case '-':
      tok = push_token(intpr, MINUS);
      break;
    case '!':
      if (peek_char(input, i) == '=') {
        i++;
        tok = push_token(intpr, NOT_EQUAL);
      } else {
        tok = push_token(intpr, BANG);
      }
      break;
    case '/':
      tok = push_token(intpr, SLASH);
      break;
    case '*':
      tok = push_token(intpr, ASTERISK);
      break;
    case '<':
      tok = push_token(intpr, LT);
      break;
    case '>':
      tok = push_token(intpr, GT);
      break;
    default:
      if (isalpha(ch)) {
        i = read_ident(intpr, input, i);
        tok = &intpr->token_buf[arrlen(intpr->token_buf) - 1];
      } else if (isdigit(ch)) {
        i = read_number(intpr, input, i);
        tok = &intpr->token_buf[arrlen(intpr->token_buf) - 1];
      } else {
        tok = new_token(intpr);
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

static void parser_advance(Interpreter *intpr) {
  intpr->curr = intpr->next;
  intpr->pos++;
  if (intpr->pos + 1 < arrlen(intpr->token_buf))
    intpr->next = intpr->token_buf[intpr->pos + 1];
  else
    intpr->next = (Token){END, "EOF"};
}

void parse(Interpreter *intpr) {
  // Empty input
  if (arrlen(intpr->token_buf) == 0) {
    intpr->pos = 0;
    intpr->curr = (Token){END, "EOF"};
    intpr->next = (Token){END, "EOF"};
    return;
  }
  intpr->pos = 0;
  intpr->curr = intpr->token_buf[0];
  intpr->next = (arrlen(intpr->token_buf) > 1) ? intpr->token_buf[1]
                                               : (Token){END, "EOF"};
  while (intpr->curr.type != END) {
    debug("curr = %s (%s)\n",
          token_type_to_str(intpr->curr.type), intpr->curr.v);
    switch (intpr->curr.type) {
    case LET:
      // parse let statement;
      parser_advance(intpr);
      break;
    default:
      parser_advance(intpr);
      break;
    }
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

  char *buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    fprintf(stderr, "error: out of memory\n");
    return NULL;
  }
  size_t read = fread(buf, 1, (size_t)size, f);
  buf[read] = '\0';
  fclose(f);

  return buf;
}

int main(int argc, char *argv[]) {
  if (argc > 2) {
    fprintf(stderr, "error: too many arguments\n");
    return EXIT_FAILURE;
  }

  if (argc == 1) {
    char line[1024];
    Interpreter intpr = {0};

    printf(">>> ");
    while (fgets(line, sizeof(line), stdin) != NULL) {
      arrfree(intpr.token_buf);
      intpr.token_buf = NULL;
      strreset(&string_arena);

      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
      }

      tokenize(&intpr, line);
      parse(&intpr);

      printf(">>> ");
    }
    return EXIT_SUCCESS;
  }

  char *source = read_file(argv[1]);
  if (!source) {
    return EXIT_FAILURE;
  }

  Interpreter intpr = {0};
  tokenize(&intpr, source);
  parse(&intpr);
  return EXIT_SUCCESS;
}
