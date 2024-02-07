package lexer

import (
	"unicode/utf8"
)

type Lexar struct {
	input        string
	position     int
	readPosition int
	ch           rune
}

func New(input string) *Lexar {
	return &Lexar{input: input}
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
