#define _GNU_SOURCE
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

const char *token_type_to_str(TokenType t) {
  return TOKEN_STRINGS[t];
}

typedef struct Token {
  TokenType type;
  char *v;
} Token;

typedef struct Interpreter {
  Token *token_buf;
  int token_count;
} Interpreter;

/* ============================================================================
 * UTILS
 * ============================================================================
 */

/* ----------------------------------------------------------------------------
 * Arena Allocator
 * ----------------------------------------------------------------------------
 */

#define MB (1024 * 1024)
#define MAX_TOKENS 1024
static size_t arena_offset = 0;
static unsigned char arena_buf[1 * MB];

unsigned char *align(unsigned char *p, size_t alignment) {
  uintptr_t addr = (uintptr_t)p;
  return (unsigned char *)((addr + (alignment - 1)) &
                           ~(uintptr_t)(alignment - 1));
}

void *arena_alloc(size_t size) {
  unsigned char *p = align(&arena_buf[arena_offset], sizeof(void *));
  size_t new_offset = (size_t)(p - arena_buf) + size;
  assert(new_offset <= sizeof(arena_buf));
  arena_offset = new_offset;
  return p;
}

// Allocate and copy a string of exactly `len` characters (+ null terminator)
char *arena_strndup(const char *src, size_t len) {
  char *dst = arena_alloc(len + 1);
  memcpy(dst, src, len);
  dst[len] = '\0';
  return dst;
}
/* ----------------------------------------------------------------------------
 * Hash Table
 * ----------------------------------------------------------------------------
 */

uint32_t hash_string(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

typedef struct {
  const char *key;
  TokenType value;
  bool occupied;
} KeywordEntry;

typedef struct {
  KeywordEntry *entries;
  size_t capacity;
} KeywordMap;

KeywordMap *kw_new(size_t capacity) {
  KeywordMap *kw = arena_alloc(sizeof(KeywordMap));
  kw->entries = arena_alloc(sizeof(KeywordEntry) * capacity);
  kw->capacity = capacity;
  for (size_t i = 0; i < capacity; i++) {
    kw->entries[i].occupied = false;
  }
  return kw;
}

void kw_insert(KeywordMap *kw, const char *key, TokenType value) {
  uint32_t hash = hash_string(key);
  size_t idx = hash & (kw->capacity - 1);

  while (kw->entries[idx].occupied) {
    if (strcmp(kw->entries[idx].key, key) == 0) {
      kw->entries[idx].value = value;
      return;
    }
    idx = (idx + 1) & (kw->capacity - 1);
  }

  kw->entries[idx].key = key;
  kw->entries[idx].value = value;
  kw->entries[idx].occupied = true;
}

TokenType kw_get(KeywordMap *kw, const char *key) {
  uint32_t hash = hash_string(key);
  size_t idx = hash & (kw->capacity - 1);

  while (kw->entries[idx].occupied) {
    if (strcmp(kw->entries[idx].key, key) == 0) {
      return kw->entries[idx].value;
    }
    idx = (idx + 1) & (kw->capacity - 1);
  }
  return IDENT;
}

/* ----------------------------------------------------------------------------
 * Debug
 * ----------------------------------------------------------------------------
 */

__attribute__((format(printf, 1, 2))) void debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *tmp;
  if (vasprintf(&tmp, fmt, args) < 0)
    return;
  va_end(args);
  printf("%s", tmp);
  free(tmp);
}

/* ============================================================================
 * UTILS END
 * ============================================================================
 */

int read_ident(Interpreter *intpr, KeywordMap *keywords, const char *input,
               int pos) {
  int start = pos++;
  while (isalpha(input[pos]))
    pos++;
  int len = pos - start;

  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = arena_strndup(&input[start], len);
  tok->type = kw_get(keywords, tok->v);
  return --pos;
}

int read_number(Interpreter *intpr, const char *input, int pos) {
  int start = pos;
  while (isdigit(input[pos]))
    pos++;
  int len = pos - start;

  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = arena_strndup(&input[start], len);
  tok->type = INT;
  return --pos;
}

char peek_char(const char *input, int pos) {
  if ((size_t)(pos + 1) >= strlen(input))
    return '\0';
  else
    return input[pos + 1];
}

int main() {
  const char *input = R"(
let five = 5;
let ten = 10;
let add = fn(x, y) {
    x + y;
};
let result = add(five, ten);
!-/*5;
5 < 10 > 5;
if (5 < 10) {
    return true;
} else {
    return false;
}

10 == 10;
10 != 9;
)";

  KeywordMap *keywords = kw_new(16);
  kw_insert(keywords, "fn", FUNCTION);
  kw_insert(keywords, "let", LET);
  kw_insert(keywords, "true", TRUE);
  kw_insert(keywords, "false", FALSE);
  kw_insert(keywords, "if", IF);
  kw_insert(keywords, "else", ELSE);
  kw_insert(keywords, "return", RETURN);

  Interpreter intpr = {0};
  intpr.token_buf = arena_alloc(MAX_TOKENS * sizeof(Token));

  for (int i = 0; input[i] != '\0'; i++) {
    char ch = input[i];
    if (isspace(ch))
      continue;

    assert(intpr.token_count < MAX_TOKENS);
    Token *tok = &intpr.token_buf[intpr.token_count];

    switch (ch) {
    case '+':
      tok->v = arena_strndup("+", 1);
      tok->type = PLUS;
      break;
    case '=':
      if (peek_char(input, i) == '=') {
        i++;
        tok->v = arena_strndup("==", 2);
        tok->type = EQUAL;
      } else {
        tok->v = arena_strndup("=", 1);
        tok->type = ASSIGN;
      }
      break;
    case ';':
      tok->v = arena_strndup(";", 1);
      tok->type = SEMICOLON;
      break;
    case '{':
      tok->v = arena_strndup("{", 1);
      tok->type = LBRACE;
      break;
    case '}':
      tok->v = arena_strndup("}", 1);
      tok->type = RBRACE;
      break;
    case '(':
      tok->v = arena_strndup("(", 1);
      tok->type = LPAREN;
      break;
    case ')':
      tok->v = arena_strndup(")", 1);
      tok->type = RPAREN;
      break;
    case ',':
      tok->v = arena_strndup(",", 1);
      tok->type = COMMA;
      break;
    case '-':
      tok->v = arena_strndup("-", 1);
      tok->type = MINUS;
      break;
    case '!':
      if (peek_char(input, i) == '=') {
        i++;
        tok->v = arena_strndup("!=", 2);
        tok->type = NOT_EQUAL;
      } else {
        tok->v = arena_strndup("!", 1);
        tok->type = BANG;
      }
      break;
    case '/':
      tok->v = arena_strndup("/", 1);
      tok->type = SLASH;
      break;
    case '*':
      tok->v = arena_strndup("*", 1);
      tok->type = ASTERISK;
      break;
    case '<':
      tok->v = arena_strndup("<", 1);
      tok->type = LT;
      break;
    case '>':
      tok->v = arena_strndup(">", 1);
      tok->type = GT;
      break;
    default:
      if (isalpha(ch)) {
        i = read_ident(&intpr, keywords, input, i);
      } else if (isdigit(ch)) {
        i = read_number(&intpr, input, i);
      } else {
        tok->v = arena_strndup(&ch, 1);
        tok->type = ILLEGAL;
      }
      break;
    }

    debug("token_type= \"%s\" token= \"%s\"\n", token_type_to_str(tok->type),
          tok->v);
    intpr.token_count++;
  }

  return EXIT_SUCCESS;
}
