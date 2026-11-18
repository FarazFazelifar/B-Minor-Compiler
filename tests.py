import json
import subprocess


def run(mode, source):
    return subprocess.run(["./bminor", mode], input=source, text=True, capture_output=True)


valid = '''total: integer = 2;
add: function integer (a: integer, b: integer) = { return a + b; }
main: function integer () = { x: integer = add(total, 3); if (x =< 5) print "small", x; return x; }
'''
r = run("ast", valid)
assert r.returncode == 0, r.stderr
ast = json.loads(r.stdout)
assert ast["kind"] == "Program" and ast["type"] == ""
assert '"kind":"FuncDecl"' in r.stdout and '"kind":"CallExpr"' in r.stdout
assert '"type":""' in r.stdout

# JSON AST strings escape control characters without changing their values.
control_source = 's: string = "a\tb";'
r = run("ast", control_source)
assert r.returncode == 0, r.stderr
control_ast = json.loads(r.stdout)
assert control_ast["children"][0]["children"][2]["value"] == '"a\tb"'

# An embedded NUL is an invalid token, not end-of-input; lexing continues.
nul_source = "x: integer = 1;\x00return x;"
r = run("tokens", nul_source)
assert r.returncode == 1 and "invalid character" in r.stderr
assert "\x00" in r.stdout and " KEYWORD return" in r.stdout and " IDENTIFIER x" in r.stdout

for source in ("x: integer =", "f: function integer () = { return ", "f: function integer () = { return 1+"):
    r = run("ast", source)
    assert r.returncode == 1 and "error:" in r.stderr, (source, r.returncode, r.stderr)
    json.loads(r.stdout)

r = run("tokens", "integer x = 3; @\nreturn x;")
assert r.returncode == 1 and "invalid character" in r.stderr
assert "<stdin>:2:1 KEYWORD return" in r.stdout

r = run("tokens", "/* never closed")
assert r.returncode == 1 and "unterminated block comment" in r.stderr
assert " INVALID /* never closed" in r.stdout

r = run("tokens", "yes: boolean = true; a: integer; b: integer; a<=b;")
assert "<stdin>:1:6 KEYWORD boolean" in r.stdout
assert " BOOLEAN true" in r.stdout and " OPERATOR <=" in r.stdout
assert " KEYWORD boolean" in r.stdout

r = run("tokens", '"unfinished')
assert r.returncode == 1 and "unterminated string literal" in r.stderr
assert ' INVALID "unfinished' in r.stdout

r = run("ast", "x: integer = ; y: integer = 4;")
assert r.returncode == 1 and "expected expression" in r.stderr
assert '"value":"y"' in r.stdout

r = run("html", valid)
assert '<span class="fn">add</span>' in r.stdout
assert '<span class="op">=</span>' in r.stdout
assert '<pre>' in r.stdout and '&quot;small&quot;' in r.stdout
r = run("ansi", "f: function integer () = { return a+b; }")
assert "\033[1;33mf\033[0m" in r.stdout
assert "\033[36m+\033[0m" in r.stdout
assert "\033[37ma\033[0m" in r.stdout

# B-Minor declarations use identifier-colon syntax, not C-style declarations.
bminor = '''
a: integer = 3;
b: array [3] integer = {1,2,3};
p: array [] integer;
f: function integer (x: integer, y: integer) = { return x ^ 2 + y; }
puts: function void (s: string);
main: function integer () = {
  c: char = 'q'; s: string = "hello\\n";
  b: boolean = true; b = a < b[0] || false;
  print s, c, b; return f(b[1], a);
}
'''
r = run("ast", bminor)
assert r.returncode == 0, r.stderr
assert '"kind":"ArrayType"' in r.stdout and '"kind":"ArrayInit"' in r.stdout
assert '"kind":"FuncDecl"' in r.stdout and '"kind":"IndexExpr"' in r.stdout
assert '"kind":"PowerExpr"' in r.stdout
slide_sample = '''printarray: function void (a: array [] integer, size: integer) = {
  i: integer;
  for (i=0; i<size; i++) { print a[i], "\\n"; }
}
puts: function void (s: string);
main: function integer () = { puts("hello world"); }
'''
r = run("ast", slide_sample)
assert r.returncode == 0, r.stderr
power_tree = json.loads(run("ast", "x: integer = 2 ^ 3 * 4;").stdout)["children"][0]["children"][2]
assert power_tree["kind"] == "BinaryExpr" and power_tree["value"] == "*"
assert power_tree["children"][0]["kind"] == "PowerExpr"

for source in ("a: integer = ; b: integer = 4;", "f: function integer () = { return 1;", "a: array [] integer = {1,};", "integer x = 1;", "f: function array [] integer ();"):
    r = run("ast", source)
    assert r.returncode == 1 and "error:" in r.stderr, (source, r.stderr)
    json.loads(r.stdout)
assert '"value":"b"' in run("ast", "a: integer = ; b: integer = 4;").stdout

r = run("tokens", "arr: array [2] integer; f: function integer () = { return a ^ b; } x: integer = a =< b; y: integer = a => b; z: boolean = a =! b;")
assert r.returncode == 0, r.stderr
for token in ("KEYWORD array", "KEYWORD function", "OPERATOR =<", "OPERATOR =>", "OPERATOR =!", "OPERATOR ^"):
    assert token in r.stdout, (token, r.stdout)

# AST locations point to the first token in each compound expression.
loc_ast = json.loads(run("ast", "main: function integer () = { f(x)[i]++; return x + y; }").stdout)
expr = loc_ast["children"][0]["children"][2]["children"][0]["children"][0]
assert (expr["kind"], expr["line"], expr["column"]) == ("PostfixExpr", 1, 31)
index = expr["children"][0]
assert (index["kind"], index["column"]) == ("IndexExpr", 31)
call = index["children"][0]
assert (call["kind"], call["column"]) == ("CallExpr", 31)
binary = loc_ast["children"][0]["children"][2]["children"][1]["children"][0]
assert (binary["kind"], binary["column"]) == ("BinaryExpr", 49)

for source in (
    "main: function integer () = { f: function integer () = ; }",
    "while: integer;",
    "main: function integer () = { while (true) ; }",
    "c: char = '';", "c: char = 'ab';", "c: char = 'é';",
    "x" + "a" * 256 + ": integer;",
    's: string = "' + "a" * 257 + '";',
    's: string = "' + "é" * 257 + '";',
):
    r = run("ast", source)
    assert r.returncode == 1 and "error:" in r.stderr, (source[:40], r.returncode, r.stderr)

nested = run("ast", "main: function integer () = { f: function integer () = ; }")
assert "nested function declarations are not allowed" in nested.stderr
assert run("ast", "a" * 256 + ": integer;").returncode == 0
assert run("ast", 's: string = "' + "a" * 256 + '";').returncode == 0
assert run("ast", 's: string = "' + "é" * 256 + '";').returncode == 0
assert " KEYWORD while" in run("tokens", "while (true) ;").stdout
assert run("tokens", r"c: char = '\n';").returncode == 0

r = run("tokens", "c: char = ''; c: char = 'ab'; c: char = 'é';")
assert r.returncode == 1 and r.stderr.count("character literal") >= 3
r = run("html", "x: integer = ;")
assert r.returncode == 1 and '<span class="err">;</span>' in r.stdout

source = '/* start */ x /* name */ : integer = 1; // end\n'
r = run("ast", source)
assert r.returncode == 0, r.stderr
assert json.loads(r.stdout)["children"][0]["value"] == "x"
r = run("html", source)
assert r.returncode == 0 and '<span class="comment">/* name */</span>' in r.stdout

r = run("tokens", "& | ~ x: integer;")
assert r.returncode == 1 and r.stderr.count("invalid character") == 3
assert " IDENTIFIER x" in r.stdout

for source in ('s: string = "first\\\nsecond";', "c: char = '\\\n';"):
    r = run("ast", source)
    assert r.returncode == 1 and "unterminated" in r.stderr, r.stderr

r = run("ast", "main: function integer () = { print 1 + ; return 2; }")
assert r.returncode == 1 and r.stderr.count("error:") == 1, r.stderr
assert '"kind":"ReturnStmt"' in r.stdout

r = run("tokens", "return x;")
assert r.returncode == 0 and not r.stderr, r.stderr
assert " KEYWORD return" in r.stdout and " IDENTIFIER x" in r.stdout

r = run("tokens", "a: integer;\rreturn;\r\nx: integer;\ny: integer;")
assert "<stdin>:2:1 KEYWORD return" in r.stdout
assert "<stdin>:3:1 IDENTIFIER x" in r.stdout and "<stdin>:4:1 IDENTIFIER y" in r.stdout
r = run("tokens", '// note\rreturn;')
assert "<stdin>:2:1 KEYWORD return" in r.stdout
r = run("tokens", '"bad\rreturn;')
assert r.returncode == 1 and "unterminated string literal" in r.stderr
assert "<stdin>:2:1 KEYWORD return" in r.stdout

r = run("tokens", "1 true 'a' \"a\" ; @")
for token in ("INTEGER 1", "BOOLEAN true", "CHARACTER 'a'", 'STRING "a"', "SEPARATOR ;", "INVALID @"):
    assert token in r.stdout, r.stdout

r = run("html", "x: integer = 1;")
assert '<span class="type">integer</span>' in r.stdout
assert "\033[35minteger\033[0m" in run("ansi", "x: integer = 1;").stdout

source = "main: function integer () = { f: function integer (); }"
assert '<span class="err">f</span>' in run("html", source).stdout
assert "\033[31mf\033[0m" in run("ansi", source).stdout

r = run("ast", "main: function integer () = { x: integer = 1 junk junk; return 2; }")
assert r.returncode == 1 and r.stderr.count("error:") == 1, r.stderr
block = json.loads(r.stdout)["children"][0]["children"][2]
assert [node["kind"] for node in block["children"]] == ["VarDecl", "ReturnStmt"]
for source in ("x: integer = 1 return 2;", "x: integer = ; return 2;"):
    r = run("ast", "main: function integer () = { " + source + " }")
    block = json.loads(r.stdout)["children"][0]["children"][2]
    assert r.returncode == 1 and block["children"][-1]["kind"] == "ReturnStmt", r.stderr

for char in ("é", "م", "€", "😀"):
    r = run("tokens", char + " x: integer;")
    assert r.returncode == 1 and r.stderr.count("invalid character") == 1
    assert "<stdin>:1:1 INVALID " + char in r.stdout and "<stdin>:1:3 IDENTIFIER x" in r.stdout

for source, position in (('s: string = "é"; x: integer;', 18), ('/* 😀 */ x: integer;', 9), ('// م\nx: integer;', 1)):
    r = run("tokens", source)
    assert r.returncode == 0 and f":{position} IDENTIFIER x" in r.stdout, r.stdout

r = subprocess.run(["./bminor", "tokens"], input=b"\xf0\x80 x: integer;", capture_output=True)
assert r.returncode == 1 and r.stdout.count(b"INVALID") == 2 and b":1:4 IDENTIFIER x" in r.stdout

source = 'main: function void () = { (foo)(); }'
r = run("ast", source)
assert r.returncode == 0, r.stderr
call = json.loads(r.stdout)["children"][0]["children"][2]["children"][0]["children"][0]
assert call["kind"] == "CallExpr" and call["children"][0]["children"][0]["column"] == source.index("foo") + 1
assert '<span class="fn">foo</span>' in run("html", source).stdout

for source, kinds in (("for (; x; ) ;", ["EmptyExpr", "Identifier", "EmptyExpr", "EmptyStmt"]),
                      ("for (x; ; ) ;", ["Identifier", "EmptyExpr", "EmptyExpr", "EmptyStmt"])):
    tree = json.loads(run("ast", "f: function void () = { " + source + " }").stdout)
    loop = tree["children"][0]["children"][2]["children"][0]
    assert [child["kind"] for child in loop["children"]] == kinds

r = run("ast", "main: function integer () = { print; return; }")
assert r.returncode == 1 and "expected expression" in r.stderr
assert '"kind":"ReturnStmt"' in r.stdout

r = run("ast", "main: function integer () = { x: integer = 1 y: integer = 2; return 3; }")
block = json.loads(r.stdout)["children"][0]["children"][2]
assert r.returncode == 1 and [n["kind"] for n in block["children"]] == ["VarDecl", "VarDecl", "ReturnStmt"]

print("regression groups passed")
