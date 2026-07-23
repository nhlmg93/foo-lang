# foo

A small programming language implementation written in C. Foo can execute the
same source with either a tree-walking interpreter or a bytecode compiler and
stack-based virtual machine.

## Building

Requires a C compiler (gcc or clang) and make.

```bash
make
```

This produces the `foo` executable.

## Execution Modes

```bash
./foo --interpreter test.foo
./foo --compiler test.foo
```

Interpreter mode is the default, so this is equivalent to the first command:

```bash
./foo test.foo
```

Omit the source file to start a persistent REPL in either mode:

```bash
./foo --interpreter
./foo --compiler
```

Compiler mode compiles to Foo bytecode in memory and immediately executes it
with the VM. It does not emit a native executable or bytecode file.

## Language

```foo
let make_adder = fn(x) {
  fn(y) { x + y; };
};

let add_two = make_adder(2);
add_two(40);
```

Supported features:

- integers and booleans
- prefix and infix operators
- `let` and `return` statements
- `if`/`else` expressions
- functions, calls, recursion, and closures
- persistent REPL bindings

Invalid programs panic and abort. Foo-managed storage comes from one bounded
16 MiB static arena.

## Project Structure

```
.
├── lib/
│   ├── arena.c                  # Shared static arena
│   ├── ast.c                    # Shared AST
│   ├── lexer.c                  # Shared lexer
│   ├── parser.c                 # Shared Pratt parser
│   ├── interpreter/
│   │   └── evaluator.c         # Tree-walking evaluator
│   └── compiler/
│       ├── bytecode.c          # Opcodes and VM values
│       ├── compiler.c          # Symbol table and AST compiler
│       └── vm.c                # Stack-based virtual machine
├── main.c                      # CLI, file execution, and REPL dispatch
├── test.foo                    # Example program
└── Makefile
```

## References

- [Writing An Interpreter In Go](https://interpreterbook.com/) by Thorsten Ball
- [Writing A Compiler In Go](https://compilerbook.com/)

## License

This project is for educational purposes. The original book's code is MIT licensed.
