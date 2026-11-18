# B-Minor compiler

Phase 1: lexer, parser, AST, and syntax highlighting.

Build: `g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor`
Run: `./bminor <tokens|ast|html|ansi> [file]` (stdin without a file)
Test: `python3 tests.py`

Grammar and lexer rules: [GRAMMAR.md](GRAMMAR.md)
