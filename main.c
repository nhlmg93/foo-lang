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

typedef struct Interpreter {
  Token *token_buf;
  int token_count;
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
 * Arena Allocator - just one permanent region
 * ----------------------------------------------------------------------------
 */

#define MB (1024 * 1024)
#define MAX_TOKENS 1024
#define ARENA_SIZE (1 * MB)

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
  assert(new_offset <= ARENA_SIZE);
  arena_offset = new_offset;
  return p;
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

// Read identifier starting at pos, null-terminate it in place, return position
// of last char processed. Sets tok->v to point into input buffer.
int read_ident(Interpreter *intpr, KeywordMap *keywords, char *input, int pos) {
  int start = pos++;
  while (isalpha(input[pos]))
    pos++;

  // Null-terminate the identifier in place (destructive tokenization)
  input[pos] = '\0';

  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = &input[start];
  tok->type = kw_get(keywords, tok->v);

  return pos;
}

int read_number(Interpreter *intpr, char *input, int pos) {
  int start = pos;
  while (isdigit(input[pos]))
    pos++;

  // Null-terminate the number in place (destructive tokenization)
  input[pos] = '\0';

  Token *tok = &intpr->token_buf[intpr->token_count];
  tok->v = &input[start];
  tok->type = INT;

  return pos;
}

char peek_char(const char *input, int pos) {
  return input[pos + 1]; // Returns '\0' if past end (null terminator)
}

void tokenize(Interpreter *intpr, KeywordMap *keywords, char *input) {
  for (int i = 0; input[i] != '\0'; i++) {
    char ch = input[i];
    if (isspace(ch))
      continue;

    assert(intpr->token_count < MAX_TOKENS);
    Token *tok = &intpr->token_buf[intpr->token_count];

    switch (ch) {
    case '+':
      tok->v = "+";
      tok->type = PLUS;
      break;
    case '=':
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
      tok->v = ";";
      tok->type = SEMICOLON;
      break;
    case '{':
      tok->v = "{";
      tok->type = LBRACE;
      break;
    case '}':
      tok->v = "}";
      tok->type = RBRACE;
      break;
    case '(':
      tok->v = "(";
      tok->type = LPAREN;
      break;
    case ')':
      tok->v = ")";
      tok->type = RPAREN;
      break;
    case ',':
      tok->v = ",";
      tok->type = COMMA;
      break;
    case '-':
      tok->v = "-";
      tok->type = MINUS;
      break;
    case '!':
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
      tok->v = "/";
      tok->type = SLASH;
      break;
    case '*':
      tok->v = "*";
      tok->type = ASTERISK;
      break;
    case '<':
      tok->v = "<";
      tok->type = LT;
      break;
    case '>':
      tok->v = ">";
      tok->type = GT;
      break;
    default:
      if (isalpha(ch)) {
        i = read_ident(intpr, keywords, input, i);
      } else if (isdigit(ch)) {
        i = read_number(intpr, input, i);
      } else {
        // For illegal tokens, just point to the char itself
        static char illegal_buf[2]; // Not thread-safe, but simple
        illegal_buf[0] = ch;
        illegal_buf[1] = '\0';
        tok->v = illegal_buf;
        tok->type = ILLEGAL;
      }
      break;
    }

    debug("token_type= \"%s\" token= \"%s\"\n", token_type_to_str(tok->type),
          tok->v);
    intpr->token_count++;
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
  KeywordMap *keywords = kw_new(16);
  kw_insert(keywords, "fn", FUNCTION);
  kw_insert(keywords, "let", LET);
  kw_insert(keywords, "true", TRUE);
  kw_insert(keywords, "false", FALSE);
  kw_insert(keywords, "if", IF);
  kw_insert(keywords, "else", ELSE);
  kw_insert(keywords, "return", RETURN);

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
    Token tokens[MAX_TOKENS];
    Interpreter intpr = {.token_buf = tokens, .token_count = 0};

    printf(">>> ");
    while (fgets(line, sizeof(line), stdin) != NULL) {
      size_t temp_base = arena_offset;
      intpr.token_count = 0;

      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
      }

      tokenize(&intpr, keywords, line);

      printf(">>> ");
    }
    return EXIT_SUCCESS;
  }

  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
    print_usage(argv[0]);
    return EXIT_SUCCESS;
  }

  // File mode - file buffer from arena, tokens on stack
  char *source = read_file(argv[1]);
  if (!source) {
    return EXIT_FAILURE;
  }

  Token tokens[MAX_TOKENS];
  Interpreter intpr = {.token_buf = tokens, .token_count = 0};
  tokenize(&intpr, keywords, source);

  return EXIT_SUCCESS;
}
