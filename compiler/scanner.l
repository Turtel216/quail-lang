%{
#include <iostream>
#include "ast.hpp"
#include "parse_driver.hpp"
#include "parser.hpp"

#define LOC drv.getLocation()

/* Runs before every rule: record the text so errors can quote it, and move
 * the location to cover exactly the token we just matched. */
#define YY_USER_ACTION \
    drv.getFile().write(yytext, yyleng); \
    LOC.step(); LOC.columns(yyleng);

%}

%option reentrant
%option noyywrap

%%

\n { LOC.lines(1); LOC.step(); drv.getFile().markLine(); }
[ \t\r]+ {}
\+ { return yy::parser::make_PLUS(LOC); }
\* { return yy::parser::make_TIMES(LOC); }
- { return yy::parser::make_MINUS(LOC); }
\/ { return yy::parser::make_DIVIDE(LOC); }
[0-9]+ { return yy::parser::make_INT(atoi(yytext), LOC); }
fun { return yy::parser::make_DEFN(LOC); }
type { return yy::parser::make_DATA(LOC); }
match { return yy::parser::make_CASE(LOC); }
with { return yy::parser::make_OF(LOC); }
let { return yy::parser::make_LET(LOC); }
in { return yy::parser::make_IN(LOC); }
if { return yy::parser::make_IF(LOC); }
else { return yy::parser::make_ELSE(LOC); }
\\ { return yy::parser::make_LAMBDA(LOC); }
\{ { return yy::parser::make_OCURLY(LOC); }
\} { return yy::parser::make_CCURLY(LOC); }
\( { return yy::parser::make_OPAREN(LOC); }
\) { return yy::parser::make_CPAREN(LOC); }
\[ { return yy::parser::make_OBRACKET(LOC); }
\] { return yy::parser::make_CBRACKET(LOC); }
,  { return yy::parser::make_COMMA(LOC); }
-> { return yy::parser::make_ARROW(LOC); }
\|> { return yy::parser::make_PIPE(LOC); }
= { return yy::parser::make_EQUAL(LOC); }
[a-z][a-zA-Z]* { return yy::parser::make_LID(std::string(yytext), LOC); }
[A-Z][a-zA-Z]* { return yy::parser::make_UID(std::string(yytext), LOC); }
<<EOF>> { return yy::parser::make_YYEOF(LOC); }

%%
