package lexer

import (
	"foo_lang/token"
	"unicode/utf8"
)

type Lexar struct {
	input        string
	position     int
	readPosition int
	ch           rune
}

func New(input string) *Lexar {
	l := &Lexar{input: input}

	l.readRune()

	return l
}

func (l *Lexar) readRune() {
	var width int
	if l.readPosition >= len(l.input) {
		l.ch = 0
	} else {
		inputSlice := l.input[l.readPosition:]
		l.ch, width = utf8.DecodeRuneInString(inputSlice)
	}
	l.position = l.readPosition
	l.readPosition += width
}

func (l *Lexar) NextToken() token.Token {
	var tok token.Token
	switch l.ch {
	case '=':
		tok = newToken(token.ASSIGN, l.ch)
	case ';':
		tok = newToken(token.SEMICOLON, l.ch)
	case '(':
		tok = newToken(token.LPAREN, l.ch)
	case ')':
		tok = newToken(token.RPAREN, l.ch)
	case ',':
		tok = newToken(token.COMMA, l.ch)
	case '+':
		tok = newToken(token.PLUS, l.ch)
	case '{':
		tok = newToken(token.LBRACE, l.ch)
	case '}':
		tok = newToken(token.RBRACE, l.ch)
	case 0:
		tok.Literal = ""
		tok.Type = token.EOF
	}
	l.readRune()
	return tok
}
func newToken(tokenType token.TokenType, ch rune) token.Token {
	return token.Token{Type: tokenType, Literal: string(ch)}
}
