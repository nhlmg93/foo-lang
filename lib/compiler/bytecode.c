#ifndef FOO_LIB_COMPILER_BYTECODE_H
#define FOO_LIB_COMPILER_BYTECODE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../lexer.c"

#define MAX_BYTECODE_INSTRUCTIONS ((uint32_t)64 * 1024)
#define MAX_BYTECODE_CONSTANTS ((uint32_t)32 * 1024)
#define MAX_FUNCTION_LOCALS ((uint16_t)256)
#define MAX_FUNCTION_PARAMETERS ((uint16_t)256)
#define MAX_FREE_VALUES ((uint16_t)256)

typedef enum : uint8_t {
  OP_CONSTANT,
  OP_NULL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_EQUAL,
  OP_NOT_EQUAL,
  OP_LESS_THAN,
  OP_GREATER_THAN,
  OP_BANG,
  OP_MINUS,
  OP_JUMP_NOT_TRUTHY,
  OP_JUMP,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_LOCAL,
  OP_GET_LOCAL,
  OP_CLOSURE,
  OP_GET_FREE,
  OP_CURRENT_CLOSURE,
  OP_CALL,
  OP_RETURN_VALUE,
  OP_RETURN,
} OpCode;

typedef struct {
  OpCode op;
  uint32_t operand;
  uint32_t operand2;
  Token token;
} Instruction;

typedef struct CompiledFunction CompiledFunction;
typedef struct VmClosure VmClosure;

typedef enum : uint8_t {
  VM_VALUE_NULL,
  VM_VALUE_INT,
  VM_VALUE_BOOL,
  VM_VALUE_COMPILED_FUNCTION,
  VM_VALUE_CLOSURE,
} VmValueType;

typedef struct {
  VmValueType type;
  union {
    int64_t integer;
    bool boolean;
    CompiledFunction *function;
    VmClosure *closure;
  } data;
} VmValue;

struct CompiledFunction {
  Instruction *instructions;
  uint16_t local_count;
  uint16_t parameter_count;
};

struct VmClosure {
  CompiledFunction *function;
  VmValue *free_values;
  uint16_t free_count;
};

typedef struct {
  Instruction *instructions;
  VmValue *constants;
} Bytecode;

const char *opcode_to_string(OpCode op);
const char *vm_value_type_to_string(VmValueType type);
VmValue vm_null_value(void);
VmValue vm_integer_value(int64_t value);
VmValue vm_boolean_value(bool value);
void vm_value_print(VmValue value);

#endif

#ifdef BYTECODE_IMPLEMENTATION

static const char *const OPCODE_STRINGS[] = {
    [OP_CONSTANT] = "CONSTANT",
    [OP_NULL] = "NULL",
    [OP_TRUE] = "TRUE",
    [OP_FALSE] = "FALSE",
    [OP_POP] = "POP",
    [OP_ADD] = "ADD",
    [OP_SUBTRACT] = "SUBTRACT",
    [OP_MULTIPLY] = "MULTIPLY",
    [OP_DIVIDE] = "DIVIDE",
    [OP_EQUAL] = "EQUAL",
    [OP_NOT_EQUAL] = "NOT_EQUAL",
    [OP_LESS_THAN] = "LESS_THAN",
    [OP_GREATER_THAN] = "GREATER_THAN",
    [OP_BANG] = "BANG",
    [OP_MINUS] = "MINUS",
    [OP_JUMP_NOT_TRUTHY] = "JUMP_NOT_TRUTHY",
    [OP_JUMP] = "JUMP",
    [OP_SET_GLOBAL] = "SET_GLOBAL",
    [OP_GET_GLOBAL] = "GET_GLOBAL",
    [OP_SET_LOCAL] = "SET_LOCAL",
    [OP_GET_LOCAL] = "GET_LOCAL",
    [OP_CLOSURE] = "CLOSURE",
    [OP_GET_FREE] = "GET_FREE",
    [OP_CURRENT_CLOSURE] = "CURRENT_CLOSURE",
    [OP_CALL] = "CALL",
    [OP_RETURN_VALUE] = "RETURN_VALUE",
    [OP_RETURN] = "RETURN",
};

const char *opcode_to_string(OpCode op) {
  PANIC((size_t)op < sizeof(OPCODE_STRINGS) / sizeof(OPCODE_STRINGS[0]),
        "unknown opcode: %u", op);
  return OPCODE_STRINGS[op];
}

const char *vm_value_type_to_string(VmValueType type) {
  switch (type) {
  case VM_VALUE_NULL:
    return "NULL";
  case VM_VALUE_INT:
    return "INTEGER";
  case VM_VALUE_BOOL:
    return "BOOLEAN";
  case VM_VALUE_COMPILED_FUNCTION:
  case VM_VALUE_CLOSURE:
    return "FUNCTION";
  }
  PANIC(false, "unknown VM value type: %u", type);
}

VmValue vm_null_value(void) { return (VmValue){.type = VM_VALUE_NULL}; }

VmValue vm_integer_value(int64_t value) {
  return (VmValue){
      .type = VM_VALUE_INT,
      .data.integer = value,
  };
}

VmValue vm_boolean_value(bool value) {
  return (VmValue){
      .type = VM_VALUE_BOOL,
      .data.boolean = value,
  };
}

void vm_value_print(VmValue value) {
  switch (value.type) {
  case VM_VALUE_NULL:
    puts("null");
    break;
  case VM_VALUE_INT:
    printf("%" PRId64 "\n", value.data.integer);
    break;
  case VM_VALUE_BOOL:
    puts(value.data.boolean ? "true" : "false");
    break;
  case VM_VALUE_COMPILED_FUNCTION:
  case VM_VALUE_CLOSURE:
    puts("<function>");
    break;
  }
}

#endif
