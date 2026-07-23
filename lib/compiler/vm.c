#ifndef FOO_LIB_COMPILER_VM_H
#define FOO_LIB_COMPILER_VM_H

#include <stddef.h>
#include <stdint.h>

#include "bytecode.c"

#define MAX_VM_STACK_VALUES ((uint32_t)4096)
#define MAX_VM_GLOBALS ((uint32_t)32 * 1024)
#define MAX_VM_FRAMES ((uint16_t)256)
#define MAX_VM_CLOSURES ((uint32_t)32 * 1024)

typedef struct {
  VmClosure *closure;
  uint32_t instruction_pointer;
  uint32_t base_pointer;
} VmFrame;

typedef struct {
  VmValue *stack;
  VmValue *globals;
  VmFrame *frames;
  VmValue *constants;
  uint32_t stack_count;
  uint32_t closure_count;
  uint16_t frame_count;
  CompiledFunction main_function;
  VmClosure main_closure;
  VmValue last_popped;
} Vm;

void vm_init(Vm *vm);
VmValue vm_run(Vm *vm, Bytecode bytecode);

#endif

#ifdef VM_IMPLEMENTATION

static VmFrame *vm_current_frame(Vm *vm) {
  PANIC(vm->frame_count > 0, "VM has no active frame");
  return &vm->frames[vm->frame_count - 1];
}

static void vm_push(Vm *vm, VmValue value) {
  PANIC(vm->stack_count < MAX_VM_STACK_VALUES,
        "VM stack capacity exceeded (capacity=%u)", MAX_VM_STACK_VALUES);
  vm->stack[vm->stack_count] = value;
  vm->stack_count++;
}

static VmValue vm_pop(Vm *vm) {
  PANIC(vm->stack_count > 0, "VM stack underflow");
  vm->stack_count--;
  return vm->stack[vm->stack_count];
}

static bool vm_is_truthy(VmValue value) {
  switch (value.type) {
  case VM_VALUE_NULL:
    return false;
  case VM_VALUE_BOOL:
    return value.data.boolean;
  default:
    return true;
  }
}

static VmValue vm_integer_binary(Instruction instruction, int64_t left,
                                 int64_t right) {
  int64_t result;
  switch (instruction.op) {
  case OP_ADD:
    PANIC(!__builtin_add_overflow(left, right, &result),
          "%u:%u: integer overflow", instruction.token.line,
          instruction.token.column);
    return vm_integer_value(result);
  case OP_SUBTRACT:
    PANIC(!__builtin_sub_overflow(left, right, &result),
          "%u:%u: integer overflow", instruction.token.line,
          instruction.token.column);
    return vm_integer_value(result);
  case OP_MULTIPLY:
    PANIC(!__builtin_mul_overflow(left, right, &result),
          "%u:%u: integer overflow", instruction.token.line,
          instruction.token.column);
    return vm_integer_value(result);
  case OP_DIVIDE:
    PANIC(right != 0, "%u:%u: division by zero", instruction.token.line,
          instruction.token.column);
    PANIC(left != INT64_MIN || right != -1, "%u:%u: integer overflow",
          instruction.token.line, instruction.token.column);
    return vm_integer_value(left / right);
  case OP_EQUAL:
    return vm_boolean_value(left == right);
  case OP_NOT_EQUAL:
    return vm_boolean_value(left != right);
  case OP_LESS_THAN:
    return vm_boolean_value(left < right);
  case OP_GREATER_THAN:
    return vm_boolean_value(left > right);
  default:
    PANIC(false, "%u:%u: unsupported integer opcode: %s",
          instruction.token.line, instruction.token.column,
          opcode_to_string(instruction.op));
  }
  __builtin_unreachable();
}

static VmValue vm_binary(Instruction instruction, VmValue left,
                         VmValue right) {
  if (left.type == VM_VALUE_INT && right.type == VM_VALUE_INT)
    return vm_integer_binary(instruction, left.data.integer,
                             right.data.integer);

  PANIC(left.type == right.type, "%u:%u: type mismatch: %s %s %s",
        instruction.token.line, instruction.token.column,
        vm_value_type_to_string(left.type), opcode_to_string(instruction.op),
        vm_value_type_to_string(right.type));

  if (left.type == VM_VALUE_BOOL &&
      (instruction.op == OP_EQUAL || instruction.op == OP_NOT_EQUAL)) {
    const bool equal = left.data.boolean == right.data.boolean;
    return vm_boolean_value(instruction.op == OP_EQUAL ? equal : !equal);
  }
  if (left.type == VM_VALUE_NULL &&
      (instruction.op == OP_EQUAL || instruction.op == OP_NOT_EQUAL))
    return vm_boolean_value(instruction.op == OP_EQUAL);

  PANIC(false, "%u:%u: unsupported operator: %s %s %s",
        instruction.token.line, instruction.token.column,
        vm_value_type_to_string(left.type), opcode_to_string(instruction.op),
        vm_value_type_to_string(right.type));
}

static void vm_execute_binary(Vm *vm, Instruction instruction) {
  const VmValue right = vm_pop(vm);
  const VmValue left = vm_pop(vm);
  vm_push(vm, vm_binary(instruction, left, right));
}

static void vm_execute_minus(Vm *vm, Instruction instruction) {
  const VmValue value = vm_pop(vm);
  PANIC(value.type == VM_VALUE_INT,
        "%u:%u: unsupported prefix operator -%s", instruction.token.line,
        instruction.token.column, vm_value_type_to_string(value.type));
  PANIC(value.data.integer != INT64_MIN, "%u:%u: integer overflow",
        instruction.token.line, instruction.token.column);
  vm_push(vm, vm_integer_value(-value.data.integer));
}

static void vm_call(Vm *vm, Instruction instruction) {
  const uint32_t argument_count = instruction.operand;
  PANIC(vm->stack_count >= argument_count + 1,
        "%u:%u: VM stack underflow while calling function",
        instruction.token.line, instruction.token.column);

  const uint32_t closure_index = vm->stack_count - argument_count - 1;
  const VmValue value = vm->stack[closure_index];
  PANIC(value.type == VM_VALUE_CLOSURE,
        "%u:%u: value is not callable: %s", instruction.token.line,
        instruction.token.column, vm_value_type_to_string(value.type));

  VmClosure *const closure = value.data.closure;
  PANIC(argument_count == closure->function->parameter_count,
        "%u:%u: expected %u arguments, got %u", instruction.token.line,
        instruction.token.column, closure->function->parameter_count,
        argument_count);
  PANIC(vm->frame_count < MAX_VM_FRAMES,
        "VM frame capacity exceeded (capacity=%u)", MAX_VM_FRAMES);

  const uint32_t base_pointer = vm->stack_count - argument_count;
  const uint32_t stack_count = base_pointer + closure->function->local_count;
  PANIC(stack_count <= MAX_VM_STACK_VALUES,
        "VM stack capacity exceeded (capacity=%u)", MAX_VM_STACK_VALUES);
  for (uint32_t i = argument_count; i < closure->function->local_count; i++)
    vm->stack[base_pointer + i] = vm_null_value();

  vm->frames[vm->frame_count] = (VmFrame){
      .closure = closure,
      .base_pointer = base_pointer,
  };
  vm->frame_count++;
  vm->stack_count = stack_count;
}

static void vm_return(Vm *vm, VmValue value) {
  if (vm->frame_count == 1) {
    vm->last_popped = value;
    vm->stack_count = 0;
    vm->frame_count = 0;
    return;
  }

  const VmFrame frame = *vm_current_frame(vm);
  PANIC(frame.base_pointer > 0, "invalid VM frame base pointer");
  vm->frame_count--;
  vm->stack_count = frame.base_pointer - 1;
  vm_push(vm, value);
}

static void vm_push_closure(Vm *vm, Instruction instruction) {
  PANIC(instruction.operand < arrlenu(vm->constants),
        "%u:%u: constant index out of bounds: %u", instruction.token.line,
        instruction.token.column, instruction.operand);
  const VmValue constant = vm->constants[instruction.operand];
  PANIC(constant.type == VM_VALUE_COMPILED_FUNCTION,
        "%u:%u: closure constant is not a compiled function",
        instruction.token.line, instruction.token.column);
  PANIC(instruction.operand2 <= MAX_FREE_VALUES,
        "%u:%u: free value capacity exceeded (capacity=%u)",
        instruction.token.line, instruction.token.column, MAX_FREE_VALUES);
  PANIC(vm->stack_count >= instruction.operand2,
        "%u:%u: VM stack underflow while creating closure",
        instruction.token.line, instruction.token.column);
  PANIC(vm->closure_count < MAX_VM_CLOSURES,
        "VM closure capacity exceeded (capacity=%u)", MAX_VM_CLOSURES);

  VmClosure *const closure = arena_alloc(sizeof(*closure));
  *closure = (VmClosure){
      .function = constant.data.function,
      .free_count = (uint16_t)instruction.operand2,
  };
  if (instruction.operand2 > 0) {
    closure->free_values =
        arena_alloc(sizeof(*closure->free_values) * instruction.operand2);
    const uint32_t first_free = vm->stack_count - instruction.operand2;
    for (uint32_t i = 0; i < instruction.operand2; i++)
      closure->free_values[i] = vm->stack[first_free + i];
    vm->stack_count = first_free;
  }

  vm->closure_count++;
  vm_push(vm, (VmValue){
                  .type = VM_VALUE_CLOSURE,
                  .data.closure = closure,
              });
}

void vm_init(Vm *vm) {
  *vm = (Vm){0};
  vm->stack =
      arena_alloc(sizeof(*vm->stack) * (size_t)MAX_VM_STACK_VALUES);
  vm->globals = arena_alloc(sizeof(*vm->globals) * (size_t)MAX_VM_GLOBALS);
  vm->frames = arena_alloc(sizeof(*vm->frames) * (size_t)MAX_VM_FRAMES);
  for (uint32_t i = 0; i < MAX_VM_GLOBALS; i++)
    vm->globals[i] = vm_null_value();
}

VmValue vm_run(Vm *vm, Bytecode bytecode) {
  vm->stack_count = 0;
  vm->frame_count = 1;
  vm->last_popped = vm_null_value();
  vm->constants = bytecode.constants;
  vm->main_function = (CompiledFunction){
      .instructions = bytecode.instructions,
  };
  vm->main_closure = (VmClosure){
      .function = &vm->main_function,
  };
  vm->frames[0] = (VmFrame){
      .closure = &vm->main_closure,
  };

  while (vm->frame_count > 0) {
    VmFrame *const frame = vm_current_frame(vm);
    Instruction *const instructions = frame->closure->function->instructions;
    if (frame->instruction_pointer >= arrlenu(instructions)) {
      PANIC(vm->frame_count == 1,
            "function ended without a return instruction");
      break;
    }

    const Instruction instruction =
        instructions[frame->instruction_pointer++];
    switch (instruction.op) {
    case OP_CONSTANT:
      PANIC(instruction.operand < arrlenu(vm->constants),
            "%u:%u: constant index out of bounds: %u", instruction.token.line,
            instruction.token.column, instruction.operand);
      vm_push(vm, vm->constants[instruction.operand]);
      break;
    case OP_NULL:
      vm_push(vm, vm_null_value());
      break;
    case OP_TRUE:
      vm_push(vm, vm_boolean_value(true));
      break;
    case OP_FALSE:
      vm_push(vm, vm_boolean_value(false));
      break;
    case OP_POP:
      vm->last_popped = vm_pop(vm);
      break;
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_EQUAL:
    case OP_NOT_EQUAL:
    case OP_LESS_THAN:
    case OP_GREATER_THAN:
      vm_execute_binary(vm, instruction);
      break;
    case OP_BANG:
      vm_push(vm, vm_boolean_value(!vm_is_truthy(vm_pop(vm))));
      break;
    case OP_MINUS:
      vm_execute_minus(vm, instruction);
      break;
    case OP_JUMP_NOT_TRUTHY: {
      const bool truthy = vm_is_truthy(vm_pop(vm));
      if (!truthy) {
        PANIC(instruction.operand <= arrlenu(instructions),
              "%u:%u: jump target out of bounds: %u", instruction.token.line,
              instruction.token.column, instruction.operand);
        frame->instruction_pointer = instruction.operand;
      }
      break;
    }
    case OP_JUMP:
      PANIC(instruction.operand <= arrlenu(instructions),
            "%u:%u: jump target out of bounds: %u", instruction.token.line,
            instruction.token.column, instruction.operand);
      frame->instruction_pointer = instruction.operand;
      break;
    case OP_SET_GLOBAL:
      PANIC(instruction.operand < MAX_VM_GLOBALS,
            "%u:%u: global index out of bounds: %u", instruction.token.line,
            instruction.token.column, instruction.operand);
      vm->globals[instruction.operand] = vm_pop(vm);
      break;
    case OP_GET_GLOBAL:
      PANIC(instruction.operand < MAX_VM_GLOBALS,
            "%u:%u: global index out of bounds: %u", instruction.token.line,
            instruction.token.column, instruction.operand);
      vm_push(vm, vm->globals[instruction.operand]);
      break;
    case OP_SET_LOCAL: {
      const uint32_t index = frame->base_pointer + instruction.operand;
      PANIC(index < MAX_VM_STACK_VALUES,
            "%u:%u: local index out of bounds: %u", instruction.token.line,
            instruction.token.column, instruction.operand);
      vm->stack[index] = vm_pop(vm);
      break;
    }
    case OP_GET_LOCAL: {
      const uint32_t index = frame->base_pointer + instruction.operand;
      PANIC(index < vm->stack_count, "%u:%u: local index out of bounds: %u",
            instruction.token.line, instruction.token.column,
            instruction.operand);
      vm_push(vm, vm->stack[index]);
      break;
    }
    case OP_CLOSURE:
      vm_push_closure(vm, instruction);
      break;
    case OP_GET_FREE:
      PANIC(instruction.operand < frame->closure->free_count,
            "%u:%u: free value index out of bounds: %u",
            instruction.token.line, instruction.token.column,
            instruction.operand);
      vm_push(vm, frame->closure->free_values[instruction.operand]);
      break;
    case OP_CURRENT_CLOSURE:
      vm_push(vm, (VmValue){
                      .type = VM_VALUE_CLOSURE,
                      .data.closure = frame->closure,
                  });
      break;
    case OP_CALL:
      vm_call(vm, instruction);
      break;
    case OP_RETURN_VALUE:
      vm_return(vm, vm_pop(vm));
      break;
    case OP_RETURN:
      vm_return(vm, vm_null_value());
      break;
    }
  }

  return vm->last_popped;
}

#endif
