%code requires {
#include <string>
#include <vector>
#include "ast.hpp"
#include "parsed_type.hpp"
#include "parser.hpp"

namespace ff { namespace drv { class ParseDriver; } }
using yyscan_t = void*;
}

%code {
#include "parse_driver.hpp"
}

%param { yyscan_t scanner }
%param { ff::drv::ParseDriver& drv }

%token PLUS
%token TIMES
%token MINUS
%token DIVIDE
%token <int> INT
%token DEFN
%token DATA
%token CASE
%token OF
%token OCURLY
%token CCURLY
%token OPAREN
%token CPAREN
%token COMMA
%token ARROW
%token EQUAL
%token LET
%token IN
%token LAMBDA
%token <std::string> LID
%token <std::string> UID

%language "c++"
%locations
%define api.value.type variant
%define api.token.constructor
%define parse.error verbose

%type <std::vector<std::string>> lowercaseParams uppercaseParams
%type <std::vector<std::unique_ptr<Branch>>> branches
%type <std::vector<std::unique_ptr<Constructor>>> constructors
%type <std::unique_ptr<Ast>> aAdd aMul case lambda let app appBase
%type <std::unique_ptr<DefinitionData>> data
%type <std::unique_ptr<DefinitionDefn>> defn
%type <std::unique_ptr<DefinitionGroup>> letDefinitions
%type <std::unique_ptr<Branch>> branch
%type <std::unique_ptr<Pattern>> pattern
%type <std::unique_ptr<Constructor>> constructor
%type <std::vector<std::unique_ptr<ff::sem::ParsedType>>> typeList
%type <std::unique_ptr<ff::sem::ParsedType>> type nonArrowType typeListElement

%start program

%%

program
    : definitions { }
    ;

definitions
    : definitions definition { }
    | definition { }
    ;

definition
    : defn { auto name = $1->name; drv.getGlobalDefs().defsDefn[name] = std::move($1); }
    | data { auto name = $1->name; drv.getGlobalDefs().defsData[name] = std::move($1); }
    ;

defn
    : DEFN LID lowercaseParams EQUAL OCURLY aAdd CCURLY
        { $$ = std::unique_ptr<DefinitionDefn>(
            new DefinitionDefn(std::move($2), std::move($3), std::move($6), @$)); }
    ;

lowercaseParams
    : %empty { $$ = std::vector<std::string>(); }
    | lowercaseParams LID { $$ = std::move($1); $$.push_back(std::move($2)); }
    ;

uppercaseParams
    : %empty { $$ = std::vector<std::string>(); }
    | uppercaseParams UID { $$ = std::move($1); $$.push_back(std::move($2)); }
    ;

aAdd
    : aAdd PLUS aMul { $$ = std::unique_ptr<Ast>(new AstBinop(PLUS, std::move($1), std::move($3), @$)); }
    | aAdd MINUS aMul { $$ = std::unique_ptr<Ast>(new AstBinop(MINUS, std::move($1), std::move($3), @$)); }
    | aMul { $$ = std::move($1); }
    ;

aMul
    : aMul TIMES app { $$ = std::unique_ptr<Ast>(new AstBinop(TIMES, std::move($1), std::move($3), @$)); }
    | aMul DIVIDE app { $$ = std::unique_ptr<Ast>(new AstBinop(DIVIDE, std::move($1), std::move($3), @$)); }
    | app { $$ = std::move($1); }
    ;

app
    : app appBase { $$ = std::unique_ptr<Ast>(new AstApp(std::move($1), std::move($2), @$)); }
    | appBase { $$ = std::move($1); }
    ;

appBase
    : INT { $$ = std::unique_ptr<Ast>(new AstInt($1, @$)); }
    | LID { $$ = std::unique_ptr<Ast>(new AstLid(std::move($1), @$)); }
    | UID { $$ = std::unique_ptr<Ast>(new AstUid(std::move($1), @$)); }
    | OPAREN aAdd CPAREN { $$ = std::move($2); }
    | case { $$ = std::move($1); }
    | lambda { $$ = std::move($1); }
    | let { $$ = std::move($1); }
    ;

case
    : CASE aAdd OF OCURLY branches CCURLY 
        { $$ = std::unique_ptr<Ast>(new AstCase(std::move($2), std::move($5), @$)); }
    ;

lambda
    : LAMBDA lowercaseParams ARROW OCURLY aAdd CCURLY
        { $$ = std::unique_ptr<Ast>(new AstLambda(std::move($2), std::move($5), @$)); }
    ;

let
    : LET OCURLY letDefinitions CCURLY IN OCURLY aAdd CCURLY
        { $$ = std::unique_ptr<Ast>(new AstLet(std::move($3), std::move($7), @$)); }
    ;

letDefinitions
    : letDefinitions defn
        { $$ = std::move($1); auto name = $2->name; $$->defsDefn[name] = std::move($2); }
    | defn
        { $$ = std::unique_ptr<DefinitionGroup>(new DefinitionGroup());
          auto name = $1->name; $$->defsDefn[name] = std::move($1); }
    ;

branches
    : branches branch { $$ = std::move($1); $$.push_back(std::move($2)); }
    | branch { $$ = std::vector<std::unique_ptr<Branch>>(); $$.push_back(std::move($1));}
    ;

branch
    : pattern ARROW OCURLY aAdd CCURLY
        { $$ = std::unique_ptr<Branch>(new Branch(std::move($1), std::move($4))); }
    ;

pattern
    : LID { $$ = std::unique_ptr<Pattern>(new PatternVar(std::move($1), @$)); }
    | UID lowercaseParams
        { $$ = std::unique_ptr<Pattern>(new PatternConstr(std::move($1), std::move($2), @$)); }
    ;

data
    : DATA UID lowercaseParams EQUAL OCURLY constructors CCURLY
        { $$ = std::unique_ptr<DefinitionData>(new DefinitionData(std::move($2), std::move($3), std::move($6), @$)); }
    ;

constructors
    : constructors COMMA constructor { $$ = std::move($1); $$.push_back(std::move($3)); }
    | constructor
        { $$ = std::vector<std::unique_ptr<Constructor>>(); $$.push_back(std::move($1)); }
    ;

constructor
    : UID typeList
        { $$ = std::unique_ptr<Constructor>(new Constructor(std::move($1), std::move($2))); }
    ;

type
    : nonArrowType ARROW type { $$ = std::unique_ptr<ff::sem::ParsedType>(new ff::sem::ParsedTypeArr(std::move($1), std::move($3))); }
    | nonArrowType { $$ = std::move($1); }
    ;

nonArrowType
    : UID typeList { $$ = std::unique_ptr<ff::sem::ParsedType>(new ff::sem::ParsedTypeApp(std::move($1), std::move($2))); }
    | LID { $$ = std::unique_ptr<ff::sem::ParsedType>(new ff::sem::ParsedTypeVar(std::move($1))); }
    | OPAREN type CPAREN { $$ = std::move($2); }
    ;

typeListElement
    : OPAREN type CPAREN { $$ = std::move($2); }
    | UID { $$ = std::unique_ptr<ff::sem::ParsedType>(new ff::sem::ParsedTypeApp(std::move($1), {})); }
    | LID { $$ = std::unique_ptr<ff::sem::ParsedType>(new ff::sem::ParsedTypeVar(std::move($1))); }
    ;

typeList
    : %empty { $$ = std::vector<std::unique_ptr<ff::sem::ParsedType>>(); }
    | typeList typeListElement { $$ = std::move($1); $$.push_back(std::move($2)); }
    ;
