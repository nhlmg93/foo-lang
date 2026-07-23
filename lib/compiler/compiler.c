#ifndef FOO_LIB_COMPILER_H
#define FOO_LIB_COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../ast.c"
#include "bytecode.c"

#define MAX_COMPILER_SCOPES ((uint16_t)256)
#define MAX_COMPILER_SYMBOL_TABLES ((uint32_t)32 * 1024)
#define MAX_COMPILER_SYMBOLS ((uint32_t)32 * 1024)
#define MAX_COMPILE_DEPTH ((uint16_t)256)

typedef enum : uint8_t {
  SYMBOL_GLOBAL,
  SYMBOL_LOCAL,
  SYMBOL_FREE,
  SYMBOL_FUNCTION,
} SymbolScope;

typedef struct {
  const char *name;
  SymbolScope scope;
  uint16_t index;
} Symbol;

typedef struct {
  const char *key;
  Symbol value;
} SymbolEntry;

typedef struct SymbolTable {
  SymbolEntry *entries;
  Symbol *free_symbols;
  struct SymbolTable *outer;
  uint16_t definition_count;
} SymbolTable;

typedef struct {
  Instruction *instructions;
} CompilerScope;

typedef struct {
  VmValue *constants;
  SymbolTable *global_symbols;
  SymbolTable *symbols;
  CompilerScope scopes[MAX_COMPILER_SCOPES];
  uint32_t instruction_count;
  uint32_t symbol_table_count;
  uint32_t symbol_count;
  uint16_t scope_index;
  uint16_t compile_depth;
} Compiler;

void compiler_init(Compiler *compiler);
Bytecode compiler_compile(Compiler *compiler, Stmt *stmts);

#endif

#ifdef COMPILER_IMPLEMENTATION

static SymbolTable *compiler_new_symbol_table(Compiler *compiler,
                                              SymbolTable *outer) {
  PANIC(compiler->symbol_table_count < MAX_COMPILER_SYMBOL_TABLES,
        "compiler symbol table capacity exceeded (capacity=%u)",
        MAX_COMPILER_SYMBOL_TABLES);

  SymbolTable *const table = arena_alloc(sizeof(*table));
  *table = (SymbolTable){.outer = outer};
  compiler->symbol_table_count++;
  return table;
}

static Symbol compiler_define_symbol(Compiler *compiler, const char *name) {
  const ptrdiff_t existing = shgeti(compiler->symbols->entries, name);
  if (existing >= 0 &&
      compiler->symbols->entries[existing].value.scope != SYMBOL_FUNCTION)
    return compiler->symbols->entries[existing].value;

  PANIC(compiler->symbol_count < MAX_COMPILER_SYMBOLS,
        "compiler symbol capacity exceeded (capacity=%u)",
        MAX_COMPILER_SYMBOLS);

  const SymbolScope scope =
      compiler->symbols->outer == NULL ? SYMBOL_GLOBAL : SYMBOL_LOCAL;
  if (scope == SYMBOL_GLOBAL)
    PANIC(compiler->symbols->definition_count < MAX_COMPILER_SYMBOLS,
          "global capacity exceeded (capacity=%u)", MAX_COMPILER_SYMBOLS);
  else
    PANIC(compiler->symbols->definition_count < MAX_FUNCTION_LOCALS,
          "local capacity exceeded (capacity=%u)", MAX_FUNCTION_LOCALS);

  const Symbol symbol = {
      .name = name,
      .scope = scope,
      .index = compiler->symbols->definition_count,
  };
  compiler->symbols->definition_count++;
  shput(compiler->symbols->entries, name, symbol);
  compiler->symbol_count++;
  return symbol;
}

static void compiler_define_function_name(Compiler *compiler,
                                          const char *name) {
  PANIC(compiler->symbol_count < MAX_COMPILER_SYMBOLS,
        "compiler symbol capacity exceeded (capacity=%u)",
        MAX_COMPILER_SYMBOLS);

  const Symbol symbol = {
      .name = name,
      .scope = SYMBOL_FUNCTION,
  };
  shput(compiler->symbols->entries, name, symbol);
  compiler->symbol_count++;
}

static Symbol compiler_define_free(Compiler *compiler, Symbol original) {
  PANIC(arrlenu(compiler->symbols->free_symbols) < MAX_FREE_VALUES,
        "free variable capacity exceeded (capacity=%u)", MAX_FREE_VALUES);
  PANIC(compiler->symbol_count < MAX_COMPILER_SYMBOLS,
        "compiler symbol capacity exceeded (capacity=%u)",
        MAX_COMPILER_SYMBOLS);

  const Symbol symbol = {
      .name = original.name,
      .scope = SYMBOL_FREE,
      .index = (uint16_t)arrlenu(compiler->symbols->free_symbols),
  };
  arrput(compiler->symbols->free_symbols, original);
  shput(compiler->symbols->entries, original.name, symbol);
  compiler->symbol_count++;
  return symbol;
}

static bool compiler_resolve_symbol(Compiler *compiler, SymbolTable *table,
                                    const char *name, Symbol *symbol) {
  const ptrdiff_t index = shgeti(table->entries, name);
  if (index >= 0) {
    *symbol = table->entries[index].value;
    return true;
  }
  if (table->outer == NULL)
    return false;

  Symbol outer_symbol;
  if (!compiler_resolve_symbol(compiler, table->outer, name, &outer_symbol))
    return false;
  if (outer_symbol.scope == SYMBOL_GLOBAL) {
    *symbol = outer_symbol;
    return true;
  }

  SymbolTable *const current = compiler->symbols;
  compiler->symbols = table;
  *symbol = compiler_define_free(compiler, outer_symbol);
  compiler->symbols = current;
  return true;
}

static Instruction *compiler_instructions(Compiler *compiler) {
  return compiler->scopes[compiler->scope_index].instructions;
}

static uint32_t compiler_emit(Compiler *compiler, OpCode op, uint32_t operand,
                              uint32_t operand2, Token token) {
  Instruction **const instructions =
      &compiler->scopes[compiler->scope_index].instructions;
  PANIC(arrlenu(*instructions) < MAX_BYTECODE_INSTRUCTIONS,
        "instruction scope capacity exceeded (capacity=%u)",
        MAX_BYTECODE_INSTRUCTIONS);
  PANIC(compiler->instruction_count < MAX_BYTECODE_INSTRUCTIONS,
        "instruction capacity exceeded (capacity=%u)",
        MAX_BYTECODE_INSTRUCTIONS);

  const uint32_t position = (uint32_t)arrlenu(*instructions);
  arrput(*instructions, ((Instruction){
                            .op = op,
                            .operand = operand,
                            .operand2 = operand2,
                            .token = token,
                        }));
  compiler->instruction_count++;
  return position;
}

static uint32_t compiler_add_constant(Compiler *compiler, VmValue value) {
  PANIC(arrlenu(compiler->constants) < MAX_BYTECODE_CONSTANTS,
        "constant capacity exceeded (capacity=%u)", MAX_BYTECODE_CONSTANTS);
  const uint32_t index = (uint32_t)arrlenu(compiler->constants);
  arrput(compiler->constants, value);
  return index;
}

static bool compiler_last_is(Compiler *compiler, OpCode op) {
  Instruction *const instructions = compiler_instructions(compiler);
  return arrlenu(instructions) > 0 && arrlast(instructions).op == op;
}

static void compiler_remove_last(Compiler *compiler) {
  Instruction *const instructions = compiler_instructions(compiler);
  PANIC(arrlenu(instructions) > 0,
        "cannot remove an instruction from an empty scope");
  stbds_header(instructions)->length--;
}

static void compiler_replace_last(Compiler *compiler, OpCode op) {
  Instruction *const instructions = compiler_instructions(compiler);
  PANIC(arrlenu(instructions) > 0,
        "cannot replace an instruction in an empty scope");
  arrlast(instructions).op = op;
}

static void compiler_patch_jump(Compiler *compiler, uint32_t position,
                                uint32_t target) {
  Instruction *const instructions = compiler_instructions(compiler);
  PANIC(position < arrlenu(instructions),
        "jump position out of bounds: %u", position);
  instructions[position].operand = target;
}

static void compiler_enter_scope(Compiler *compiler) {
  PANIC(compiler->scope_index + 1 < MAX_COMPILER_SCOPES,
        "compiler scope capacity exceeded (capacity=%u)",
        MAX_COMPILER_SCOPES);
  compiler->scope_index++;
  compiler->scopes[compiler->scope_index] = (CompilerScope){0};
  compiler->symbols = compiler_new_symbol_table(compiler, compiler->symbols);
}

static Instruction *compiler_leave_scope(Compiler *compiler) {
  PANIC(compiler->scope_index > 0, "cannot leave the global compiler scope");
  Instruction *const instructions = compiler_instructions(compiler);
  compiler->symbols = compiler->symbols->outer;
  compiler->scope_index--;
  return instructions;
}

static void compiler_load_symbol(Compiler *compiler, Symbol symbol,
                                 Token token) {
  switch (symbol.scope) {
  case SYMBOL_GLOBAL:
    compiler_emit(compiler, OP_GET_GLOBAL, symbol.index, 0, token);
    break;
  case SYMBOL_LOCAL:
    compiler_emit(compiler, OP_GET_LOCAL, symbol.index, 0, token);
    break;
  case SYMBOL_FREE:
    compiler_emit(compiler, OP_GET_FREE, symbol.index, 0, token);
    break;
  case SYMBOL_FUNCTION:
    compiler_emit(compiler, OP_CURRENT_CLOSURE, 0, 0, token);
    break;
  }
}

static void compiler_compile_expression(Compiler *compiler, Expr *expr,
                                        const char *function_name);

static void compiler_compile_statements(Compiler *compiler, Stmt *stmts) {
  for (size_t i = 0; i < arrlenu(stmts); i++) {
    Stmt *const stmt = &stmts[i];
    switch (stmt->type) {
    case STMT_LET: {
      const bool is_function = stmt->data.let.value->type == EXPR_FUNCTION;
      Symbol symbol;
      if (is_function)
        symbol = compiler_define_symbol(compiler, stmt->data.let.name);

      compiler_compile_expression(
          compiler, stmt->data.let.value,
          is_function ? stmt->data.let.name : NULL);
      if (!is_function)
        symbol = compiler_define_symbol(compiler, stmt->data.let.name);

      const OpCode op =
          symbol.scope == SYMBOL_GLOBAL ? OP_SET_GLOBAL : OP_SET_LOCAL;
      compiler_emit(compiler, op, symbol.index, 0, stmt->token);
      break;
    }
    case STMT_RETURN:
      compiler_compile_expression(compiler, stmt->data.ret.value, NULL);
      compiler_emit(compiler, OP_RETURN_VALUE, 0, 0, stmt->token);
      break;
    case STMT_EXPR:
      compiler_compile_expression(compiler, stmt->data.expr.value, NULL);
      compiler_emit(compiler, OP_POP, 0, 0, stmt->token);
      break;
    }
  }
}

static void compiler_compile_if(Compiler *compiler, Expr *expr) {
  compiler_compile_expression(compiler, expr->data.if_expr.condition, NULL);
  const uint32_t jump_if_false =
      compiler_emit(compiler, OP_JUMP_NOT_TRUTHY, UINT32_MAX, 0, expr->token);

  compiler_compile_statements(compiler, expr->data.if_expr.consequence);
  if (compiler_last_is(compiler, OP_POP))
    compiler_remove_last(compiler);
  else if (!compiler_last_is(compiler, OP_RETURN_VALUE))
    compiler_emit(compiler, OP_NULL, 0, 0, expr->token);

  const uint32_t jump_end =
      compiler_emit(compiler, OP_JUMP, UINT32_MAX, 0, expr->token);
  compiler_patch_jump(compiler, jump_if_false,
                      (uint32_t)arrlenu(compiler_instructions(compiler)));

  if (expr->data.if_expr.alternative == NULL) {
    compiler_emit(compiler, OP_NULL, 0, 0, expr->token);
  } else {
    compiler_compile_statements(compiler, expr->data.if_expr.alternative);
    if (compiler_last_is(compiler, OP_POP))
      compiler_remove_last(compiler);
    else if (!compiler_last_is(compiler, OP_RETURN_VALUE))
      compiler_emit(compiler, OP_NULL, 0, 0, expr->token);
  }

  compiler_patch_jump(compiler, jump_end,
                      (uint32_t)arrlenu(compiler_instructions(compiler)));
}

static void compiler_compile_function(Compiler *compiler, Expr *expr,
                                      const char *function_name) {
  compiler_enter_scope(compiler);
  if (function_name != NULL)
    compiler_define_function_name(compiler, function_name);

  const size_t parameter_count = arrlenu(expr->data.function.parameters);
  PANIC(parameter_count <= MAX_FUNCTION_PARAMETERS,
        "%u:%u: parameter capacity exceeded (capacity=%u)", expr->token.line,
        expr->token.column, MAX_FUNCTION_PARAMETERS);
  for (size_t i = 0; i < parameter_count; i++)
    compiler_define_symbol(compiler, expr->data.function.parameters[i]);

  compiler_compile_statements(compiler, expr->data.function.body);
  if (compiler_last_is(compiler, OP_POP))
    compiler_replace_last(compiler, OP_RETURN_VALUE);
  if (!compiler_last_is(compiler, OP_RETURN_VALUE) &&
      !compiler_last_is(compiler, OP_RETURN))
    compiler_emit(compiler, OP_RETURN, 0, 0, expr->token);

  const uint16_t local_count = compiler->symbols->definition_count;
  Symbol *const free_symbols = compiler->symbols->free_symbols;
  const uint16_t free_count = (uint16_t)arrlenu(free_symbols);
  Instruction *const instructions = compiler_leave_scope(compiler);

  for (uint16_t i = 0; i < free_count; i++)
    compiler_load_symbol(compiler, free_symbols[i], expr->token);

  CompiledFunction *const function = arena_alloc(sizeof(*function));
  *function = (CompiledFunction){
      .instructions = instructions,
      .local_count = local_count,
      .parameter_count = (uint16_t)parameter_count,
  };
  const uint32_t constant = compiler_add_constant(
      compiler, (VmValue){
                    .type = VM_VALUE_COMPILED_FUNCTION,
                    .data.function = function,
                });
  compiler_emit(compiler, OP_CLOSURE, constant, free_count, expr->token);
}

static void compiler_compile_expression(Compiler *compiler, Expr *expr,
                                        const char *function_name) {
  PANIC(compiler->compile_depth < MAX_COMPILE_DEPTH,
        "%u:%u: compiler depth exceeds capacity (capacity=%u)",
        expr->token.line, expr->token.column, MAX_COMPILE_DEPTH);
  compiler->compile_depth++;

  switch (expr->type) {
  case EXPR_IDENT: {
    Symbol symbol;
    PANIC(compiler_resolve_symbol(compiler, compiler->symbols,
                                  expr->data.ident, &symbol),
          "%u:%u: identifier not found: %s", expr->token.line,
          expr->token.column, expr->data.ident);
    compiler_load_symbol(compiler, symbol, expr->token);
    break;
  }
  case EXPR_INT: {
    const uint32_t constant =
        compiler_add_constant(compiler, vm_integer_value(expr->data.integer));
    compiler_emit(compiler, OP_CONSTANT, constant, 0, expr->token);
    break;
  }
  case EXPR_BOOL:
    compiler_emit(compiler, expr->data.boolean ? OP_TRUE : OP_FALSE, 0, 0,
                  expr->token);
    break;
  case EXPR_PREFIX:
    compiler_compile_expression(compiler, expr->data.prefix.right, NULL);
    PANIC(expr->data.prefix.op == BANG || expr->data.prefix.op == MINUS,
          "%u:%u: unsupported prefix operator: %s", expr->token.line,
          expr->token.column, token_type_to_str(expr->data.prefix.op));
    compiler_emit(compiler,
                  expr->data.prefix.op == BANG ? OP_BANG : OP_MINUS, 0, 0,
                  expr->token);
    break;
  case EXPR_INFIX: {
    compiler_compile_expression(compiler, expr->data.infix.left, NULL);
    compiler_compile_expression(compiler, expr->data.infix.right, NULL);
    OpCode op;
    switch (expr->data.infix.op) {
    case PLUS:
      op = OP_ADD;
      break;
    case MINUS:
      op = OP_SUBTRACT;
      break;
    case ASTERISK:
      op = OP_MULTIPLY;
      break;
    case SLASH:
      op = OP_DIVIDE;
      break;
    case EQUAL:
      op = OP_EQUAL;
      break;
    case NOT_EQUAL:
      op = OP_NOT_EQUAL;
      break;
    case LT:
      op = OP_LESS_THAN;
      break;
    case GT:
      op = OP_GREATER_THAN;
      break;
    default:
      PANIC(false, "%u:%u: unsupported infix operator: %s", expr->token.line,
            expr->token.column, token_type_to_str(expr->data.infix.op));
    }
    compiler_emit(compiler, op, 0, 0, expr->token);
    break;
  }
  case EXPR_IF:
    compiler_compile_if(compiler, expr);
    break;
  case EXPR_FUNCTION:
    compiler_compile_function(compiler, expr, function_name);
    break;
  case EXPR_CALL: {
    compiler_compile_expression(compiler, expr->data.call.function, NULL);
    const size_t argument_count = arrlenu(expr->data.call.arguments);
    PANIC(argument_count <= MAX_FUNCTION_PARAMETERS,
          "%u:%u: argument capacity exceeded (capacity=%u)", expr->token.line,
          expr->token.column, MAX_FUNCTION_PARAMETERS);
    for (size_t i = 0; i < argument_count; i++)
      compiler_compile_expression(compiler, expr->data.call.arguments[i], NULL);
    compiler_emit(compiler, OP_CALL, (uint32_t)argument_count, 0, expr->token);
    break;
  }
  }

  compiler->compile_depth--;
}

void compiler_init(Compiler *compiler) {
  *compiler = (Compiler){0};
  compiler->global_symbols = compiler_new_symbol_table(compiler, NULL);
  compiler->symbols = compiler->global_symbols;
}

Bytecode compiler_compile(Compiler *compiler, Stmt *stmts) {
  PANIC(compiler->scope_index == 0,
        "compiler must start in the global scope");
  Instruction **const instructions = &compiler->scopes[0].instructions;
  if (*instructions != NULL)
    stbds_header(*instructions)->length = 0;
  compiler->symbols = compiler->global_symbols;
  compiler->compile_depth = 0;
  compiler_compile_statements(compiler, stmts);
  return (Bytecode){
      .instructions = *instructions,
      .constants = compiler->constants,
  };
}

#endif
