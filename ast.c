#ifndef FOO_AST_H
#define FOO_AST_H

#include <stdbool.h>
#include <stdint.h>

#include "lexer.c"

#define MAX_AST_NODES ((uint32_t)32 * 1024)
#define MAX_STATEMENTS ((uint32_t)32 * 1024)
#define MAX_LIST_ITEMS ((uint32_t)32 * 1024)

typedef struct Expr Expr;

typedef enum : uint8_t {
  STMT_LET,
  STMT_RETURN,
  STMT_EXPR,
} StmtType;

typedef enum : uint8_t {
  EXPR_IDENT,
  EXPR_INT,
  EXPR_BOOL,
  EXPR_PREFIX,
  EXPR_INFIX,
  EXPR_IF,
  EXPR_FUNCTION,
  EXPR_CALL,
} ExprType;

typedef struct Stmt {
  StmtType type;
  Token token;
  union {
    struct {
      const char *name;
      Expr *value;
    } let;
    struct {
      Expr *value;
    } ret;
    struct {
      Expr *value;
    } expr;
  } data;
} Stmt;

struct Expr {
  ExprType type;
  Token token;
  union {
    const char *ident;
    int64_t integer;
    bool boolean;
    struct {
      TokenType op;
      Expr *right;
    } prefix;
    struct {
      Expr *left;
      TokenType op;
      Expr *right;
    } infix;
    struct {
      Expr *condition;
      Stmt *consequence;
      Stmt *alternative;
    } if_expr;
    struct {
      const char **parameters;
      Stmt *body;
    } function;
    struct {
      Expr *function;
      Expr **arguments;
    } call;
  } data;
};

typedef struct {
  Stmt *stmts;
  uint32_t expr_count;
  uint32_t stmt_count;
  uint32_t list_item_count;
} Ast;

void ast_init(Ast *ast);
void ast_clear(Ast *ast);
Expr *ast_new_expr(Ast *ast, ExprType type, Token token);
void ast_append_statement(Ast *ast, Stmt **stmts, Stmt stmt);
void ast_append_expression(Ast *ast, Expr ***expressions, Expr *expr);
void ast_append_string(Ast *ast, const char ***strings, const char *value);

#endif

#ifdef AST_IMPLEMENTATION

static void ast_capacity_exceeded(const char *kind, uint32_t capacity) {
  PANIC(false, "AST %s capacity exceeded (capacity=%u)", kind, capacity);
}

void ast_init(Ast *ast) { *ast = (Ast){0}; }

void ast_clear(Ast *ast) {
  if (ast->stmts != NULL)
    stbds_header(ast->stmts)->length = 0;
  ast->expr_count = 0;
  ast->stmt_count = 0;
  ast->list_item_count = 0;
}

Expr *ast_new_expr(Ast *ast, ExprType type, Token token) {
  if (ast->expr_count >= MAX_AST_NODES)
    ast_capacity_exceeded("expression", MAX_AST_NODES);

  Expr *const expr = arena_alloc(sizeof(*expr));
  *expr = (Expr){
      .type = type,
      .token = token,
  };
  ast->expr_count++;
  return expr;
}

void ast_append_statement(Ast *ast, Stmt **stmts, Stmt stmt) {
  if (ast->stmt_count >= MAX_STATEMENTS)
    ast_capacity_exceeded("statement", MAX_STATEMENTS);
  arrput(*stmts, stmt);
  ast->stmt_count++;
}

void ast_append_expression(Ast *ast, Expr ***expressions, Expr *expr) {
  if (ast->list_item_count >= MAX_LIST_ITEMS)
    ast_capacity_exceeded("list item", MAX_LIST_ITEMS);
  arrput(*expressions, expr);
  ast->list_item_count++;
}

void ast_append_string(Ast *ast, const char ***strings, const char *value) {
  if (ast->list_item_count >= MAX_LIST_ITEMS)
    ast_capacity_exceeded("list item", MAX_LIST_ITEMS);
  arrput(*strings, value);
  ast->list_item_count++;
}

#endif
