#ifndef FOO_PARSER_H
#define FOO_PARSER_H

#include <stdarg.h>

#include "ast.c"

#define MAX_PARSE_DEPTH ((uint32_t)256)

typedef enum : uint8_t {
  PREC_LOWEST,
  PREC_EQUALS,
  PREC_LESS_GREATER,
  PREC_SUM,
  PREC_PRODUCT,
  PREC_PREFIX,
  PREC_CALL,
} Precedence;

typedef struct {
  Token *tokens;
  uint32_t pos;
  Token curr;
  Token next;
  uint32_t depth;
  Ast ast;
} Parser;

void parser_init(Parser *parser);
void parser_parse(Parser *parser, Token *tokens);

#endif

#ifdef PARSER_IMPLEMENTATION

static Token end_token(void) {
  return (Token){
      .type = END,
      .v = "EOF",
  };
}

static void parser_advance(Parser *parser) {
  parser->curr = parser->next;
  parser->pos++;
  if ((size_t)parser->pos + 1 < arrlenu(parser->tokens))
    parser->next = parser->tokens[parser->pos + 1];
  else
    parser->next = end_token();
}

[[noreturn]] static void parser_error(Token token, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  PANIC(false, "%u:%u: %s", token.line, token.column, message);
}

static bool parser_expect_next(Parser *parser, TokenType type) {
  if (parser->next.type == type) {
    parser_advance(parser);
    return true;
  }

  parser_error(parser->next, "expected %s, got %s", token_type_to_str(type),
               token_type_to_str(parser->next.type));
}

static Precedence token_precedence(TokenType type) {
  switch (type) {
  case EQUAL:
  case NOT_EQUAL:
    return PREC_EQUALS;
  case LT:
  case GT:
    return PREC_LESS_GREATER;
  case PLUS:
  case MINUS:
    return PREC_SUM;
  case SLASH:
  case ASTERISK:
    return PREC_PRODUCT;
  case LPAREN:
    return PREC_CALL;
  default:
    return PREC_LOWEST;
  }
}

static bool is_infix_operator(TokenType type) {
  switch (type) {
  case PLUS:
  case MINUS:
  case SLASH:
  case ASTERISK:
  case EQUAL:
  case NOT_EQUAL:
  case LT:
  case GT:
    return true;
  default:
    return false;
  }
}

static int64_t parse_integer(Token token) {
  uint64_t value = 0;
  for (size_t i = 0; i <= MAX_SOURCE_BYTES; i++) {
    const char ch = token.v[i];
    if (ch == '\0')
      return (int64_t)value;

    const uint64_t digit = (uint64_t)(ch - '0');
    if (value > ((uint64_t)INT64_MAX - digit) / 10)
      parser_error(token, "integer literal is too large");
    value = value * 10 + digit;
  }
  parser_error(token, "integer literal exceeds source capacity");
}

static Expr *parse_expression(Parser *parser, Precedence precedence);
static bool parse_statement(Parser *parser, Stmt *stmt);

static bool parse_block(Parser *parser, Stmt **stmts) {
  *stmts = NULL;
  parser_advance(parser);

  while (parser->curr.type != RBRACE && parser->curr.type != END) {
    Stmt stmt;
    PANIC(parse_statement(parser, &stmt),
          "parser stopped without producing a block statement");
    ast_append_statement(&parser->ast, stmts, stmt);
    parser_advance(parser);
  }

  if (parser->curr.type == END)
    parser_error(parser->curr, "expected }, got EOF");
  return true;
}

static Expr *parse_grouped_expression(Parser *parser) {
  parser_advance(parser);
  Expr *const expr = parse_expression(parser, PREC_LOWEST);
  PANIC(parser_expect_next(parser, RPAREN),
        "parser failed to close grouped expression");
  return expr;
}

static Expr *parse_prefix_expression(Parser *parser) {
  const Token token = parser->curr;
  const TokenType op = token.type;
  parser_advance(parser);
  Expr *const right = parse_expression(parser, PREC_PREFIX);

  Expr *const expr = ast_new_expr(&parser->ast, EXPR_PREFIX, token);
  expr->data.prefix.op = op;
  expr->data.prefix.right = right;
  return expr;
}

static Expr *parse_if_expression(Parser *parser) {
  const Token token = parser->curr;
  PANIC(parser_expect_next(parser, LPAREN),
        "parser failed to open if condition");

  parser_advance(parser);
  Expr *const condition = parse_expression(parser, PREC_LOWEST);
  PANIC(parser_expect_next(parser, RPAREN) &&
            parser_expect_next(parser, LBRACE),
        "parser failed to open if consequence");

  Stmt *consequence;
  parse_block(parser, &consequence);

  Stmt *alternative = NULL;
  if (parser->next.type == ELSE) {
    parser_advance(parser);
    PANIC(parser_expect_next(parser, LBRACE),
          "parser failed to open else alternative");
    parse_block(parser, &alternative);
  }

  Expr *const expr = ast_new_expr(&parser->ast, EXPR_IF, token);
  expr->data.if_expr.condition = condition;
  expr->data.if_expr.consequence = consequence;
  expr->data.if_expr.alternative = alternative;
  return expr;
}

static void parse_function_parameters(Parser *parser,
                                      const char ***parameters) {
  *parameters = NULL;
  if (parser->next.type == RPAREN) {
    parser_advance(parser);
    return;
  }

  parser_advance(parser);
  if (parser->curr.type != IDENT)
    parser_error(parser->curr, "expected function parameter, got %s",
                 token_type_to_str(parser->curr.type));
  ast_append_string(&parser->ast, parameters, parser->curr.v);

  while (parser->next.type == COMMA) {
    parser_advance(parser);
    parser_advance(parser);
    if (parser->curr.type != IDENT)
      parser_error(parser->curr, "expected function parameter, got %s",
                   token_type_to_str(parser->curr.type));
    ast_append_string(&parser->ast, parameters, parser->curr.v);
  }

  PANIC(parser_expect_next(parser, RPAREN),
        "parser failed to close function parameters");
}

static Expr *parse_function_expression(Parser *parser) {
  const Token token = parser->curr;
  PANIC(parser_expect_next(parser, LPAREN),
        "parser failed to open function parameters");

  const char **parameters;
  parse_function_parameters(parser, &parameters);
  PANIC(parser_expect_next(parser, LBRACE),
        "parser failed to open function body");

  Stmt *body;
  parse_block(parser, &body);

  Expr *const expr = ast_new_expr(&parser->ast, EXPR_FUNCTION, token);
  expr->data.function.parameters = parameters;
  expr->data.function.body = body;
  return expr;
}

static Expr *parse_prefix(Parser *parser) {
  const Token token = parser->curr;
  Expr *expr;

  switch (token.type) {
  case IDENT:
    expr = ast_new_expr(&parser->ast, EXPR_IDENT, token);
    expr->data.ident = token.v;
    return expr;
  case INT:
    expr = ast_new_expr(&parser->ast, EXPR_INT, token);
    expr->data.integer = parse_integer(token);
    return expr;
  case TRUE:
  case FALSE:
    expr = ast_new_expr(&parser->ast, EXPR_BOOL, token);
    expr->data.boolean = token.type == TRUE;
    return expr;
  case BANG:
  case MINUS:
    return parse_prefix_expression(parser);
  case LPAREN:
    return parse_grouped_expression(parser);
  case IF:
    return parse_if_expression(parser);
  case FUNCTION:
    return parse_function_expression(parser);
  default:
    parser_error(token, "expected expression, got %s",
                 token_type_to_str(token.type));
  }
}

static Expr *parse_infix_expression(Parser *parser, Expr *left) {
  const Token token = parser->curr;
  const Precedence precedence = token_precedence(token.type);
  parser_advance(parser);
  Expr *const right = parse_expression(parser, precedence);

  Expr *const expr = ast_new_expr(&parser->ast, EXPR_INFIX, token);
  expr->data.infix.left = left;
  expr->data.infix.op = token.type;
  expr->data.infix.right = right;
  return expr;
}

static void parse_expression_list(Parser *parser, TokenType end,
                                  Expr ***expressions) {
  *expressions = NULL;
  if (parser->next.type == end) {
    parser_advance(parser);
    return;
  }

  parser_advance(parser);
  ast_append_expression(&parser->ast, expressions,
                        parse_expression(parser, PREC_LOWEST));
  while (parser->next.type == COMMA) {
    parser_advance(parser);
    parser_advance(parser);
    ast_append_expression(&parser->ast, expressions,
                          parse_expression(parser, PREC_LOWEST));
  }

  PANIC(parser_expect_next(parser, end),
        "parser failed to close expression list");
}

static Expr *parse_call_expression(Parser *parser, Expr *function) {
  const Token token = parser->curr;
  Expr **arguments;
  parse_expression_list(parser, RPAREN, &arguments);

  Expr *const expr = ast_new_expr(&parser->ast, EXPR_CALL, token);
  expr->data.call.function = function;
  expr->data.call.arguments = arguments;
  return expr;
}

static Expr *parse_expression(Parser *parser, Precedence precedence) {
  PANIC(parser->depth < MAX_PARSE_DEPTH,
        "%u:%u: expression nesting exceeds capacity of %u",
        parser->curr.line, parser->curr.column, MAX_PARSE_DEPTH);

  parser->depth++;
  Expr *left = parse_prefix(parser);
  while (parser->next.type != SEMICOLON &&
         precedence < token_precedence(parser->next.type)) {
    const TokenType next_type = parser->next.type;
    if (!is_infix_operator(next_type) && next_type != LPAREN)
      break;

    parser_advance(parser);
    if (parser->curr.type == LPAREN)
      left = parse_call_expression(parser, left);
    else
      left = parse_infix_expression(parser, left);
  }

  parser->depth--;
  return left;
}

static bool parse_let_statement(Parser *parser, Stmt *stmt) {
  const Token token = parser->curr;
  PANIC(parser_expect_next(parser, IDENT),
        "parser failed to read let binding name");
  const char *const name = parser->curr.v;

  PANIC(parser_expect_next(parser, ASSIGN),
        "parser failed to read let assignment");
  parser_advance(parser);
  Expr *const value = parse_expression(parser, PREC_LOWEST);
  if (parser->next.type == SEMICOLON)
    parser_advance(parser);

  *stmt = (Stmt){
      .type = STMT_LET,
      .token = token,
      .data.let = {.name = name, .value = value},
  };
  return true;
}

static bool parse_return_statement(Parser *parser, Stmt *stmt) {
  const Token token = parser->curr;
  parser_advance(parser);
  Expr *const value = parse_expression(parser, PREC_LOWEST);
  if (parser->next.type == SEMICOLON)
    parser_advance(parser);

  *stmt = (Stmt){
      .type = STMT_RETURN,
      .token = token,
      .data.ret = {.value = value},
  };
  return true;
}

static bool parse_expression_statement(Parser *parser, Stmt *stmt) {
  const Token token = parser->curr;
  Expr *const value = parse_expression(parser, PREC_LOWEST);
  if (parser->next.type == SEMICOLON)
    parser_advance(parser);

  *stmt = (Stmt){
      .type = STMT_EXPR,
      .token = token,
      .data.expr = {.value = value},
  };
  return true;
}

static bool parse_statement(Parser *parser, Stmt *stmt) {
  switch (parser->curr.type) {
  case LET:
    return parse_let_statement(parser, stmt);
  case RETURN:
    return parse_return_statement(parser, stmt);
  default:
    return parse_expression_statement(parser, stmt);
  }
}

void parser_init(Parser *parser) {
  *parser = (Parser){0};
  ast_init(&parser->ast);
}

void parser_parse(Parser *parser, Token *tokens) {
  ast_clear(&parser->ast);
  parser->tokens = tokens;
  parser->depth = 0;

  if (arrlen(tokens) == 0) {
    parser->pos = 0;
    parser->curr = end_token();
    parser->next = end_token();
    return;
  }

  parser->pos = 0;
  parser->curr = tokens[0];
  parser->next = arrlen(tokens) > 1 ? tokens[1] : end_token();
  while (parser->curr.type != END) {
    Stmt stmt;
    PANIC(parse_statement(parser, &stmt),
          "parser stopped without producing a statement");
    ast_append_statement(&parser->ast, &parser->ast.stmts, stmt);
    parser_advance(parser);
  }
}

#endif
