%{
#include <iostream>
#include "../include/ast.hpp"
#include "parser.hpp"

#define YY_DECL yy::parser::symbol_type yylex()

%}

%%

[ \n]+ {}
\+ { return yy::parser::make_PLUS(); }
\* { return yy::parser::make_TIMES(); }
- { return yy::parser::make_MINUS(); }
\/ { return yy::parser::make_DIVIDE(); }
[0-9]+ { return yy::parser::make_INT(atoi(yytext)); }
fun { return yy::parser::make_DEFN(); }
type { return yy::parser::make_DATA(); }
match { return yy::parser::make_CASE(); }
with { return yy::parser::make_OF(); }
let { return yy::parser::make_LET(); }
in { return yy::parser::make_IN(); }
\\ { return yy::parser::make_LAMBDA(); }
\{ { return yy::parser::make_OCURLY(); }
\} { return yy::parser::make_CCURLY(); }
\( { return yy::parser::make_OPAREN(); }
\) { return yy::parser::make_CPAREN(); }
,  { return yy::parser::make_COMMA(); }
-> { return yy::parser::make_ARROW(); }
= { return yy::parser::make_EQUAL(); }
[a-z][a-zA-Z]* { return yy::parser::make_LID(std::string(yytext)); }
[A-Z][a-zA-Z]* { return yy::parser::make_UID(std::string(yytext)); }

%%

int yywrap() {
    yypop_buffer_state();
    if (!YY_CURRENT_BUFFER) {
        return 1; // No more input
    }
    return 0; 
}
