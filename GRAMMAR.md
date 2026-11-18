# Grammar

B-Minor slides, not the C-like handout appendix. No C-style declarations, floats, or `while` statements. Type checking is not implemented.

## Lexer

- Identifier: `[A-Za-z_][A-Za-z0-9_]*`, maximum 256 characters.
- Keywords: `array boolean char else for function if integer print return string void while`.
- Boolean: `true | false`. Integer: `[0-9]+`.
- String: `"([^"\\\r\n]|\\[^\r\n])*"`, maximum 256 characters.
- Character: `'([^'\\\r\n]|\\[^\r\n])'`, one ASCII character or escape.
- Comments: `//` to end of line, or `/* ... */`.
- Operators: `+ - * / % = < > ! ^ == != <= >= ++ -- && ||`.
- Separators: `( ) { } [ ] ; , : ? .`.

Whitespace is skipped. Longest identifiers and integers are read first; keywords follow. Comments and two-character operators take priority. Invalid characters and unterminated literals or comments produce diagnostics.

## Parser

Recursive descent with precedence climbing; the grammar has no left recursion. Expression alternatives start with an identifier, literal, `(`, `!`, `+`, or `-`. Statements use two-token lookahead: `identifier :` starts a variable declaration, otherwise it starts an expression. Other statement starters are distinct. At top level, `function` after `identifier :` selects a function declaration; a type selects a variable declaration.

The dangling-else FIRST/FOLLOW conflict is resolved by binding `else` to the nearest `if`; the full grammar is not LL(1). Expression precedence and associativity are handled by precedence climbing. Top-level recovery uses `;`, `}`, `if`, and `return`.

```ebnf
program       = { declaration } ;
declaration   = variable-decl | function-decl ;
variable-decl = identifier ":" ( atomic-type [ "=" expression ]
              | array-type [ "=" ( expression | array-initializer ) ] ) ";" ;
function-decl = identifier ":" "function" return-type "(" [ parameter { "," parameter } ] ")"
                ( "=" statement | ";" ) ;
parameter     = identifier ":" type ;
type          = atomic-type | array-type ;
atomic-type   = "integer" | "boolean" | "char" | "string" ;
array-type    = "array" "[" [ integer ] "]" atomic-type ;
return-type   = atomic-type | "void" ;
array-initializer = "{" [ expression { "," expression } ] "}" ;
statement     = block | variable-decl | "if" "(" expression ")" statement [ "else" statement ]
              | "for" "(" [ expression ] ";" [ expression ] ";" [ expression ] ")" statement
              | "return" [ expression ] ";" | "print" expression { "," expression } ";"
              | expression ";" | ";" ;
block         = "{" { statement } "}" ;
expression    = assignment ;
assignment    = logical-or [ "=" assignment ] ;
logical-or    = logical-and { "||" logical-and } ;
logical-and   = equality { "&&" equality } ;
equality      = relation { ( "==" | "!=" ) relation } ;
relation      = additive { ( "<" | ">" | "<=" | ">=" ) additive } ;
additive      = multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative = power { ( "*" | "/" | "%" ) power } ;
power         = unary [ "^" power ] ;
unary         = ( "!" | "+" | "-" ) unary | postfix ;
postfix       = primary { "(" [ expression { "," expression } ] ")" | "[" expression "]" | "++" | "--" } ;
primary       = identifier | integer | boolean | string | character | "(" expression ")" ;
```
