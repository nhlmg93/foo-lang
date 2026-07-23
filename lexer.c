#ifndef FOO_LEXER_H
#define FOO_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_SOURCE_BYTES ((size_t)1 * 1024 * 1024)
#define MAX_TOKENS ((size_t)32 * 1024)

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

typedef struct {
  TokenType type;
  const char *v;
  uint32_t line;
  uint32_t column;
} Token;

typedef struct {
  Token *tokens;
} Lexer;

const char *token_type_to_str(TokenType type);
void lexer_init(Lexer *lexer);
void lexer_clear(Lexer *lexer);
void lexer_tokenize(Lexer *lexer, char *input);

#endif

#ifdef LEXER_IMPLEMENTATION

static const char *const TOKEN_STRINGS[] = {
#define X(kind, str) str,
    TOKEN_TABLE
#undef X
};

typedef struct {
  const char *key;
  TokenType value;
} KeywordEntry;

static stbds_string_arena lexer_strings;
static KeywordEntry *keyword_map;

const char *token_type_to_str(TokenType type) { return TOKEN_STRINGS[type]; }

void lexer_init(Lexer *lexer) {
  *lexer = (Lexer){0};
  arrsetcap(lexer->tokens, MAX_TOKENS);
}

void lexer_clear(Lexer *lexer) {
  stbds_header(lexer->tokens)->length = 0;
}

static void keyword_map_init(void) {
  if (keyword_map != NULL)
    return;

  static const KeywordEntry KEYWORDS[] = {
      {"fn", FUNCTION}, {"let", LET},   {"true", TRUE},
      {"false", FALSE}, {"if", IF},     {"else", ELSE},
      {"return", RETURN},
  };

  shdefault(keyword_map, IDENT);
  for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++)
    shput(keyword_map, KEYWORDS[i].key, KEYWORDS[i].value);
}

static TokenType keyword_lookup(const char *key) {
  keyword_map_init();
  return shget(keyword_map, key);
}

static void new_token(Lexer *lexer, TokenType type, const char *value,
                      uint32_t line, uint32_t column) {
  PANIC(arrlenu(lexer->tokens) < MAX_TOKENS,
        "token capacity exceeded (capacity=%zu)", MAX_TOKENS);

  arrput(lexer->tokens, ((Token){
                            .type = type,
                            .v = value,
                            .line = line,
                            .column = column,
                        }));
}

static bool is_identifier_start(char ch) {
  const unsigned char value = (unsigned char)ch;
  return ch == '_' || (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z');
}

static bool is_identifier_char(char ch) {
  const unsigned char value = (unsigned char)ch;
  return is_identifier_start(ch) || (value >= '0' && value <= '9');
}

static bool is_digit(char ch) {
  const unsigned char value = (unsigned char)ch;
  return value >= '0' && value <= '9';
}

static bool is_horizontal_space(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\v' || ch == '\f';
}

static size_t source_length(const char *input) {
  for (size_t length = 0; length <= MAX_SOURCE_BYTES; length++) {
    if (input[length] == '\0')
      return length;
  }
  PANIC(false, "source exceeds capacity (capacity=%zu)", MAX_SOURCE_BYTES);
}

static char *copy_lexeme(char *input, size_t start, size_t end) {
  const char saved = input[end];
  input[end] = '\0';
  char *const result = stralloc(&lexer_strings, &input[start]);
  input[end] = saved;
  return result;
}

static void push_token(Lexer *lexer, TokenType type, uint32_t line,
                       uint32_t column) {
  new_token(lexer, type, TOKEN_STRINGS[type], line, column);
}

void lexer_tokenize(Lexer *lexer, char *input) {
  const size_t length = source_length(input);
  size_t pos = 0;
  uint32_t line = 1;
  uint32_t column = 1;

  while (pos < length) {
    const char ch = input[pos];
    if (ch == '\n') {
      pos++;
      line++;
      column = 1;
      continue;
    }
    if (is_horizontal_space(ch)) {
      pos++;
      column++;
      continue;
    }

    const uint32_t token_column = column;
    switch (ch) {
    case '+':
      push_token(lexer, PLUS, line, token_column);
      break;
    case '=':
      if (pos + 1 < length && input[pos + 1] == '=') {
        push_token(lexer, EQUAL, line, token_column);
        pos++;
        column++;
      } else {
        push_token(lexer, ASSIGN, line, token_column);
      }
      break;
    case ';':
      push_token(lexer, SEMICOLON, line, token_column);
      break;
    case '{':
      push_token(lexer, LBRACE, line, token_column);
      break;
    case '}':
      push_token(lexer, RBRACE, line, token_column);
      break;
    case '(':
      push_token(lexer, LPAREN, line, token_column);
      break;
    case ')':
      push_token(lexer, RPAREN, line, token_column);
      break;
    case ',':
      push_token(lexer, COMMA, line, token_column);
      break;
    case '-':
      push_token(lexer, MINUS, line, token_column);
      break;
    case '!':
      if (pos + 1 < length && input[pos + 1] == '=') {
        push_token(lexer, NOT_EQUAL, line, token_column);
        pos++;
        column++;
      } else {
        push_token(lexer, BANG, line, token_column);
      }
      break;
    case '/':
      push_token(lexer, SLASH, line, token_column);
      break;
    case '*':
      push_token(lexer, ASTERISK, line, token_column);
      break;
    case '<':
      push_token(lexer, LT, line, token_column);
      break;
    case '>':
      push_token(lexer, GT, line, token_column);
      break;
    default:
      if (is_identifier_start(ch)) {
        const size_t start = pos;
        while (pos < length && is_identifier_char(input[pos]))
          pos++;
        char *const ident = copy_lexeme(input, start, pos);
        new_token(lexer, keyword_lookup(ident), ident, line, token_column);
        column += (uint32_t)(pos - start);
        continue;
      }
      if (is_digit(ch)) {
        const size_t start = pos;
        while (pos < length && is_digit(input[pos]))
          pos++;
        char *const number = copy_lexeme(input, start, pos);
        new_token(lexer, INT, number, line, token_column);
        column += (uint32_t)(pos - start);
        continue;
      }
      PANIC(false, "%u:%u: illegal character '%c'", line, token_column, ch);
    }

    pos++;
    column++;
  }

  push_token(lexer, END, line, column);
}

#endif
