# B-Minor Phase 1 implementation

Build with `g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor`.

Run `./bminor html input.bm`, `./bminor ansi input.bm`, `./bminor tokens input.bm`, or `./bminor ast input.bm`. Without a filename, input is read from standard input. Diagnostics go to standard error; a run with lexical or syntax errors exits 1, and an unreadable file exits 2. Output is still produced when diagnostics occur.

The implementation targets the B-Minor rules in `B-Minor-course-1182134.pdf`, not the C-like grammar in the phase handout appendix. B-Minor declarations use `name: type`, arrays use `array [size] type`, and functions use `name: function return-type (parameters) = body` (or `;` for a prototype). C-style declarations are not accepted.

## Lexical rules

- Identifier: `[A-Za-z_][A-Za-z0-9_]*`, at most 256 characters.
- Keywords: `array boolean char else for function if integer print return string void while`.
- Boolean: `true` or `false`. Integer: `[0-9]+`.
- String: `"([^"\\\n]|\\[^\n])*"`, at most 256 characters.
- Character: `'([^'\\\n]|\\[^\n])'`, with one ASCII character or escape.
- Comments: `//` through the end of the line, or `/*` through the first `*/`.
- Operators: `+ - * / % = < > ! ^ == != <= >= =< => =! ++ -- && ||`.
- Separators: `( ) { } [ ] ; , : ? .`. Whitespace is skipped.

The lexer reads the longest identifier or integer, then checks identifiers against keywords and Boolean literals. Two-character operators and comment openers take priority over single-character operators. The course operator table and examples differ, so both `=<`/`=>`/`=!` and `<=`/`>=`/`!=` are accepted. Literal lexemes retain their escapes. Invalid characters and unterminated literals or block comments produce diagnostics; scanning continues.

## Parser and AST

Recursive-descent declaration/statement parsing with precedence-climbing expressions builds JSON AST nodes with source location and initially empty `type`. Supported declarations, arrays/initializers/indexing, functions/prototypes, blocks, `if`/`else`, `for`, `return`, `print`, and expression statements are covered. Power (`^`) binds tighter than multiplication and is right-associative; calls/indexing/postfix increments bind tighter, unary `!`, `+`, `-` bind tighter than power. Other operators follow the precedence ordering on slide B-11; assignment is right-associative. Syntax recovery synchronizes at semicolon/brace and `if`/`return`.

```ebnf
program       = { declaration } ;
declaration   = identifier ":" ( type [ "=" ( expression | array-initializer ) ] ";"
              | "function" return-type "(" [ parameter { "," parameter } ] ")"
                ( "=" statement | ";" ) ) ;
parameter     = identifier ":" type ;
type          = "integer" | "boolean" | "char" | "string"
              | "array" "[" [ integer ] "]" ("integer" | "boolean" | "char" | "string") ;
return-type   = "integer" | "boolean" | "char" | "string" | "void" ;
array-initializer = "{" expression { "," expression } "}" ;
statement     = block | declaration | "if" "(" expression ")" statement [ "else" statement ]
              | "for" "(" [ expression ] ";" [ expression ] ";" [ expression ] ")" statement
              | "return" [ expression ] ";" | "print" [ expression { "," expression } ] ";"
              | expression ";" | ";" ;
block         = "{" { statement } "}" ;
expression    = assignment ;
assignment    = logical-or [ "=" assignment ] ;
logical-or    = logical-and { "||" logical-and } ;
logical-and   = equality { "&&" equality } ;
equality      = relation { ( "==" | "=!" | "!=" ) relation } ;
relation      = additive { ( "<" | ">" | "=<" | "=>" | "<=" | ">=" ) additive } ;
additive      = multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative = power { ( "*" | "/" | "%" ) power } ;
power         = unary [ "^" power ] ;
unary         = ( "!" | "+" | "-" ) unary | postfix ;
postfix       = primary { "(" [ expression { "," expression } ] ")" | "[" expression "]" | "++" | "--" } ;
primary       = identifier | integer | boolean | string | character | "(" expression ")" ;
```

`while`, floating-point values, and C-style declarations are intentionally excluded from the parser because the slides do not define them (the slides explicitly say B-Minor has no `while`). This is a Phase 1 syntax parser, not semantic/type checking; it does not enforce `main`, constant-only global initializers, type compatibility, or array-size/initializer count.

Build and run regression checks with:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor
python3 tests.py
```
