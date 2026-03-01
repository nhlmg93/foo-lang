# foo

An interpreter for the **foo** programming language, written in C.

This project is an implementation of the **foo** programming language interpreter following the book ["Writing An Interpreter In Go"](https://interpreterbook.com/) by Thorsten Ball. Instead of Go, we're using C.

## Building

Requires a C compiler (gcc or clang) and make.

```bash
make
```

This produces the `foo` executable.

## Usage

Currently the lexer can be run with:

```bash
./foo "<input_string>"
```

## Architecture

Following the interpreter book's structure:

1. **Lexer** - Converts source code into tokens
2. **Parser** - Builds an Abstract Syntax Tree (AST) from tokens
3. **Evaluator** - Walks the AST and executes the code
4. **REPL** - Interactive read-eval-print loop

## Project Structure

```
.
├── main.c          # Lexer implementation (in progress)
├── Makefile        # Build configuration
├── foo             # Compiled executable
└── README.md       # This file
```

## References

- [Writing An Interpreter In Go](https://interpreterbook.com/) by Thorsten Ball
- [Writing A Compiler In Go](https://compilerbook.com/) - Sequel to the interpreter book

## License

This project is for educational purposes. The original book's code is MIT licensed.
