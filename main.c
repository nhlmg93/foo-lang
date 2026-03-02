#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define TOKEN_IMPLEMENTATION
#include "token.c"

#define ARENA_IMPLEMENTATION
#include "arena.c"

#define LEXER_IMPLEMENTATION
#include "lexer.c"

// int main(int argc, char *argv[]) {
//  if (argc <= 1) {
//    fprintf(stderr, "Usage: %s <input_string>\n", argv[0]);
//    return EXIT_FAILURE;
//  }
int main() {

  arena_init_global();

  const char *input = R"(
let five = 5;
let ten = 10;
let add = fn(x, y) {
    x + y;
};
let result = add(five, ten);
)";

  Lexer l = lexer_init(input);

  read_char(&l);
  // print_lexer(l);

  Token t;
  while (true) {
    t = next_token(&l, &g_arena);
    // print_lexer(l);

    if (strcmp(t.type, EOF_TOKEN) == 0) {
      print_token(t);
      break;
    } else {
      print_token(t);
    }
  }

  return EXIT_SUCCESS;
}
