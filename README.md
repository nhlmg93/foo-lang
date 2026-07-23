# foo

A small tree-walking interpreter written in C, based on ["Writing An
Interpreter In Go"](https://interpreterbook.com/) by Thorsten Ball.

## Building

Requires a C compiler (gcc or clang) and make.

```bash
make
```

This produces the `foo` executable.

## Run a file

```bash
./foo test.foo
```

Run without arguments to start the REPL:

```bash
./foo
```

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

Invalid programs panic and abort. Runtime storage comes from one bounded 16 MiB
static arena; there is no heap allocation.

## Project Structure

```
.
├── arena.c         # STB-style static arena
├── ast.c           # STB-style AST declarations and implementation
├── lexer.c         # STB-style lexer
├── parser.c        # STB-style Pratt parser
├── main.c          # Evaluator, CLI, and REPL
├── test.foo        # Example program
└── Makefile
```

## References

- [Writing An Interpreter In Go](https://interpreterbook.com/) by Thorsten Ball
- [Writing A Compiler In Go](https://compilerbook.com/)

## License

This project is for educational purposes. The original book's code is MIT licensed.
