# B-Minor Phase 1 implementation

Build with `g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor`.

Run `./bminor html input.bm`, `./bminor ansi input.bm`, `./bminor tokens input.bm`, or `./bminor ast input.bm`. Without a filename, input is read from standard input. Diagnostics go to standard error; a run with lexical or syntax errors exits 1, and an unreadable file exits 2. Output is still produced when diagnostics occur.

The implementation targets the B-Minor rules in `B-Minor-course-1182134.pdf`, not the C-like grammar in the phase handout appendix. B-Minor declarations use `name: type`, arrays use `array [size] type`, and functions use `name: function return-type (parameters) = body` (or `;` for a prototype). C-style declarations are not accepted.

## Lexical rules

Identifiers are `[A-Za-z_][A-Za-z0-9_]*`; the course slides cap them at 256 characters (this implementation does not yet enforce that limit). Keywords are `array boolean char else for function if integer print return string void`; `true` and `false` are Boolean literals. Integers are decimal digit sequences. Strings and characters are quoted with backslash escaping; the slides define `\\n`, `\\0`, and backslash-plus-any-other-character as escapes, but this implementation preserves raw lexemes and does not validate escape sequences, literal lengths, or one-character character payloads. Whitespace is skipped; both C/C++ comments are recognized. Longest match handles multi-character operators, including course spellings `=<`, `=>`, `=!`, plus conventional spellings `<`, `>`, `!=`, `<=`, `>=` shown in examples/table variants. The slides themselves are inconsistent: the operator table lists `=<`, `=>`, `=!`, but examples use `<`, `==`; implementation recognizes both forms. Floats are not a B-Minor token and are not recognized as one.

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

`while`, floating-point values, and C-style declarations are intentionally excluded because the slides do not define them (the slides explicitly say B-Minor has no `while`). This is a Phase 1 syntax parser, not semantic/type checking; it does not enforce `main`, constant-only global initializers, type compatibility, array-size/initializer count, or the slide's stated identifier/string limits.

Build and run regression checks with:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o bminor
python3 tests.py
```
