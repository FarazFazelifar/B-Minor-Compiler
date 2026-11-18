# Grammar

B-Minor slides, not the C-like handout appendix. No C-style declarations, floats, or `while` statements. Type checking is not implemented.

## Lexer

- Identifier: `[A-Za-z_][A-Za-z0-9_]*`, maximum 256 characters.
- Keywords: `array boolean char else for function if integer print return string void while`.
- Boolean: `true | false`. Integer: `[0-9]+`.
- String: `"([^"\\\n]|\\[^\n])*"`, maximum 256 characters.
- Character: `'([^'\\\n]|\\[^\n])'`, one ASCII character or escape.
- Comments: `//` to end of line, or `/* ... */`.
- Operators: `+ - * / % = < > ! ^ == != <= >= =< => =! ++ -- && ||`.
- Separators: `( ) { } [ ] ; , : ? .`.

Whitespace is skipped. Longest identifiers and integers are read first; keywords follow. Comments and two-character operators take priority. Both slide and conventional comparison spellings are accepted. Invalid characters and unterminated literals or comments produce diagnostics.

## Parser

Recursive descent with precedence climbing. `identifier :` starts a declaration; otherwise an identifier starts an expression. `function` after `:` selects a function declaration. `else` binds to the nearest `if`, resolving the dangling-else FIRST/FOLLOW conflict. The full grammar is not LL(1). Top-level recovery uses `;`, `}`, `if`, and `return`.

```ebnf
program       = { declaration } ;
declaration   = identifier ":" ( type [ "=" ( expression | array-initializer ) ] ";"
              | "function" return-type "(" [ parameter { "," parameter } ] ")"
                ( "=" statement | ";" ) ) ;
parameter     = identifier ":" type ;
type          = "integer" | "boolean" | "char" | "string"
              | "array" "[" [ integer ] "]" ("integer" | "boolean" | "char" | "string") ;
return-type   = "integer" | "boolean" | "char" | "string" | "void" ;
array-initializer = "{" [ expression { "," expression } ] "}" ;
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
