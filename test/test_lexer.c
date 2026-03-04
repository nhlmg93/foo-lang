#include "../lib/minunit.h"

#define ARENA_IMPLEMENTATION
#include "../arena.c"

#define TOKEN_IMPLEMENTATION
#include "../token.c"

#define LEXER_IMPLEMENTATION
#include "../lexer.c"

// Test helper to check token type
static void assert_token_type(Lexer *l, Arena *arena, const char *expected_type) {
    Token t = next_token(l, arena);
    mu_assert_string_eq(expected_type, t.type);
}

MU_TEST(test_let_keyword) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("let");
    read_char(&l);
    
    Token t = next_token(&l, &arena);
    mu_assert_string_eq(LET, t.type);
    mu_assert_string_eq("let", t.literal);
}

MU_TEST(test_fn_keyword) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("fn");
    read_char(&l);
    
    Token t = next_token(&l, &arena);
    mu_assert_string_eq(FUNCTION, t.type);
    mu_assert_string_eq("fn", t.literal);
}

MU_TEST(test_identifier) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("five");
    read_char(&l);
    
    Token t = next_token(&l, &arena);
    mu_assert_string_eq(IDENT, t.type);
    mu_assert_string_eq("five", t.literal);
}

MU_TEST(test_operators) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("=+");
    read_char(&l);
    
    assert_token_type(&l, &arena, ASSIGN);
    assert_token_type(&l, &arena, PLUS);
}

MU_TEST(test_delimiters) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("(){},;");
    read_char(&l);
    
    assert_token_type(&l, &arena, LPAREN);
    assert_token_type(&l, &arena, RPAREN);
    assert_token_type(&l, &arena, LBRACE);
    assert_token_type(&l, &arena, RBRACE);
    assert_token_type(&l, &arena, COMMA);
    assert_token_type(&l, &arena, SEMICOLON);
}

MU_TEST(test_simple_assignment) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("let five = 5;");
    read_char(&l);
    
    assert_token_type(&l, &arena, LET);
    assert_token_type(&l, &arena, IDENT);
    assert_token_type(&l, &arena, ASSIGN);
    assert_token_type(&l, &arena, ILLEGAL);  // numbers not implemented yet
    assert_token_type(&l, &arena, SEMICOLON);
}

MU_TEST(test_whitespace_skip) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("   let   ");
    read_char(&l);
    
    Token t = next_token(&l, &arena);
    mu_assert_string_eq(LET, t.type);
    mu_assert_string_eq("let", t.literal);
}

MU_TEST(test_eof) {
    Arena arena;
    uint8_t buffer[1024];
    arena_init(&arena, buffer, sizeof(buffer));
    
    Lexer l = lexer_init("");
    read_char(&l);
    
    Token t = next_token(&l, &arena);
    mu_assert_string_eq(EOF_TOKEN, t.type);
}

MU_TEST_SUITE(test_lexer_suite) {
    MU_RUN_TEST(test_let_keyword);
    MU_RUN_TEST(test_fn_keyword);
    MU_RUN_TEST(test_identifier);
    MU_RUN_TEST(test_operators);
    MU_RUN_TEST(test_delimiters);
    MU_RUN_TEST(test_simple_assignment);
    MU_RUN_TEST(test_whitespace_skip);
    MU_RUN_TEST(test_eof);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    MU_RUN_SUITE(test_lexer_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}