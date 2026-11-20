# B-Minor compiler

Phase 1: lexer, parser, AST, and syntax highlighting.

Build: `g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor`
Run: `./bminor <tokens|ast|html|ansi> [file]` (stdin without a file; HTML by default)
Test: `python3 tests.py`
Save highlighting: `./bminor html input.bminor > output.html`

Exit codes: 0 success, 1 source errors, 2 invalid arguments or unreadable file.
Grammar and lexer rules: [GRAMMAR.md](GRAMMAR.md)
