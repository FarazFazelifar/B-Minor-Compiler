#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

enum class Kind { End, Invalid, Identifier, Integer, Boolean, String, Character, Comment, Keyword, Operator, Separator };
struct Token { Kind kind; string text, file; int line, col, start, end; };
struct Error { string msg, file; int line, col; };

class Lexer {
    string src, file;
    size_t pos = 0;
    int line = 1, col = 1;
    bool done() const { return pos >= src.size(); }
    char peek(size_t n = 0) const { return pos + n < src.size() ? src[pos + n] : '\0'; }
    char take() { if (done()) return '\0'; char c = src[pos++]; if (c == '\r' || (c == '\n' && (pos < 2 || src[pos-2] != '\r'))) { ++line; col = 1; } else if (c != '\n') ++col; return c; }
public:
    vector<Token> tokens; vector<Error> errors;
    Lexer(string s, string f) : src(move(s)), file(move(f)) {}
    vector<Token> run() {
        static const unordered_set<string> words = {"array","boolean","char","else","false","for","function","if","integer","print","return","string","true","void","while"};
        while (!done()) {
            if (isspace((unsigned char)peek())) { take(); continue; }
            int st = (int)pos, ln = line, cl = col; char c = peek(); Kind k = Kind::Invalid;
            if (isalpha((unsigned char)c) || c == '_') {
                take(); while (isalnum((unsigned char)peek()) || peek() == '_') take();
                string t = src.substr(st, pos-st);
                if (t.size() > 256) errors.push_back({"identifier exceeds 256 characters",file,ln,cl});
                k = t == "true" || t == "false" ? Kind::Boolean : words.count(t) ? Kind::Keyword : Kind::Identifier;
            } else if (isdigit((unsigned char)c)) {
                take(); while (isdigit((unsigned char)peek())) take(); k = Kind::Integer;
            } else if (c=='"' || c=='\'') {
                char q=take(); k=q=='"'?Kind::String:Kind::Character; bool closed=false, validChar=true; size_t literalLen=0;
                while (!done() && peek()!='\n' && peek()!='\r') {
                    char x=take();
                    if (x==q) { closed=true; break; }
                    if ((static_cast<unsigned char>(x)&0xC0)!=0x80) ++literalLen;
                    if (q=='\'' && static_cast<unsigned char>(x)>=128) validChar=false;
                    if (x=='\\' && !done() && peek()!='\n' && peek()!='\r') {
                        char escaped=take();
                        if (q=='\'' && static_cast<unsigned char>(escaped)>=128) validChar=false;
                    }
                }
                if (!closed) { k=Kind::Invalid; errors.push_back({q=='"'?"unterminated string literal":"unterminated character literal",file,ln,cl}); }
                else if (q=='\'' && (!validChar || literalLen!=1)) { k=Kind::Invalid; errors.push_back({"character literal must contain exactly one ASCII character",file,ln,cl}); }
                else if (q=='"' && literalLen>256) errors.push_back({"string literal exceeds 256 characters",file,ln,cl});
            } else if (c=='/' && peek(1)=='/') { take(); take(); while(!done()&&peek()!='\n'&&peek()!='\r') take(); k=Kind::Comment;
            } else if (c=='/' && peek(1)=='*') {
                take(); take(); bool closed=false;
                while(!done()) { if(peek()=='*'&&peek(1)=='/') {take();take();closed=true;break;} take(); }
                if(closed) k=Kind::Comment; else { k=Kind::Invalid; errors.push_back({"unterminated block comment",file,ln,cl}); }
            } else {
                string two; two+=c; two+=peek(1);
                static const unordered_set<string> pairs={"==","!=","<=",">=","=<","=>","=!","++","--","&&","||"};
                if(pairs.count(two)) {take();take();k=Kind::Operator;}
                else if(string("+-*/%=<>!^=").find(c)!=string::npos) {take();k=Kind::Operator;}
                else if(string("(){}[];,:?.").find(c)!=string::npos) {take();k=Kind::Separator;}
                else { take(); errors.push_back({string("invalid character '")+c+"'",file,ln,cl}); }
            }
            tokens.push_back({k,src.substr(st,pos-st),file,ln,cl,st,(int)pos});
        }
        tokens.push_back({Kind::End,"",file,line,col,(int)pos,(int)pos}); return tokens;
    }
};

struct Node {
    string kind, value, type_annotation; int line, col, start, end; vector<unique_ptr<Node>> kids;
    Node(string k,Token t):kind(move(k)),value(t.text),line(t.line),col(t.col),start(t.start),end(t.end){}
};
class Parser {
    vector<Token>& t; size_t i=0;
    Token& cur(){return t[min(i,t.size()-1)];} Token take(){Token x=cur();if(cur().kind!=Kind::End)++i;return x;}
    bool is(const string&s){return cur().text==s;} bool accept(const string&s){if(is(s)){take();return true;}return false;}
    unique_ptr<Node> node(string k,Token x){return make_unique<Node>(move(k),move(x));}
    void error(string m){errors.push_back({move(m),cur().file,cur().line,cur().col});}
    Token need(const string&s){if(is(s))return take();error("expected '"+s+"'");return cur();}
    void sync(){size_t old=i;while(cur().kind!=Kind::End&&!is(";")&&!is("}")&&!is("if")&&!is("return"))take();if(is(";"))take();if(i==old&&cur().kind!=Kind::End)take();}
    bool atomType(){return is("integer")||is("boolean")||is("char")||is("string");}
    unique_ptr<Node> type(bool allowArray=true){
        Token a=cur();
        if(atomType()){take();return node("Type",a);}
        if(accept("array")){if(!allowArray)error("function return type must be atomic or void");auto n=node("ArrayType",a);need("[");if(cur().kind==Kind::Integer)n->kids.push_back(node("Literal",take()));need("]");if(atomType()){Token el=take();n->kids.push_back(node("Type",el));}else error("expected array element type");return n;}
        error("expected type");return node("Error",a);
    }
    unique_ptr<Node> returnType(){if(is("void")){Token a=take();return node("Type",a);}return type(false);}
    bool declarationStart(){return cur().kind==Kind::Identifier&&i+1<t.size()&&(t[i+1].text==":" );}
    unique_ptr<Node> arrayInit(){Token a=need("{");auto n=node("ArrayInit",a);if(!is("}")){n->kids.push_back(expr());while(accept(",")){if(is("}")){error("expected array initializer element");break;}n->kids.push_back(expr());}}Token z=need("}");n->end=z.end;return n;}
    unique_ptr<Node> declaration(){
        Token name=take();auto id=node("Identifier",name);need(":");
        if(accept("function")){auto f=node("FuncDecl",name);f->kids.push_back(move(id));f->kids.push_back(returnType());need("(");if(!is(")")){do{Token p=cur();if(cur().kind!=Kind::Identifier){error("expected parameter name");break;}take();auto param=node("Parameter",p);need(":");param->kids.push_back(type());f->kids.push_back(move(param));}while(accept(","));}need(")");if(accept("="))f->kids.push_back(stmt());else need(";");return f;}
        auto v=node("VarDecl",name);v->kids.push_back(move(id));v->kids.push_back(type());if(accept("=")){if(v->kids.back()->kind=="ArrayType"&&is("{"))v->kids.push_back(arrayInit());else v->kids.push_back(expr());}need(";");return v;
    }
    int precedence(const string&op){if(op=="=")return 1;if(op=="||")return 2;if(op=="&&")return 3;if(op=="=="||op=="!="||op=="=!")return 4;if(op=="<"||op==">"||op=="=<"||op=="=>"||op=="<="||op==">=")return 5;if(op=="+"||op=="-")return 6;if(op=="*"||op=="/"||op=="%")return 7;if(op=="^")return 8;return 0;}
    unique_ptr<Node> expr(int minp=1){
        if(is(";")||is(")")||is("]")||is("}")||is(",")){error("expected expression");return node("Error",cur());}
        Token a=take();unique_ptr<Node> left;
        if(a.kind==Kind::Identifier)left=node("Identifier",a);
        else if(a.kind==Kind::Integer||a.kind==Kind::Boolean||a.kind==Kind::String||a.kind==Kind::Character)left=node("Literal",a);
        else if(a.text=="("){left=expr();need(")");left->line=a.line;left->col=a.col;left->start=a.start;}
        else if(a.text=="!"||a.text=="-"||a.text=="+"){left=node("UnaryExpr",a);left->kids.push_back(expr(9));}
        else{errors.push_back({"expected expression",a.file,a.line,a.col});left=node("Error",a);}
        while(true){
            if(is("(")&&12>=minp){Token op=take();auto n=node("CallExpr",op);n->line=left->line;n->col=left->col;n->start=left->start;n->kids.push_back(move(left));if(!is(")")){do{n->kids.push_back(expr());}while(accept(","));}Token z=need(")");n->end=z.end;left=move(n);continue;}
            if(is("[")&&12>=minp){Token op=take();auto n=node("IndexExpr",op);n->line=left->line;n->col=left->col;n->start=left->start;n->kids.push_back(move(left));n->kids.push_back(expr());Token z=need("]");n->end=z.end;left=move(n);continue;}
            if((is("++")||is("--"))&&11>=minp){Token op=take();auto n=node("PostfixExpr",op);n->line=left->line;n->col=left->col;n->start=left->start;n->kids.push_back(move(left));left=move(n);continue;}
            string op=cur().text;int p=precedence(op);if(!p||p<minp)break;Token oper=take();auto n=node(p==1?"AssignExpr":p==8?"PowerExpr":"BinaryExpr",oper);n->line=left->line;n->col=left->col;n->start=left->start;n->kids.push_back(move(left));n->kids.push_back(expr(p==1||p==8?p:p+1));left=move(n);
        }return left;
    }
    unique_ptr<Node> stmt(){Token a=cur();if(accept("{")){auto n=node("BlockStmt",a);while(cur().kind!=Kind::End&&!is("}")){size_t old=i, count=errors.size();n->kids.push_back(stmt());if(errors.size()>count&&i>old&&t[i-1].text!=";"&&t[i-1].text!="}"){while(cur().kind!=Kind::End&&!is(";")&&!is("}")&&!is("if")&&!is("return"))take();accept(";");}if(i==old)take();}Token z=need("}");n->end=z.end;return n;}
        if(accept("if")){auto n=node("IfStmt",a);need("(");n->kids.push_back(expr());need(")");n->kids.push_back(stmt());if(accept("else"))n->kids.push_back(stmt());return n;}
        if(accept("for")){auto n=node("ForStmt",a);need("(");if(!is(";"))n->kids.push_back(expr());need(";");if(!is(";"))n->kids.push_back(expr());need(";");if(!is(")"))n->kids.push_back(expr());need(")");n->kids.push_back(stmt());return n;}
        if(accept("return")||accept("print")){auto n=node(a.text=="return"?"ReturnStmt":"PrintStmt",a);if(!is(";")&&cur().kind!=Kind::End){do{n->kids.push_back(expr());}while(a.text=="print"&&accept(","));}need(";");return n;}
        if(accept(";"))return node("EmptyStmt",a);
        if(declarationStart()){if(i+2<t.size()&&t[i+2].text=="function")error("nested function declarations are not allowed");return declaration();}
        auto n=node("ExprStmt",a);n->kids.push_back(expr());need(";");return n;
    }
public:
    vector<Error> errors;Parser(vector<Token>&v):t(v){}
    unique_ptr<Node> parse(){Token a=cur();auto root=node("Program",a);while(cur().kind!=Kind::End){size_t old=i;if(declarationStart())root->kids.push_back(declaration());else{error("expected declaration");sync();}if(i==old&&cur().kind!=Kind::End)take();}return root;}
};
string esc(const string&s){string o;for(char c:s){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else o+=c;}return o;}
void jsonString(const string&s){static const char hex[]="0123456789abcdef";cout<<char(34);for(unsigned char c:s){switch(c){case 34:cout<<"\\\"";break;case 92:cout<<"\\\\";break;case 8:cout<<"\\b";break;case 12:cout<<"\\f";break;case 10:cout<<"\\n";break;case 13:cout<<"\\r";break;case 9:cout<<"\\t";break;default:if(c<32)cout<<"\\u00"<<hex[c>>4]<<hex[c&15];else cout<<static_cast<char>(c);}}cout<<char(34);}
void json(const Node*n){cout<<"{\"kind\":";jsonString(n->kind);cout<<",\"value\":";jsonString(n->value);cout<<",\"line\":"<<n->line<<",\"column\":"<<n->col<<",\"type\":";jsonString(n->type_annotation);cout<<",\"children\":[";for(size_t j=0;j<n->kids.size();++j){if(j)cout<<",";json(n->kids[j].get());}cout<<"]}";}
void markCalls(const Node*n,unordered_set<int>&calls,unordered_set<int>&decls){if(n->kind=="CallExpr"&&!n->kids.empty()&&n->kids[0]->kind=="Identifier")calls.insert(n->kids[0]->start);if(n->kind=="FuncDecl"&&!n->kids.empty())decls.insert(n->kids[0]->start);for(auto&k:n->kids)markCalls(k.get(),calls,decls);}
string color(const Token& t){if(t.kind==Kind::Keyword&&(t.text=="integer"||t.text=="boolean"||t.text=="char"||t.text=="string"||t.text=="void"||t.text=="array"))return "type";switch(t.kind){case Kind::Keyword:return "kw";case Kind::Identifier:return "id";case Kind::Integer:case Kind::Boolean:case Kind::String:case Kind::Character:return "lit";case Kind::Comment:return "comment";case Kind::Operator:return "op";case Kind::Invalid:return "err";default:return "";}}
string tokenKind(Kind k){switch(k){case Kind::Invalid:return "INVALID";case Kind::Identifier:return "IDENTIFIER";case Kind::Integer:return "INTEGER";case Kind::Boolean:return "BOOLEAN";case Kind::String:return "STRING";case Kind::Character:return "CHARACTER";case Kind::Comment:return "COMMENT";case Kind::Keyword:return "KEYWORD";case Kind::Operator:return "OPERATOR";case Kind::Separator:return "SEPARATOR";default:return "END";}}
int main(int argc,char**argv){string mode="html",file="<stdin>";if(argc>1)mode=argv[1];if(argc>2)file=argv[2];string src;if(file=="<stdin>"){ostringstream b;b<<cin.rdbuf();src=b.str();}else{ifstream in(file);if(!in){cerr<<"cannot open "<<file<<"\n";return 2;}ostringstream b;b<<in.rdbuf();src=b.str();}Lexer lx(src,file);auto ts=lx.run();vector<Token> parsed;for(auto&t:ts)if(t.kind!=Kind::Comment)parsed.push_back(t);Parser p(parsed);auto ast=mode=="tokens"?nullptr:p.parse();vector<Error> errs=lx.errors;errs.insert(errs.end(),p.errors.begin(),p.errors.end());unordered_set<int> bad;for(auto&e:errs)for(auto&t:ts)if(t.line==e.line&&t.col==e.col){bad.insert(t.start);break;}
    if(mode=="tokens"){for(auto&t:ts)if(t.kind!=Kind::End)cout<<t.file<<":"<<t.line<<":"<<t.col<<" "<<tokenKind(t.kind)<<" "<<t.text<<"\n";}
    else if(mode=="ast")json(ast.get());
    else if(mode=="ansi"){unordered_set<int>calls,decls;markCalls(ast.get(),calls,decls);int at=0;for(auto&t:ts){if(t.kind==Kind::End)break;cout<<src.substr(at,t.start-at);string c=bad.count(t.start)?"err":color(t);if(c!="err"&&t.kind==Kind::Identifier&&(calls.count(t.start)||decls.count(t.start)))c="fn";string code=c=="kw"?"\033[1;34m":c=="type"?"\033[35m":c=="id"?"\033[37m":c=="fn"?"\033[1;33m":c=="lit"?"\033[32m":c=="comment"?"\033[90m":c=="op"?"\033[36m":c=="err"?"\033[31m":"";if(!code.empty())cout<<code<<t.text<<"\033[0m";else cout<<t.text;at=t.end;}cout<<src.substr(at);}
    else {unordered_set<int>calls,decls;markCalls(ast.get(),calls,decls);cout<<"<!doctype html><html><head><meta charset=\"utf-8\"><style>body{background:#1e1e1e;color:#ddd}pre{white-space:pre-wrap;font:14px monospace}.kw{color:#569cd6}.type{color:#c586c0}.id{color:#ddd}.fn{color:#dcdcaa}.lit{color:#ce9178}.comment{color:#6a9955}.op{color:#4ec9b0}.err{color:#f44747}</style></head><body><pre>";int at=0;for(auto&t:ts){if(t.kind==Kind::End)break;cout<<esc(src.substr(at,t.start-at));string c=bad.count(t.start)?"err":color(t);if(c!="err"&&t.kind==Kind::Identifier&&(calls.count(t.start)||decls.count(t.start)))c="fn";string text=esc(t.text);if(!c.empty())cout<<"<span class=\""<<c<<"\">"<<text<<"</span>";else cout<<text;at=t.end;}cout<<esc(src.substr(at))<<"</pre></body></html>";}
    for(auto&e:errs) cerr<<e.file<<":"<<e.line<<":"<<e.col<<": error: "<<e.msg<<"\n";
    return errs.empty()?0:1;
}
