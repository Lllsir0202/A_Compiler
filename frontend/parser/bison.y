%require "3.2"
%language "C++"

// Use variant type for semantic values
// This allows us to store different types of values in the same variable
%define api.token.constructor
%define api.value.type variant

// Try to use detailed error messages
%define parse.error detailed

// Location tracking: location type is frontend::SourceRange
%locations
%define api.location.type {frontend::SourceRange}

%code requires {
    #include <string>
    #include <memory>
    #include "frontend/token/token.h"
    #include "frontend/ast/ast_base.h"

    namespace frontend{
        class BisonParserImpl;
        using ASTNodePtr = std::unique_ptr<ast::ASTNode>;
        using ExprNodePtr = std::unique_ptr<ast::ExprNode>;
        using VarDefNodePtr = std::unique_ptr<ast::VarDefNode>;
        using DeclNodePtr = std::unique_ptr<ast::DeclNode>;
        class ExprNode;
        class StmtNode;
        struct SourceRange;
    }
}

/* pass parser instance to Parser and yylex */
%param { frontend::BisonParserImpl& driver }

%code {
    #include "frontend/parser/bison_parser_impl.h"
    #include "frontend/parser/parser_utils.h"
    #include "frontend/token/token.h"
    #include "frontend/ast/ast_base.h"
    #include "frontend/ast/ast_spec.h"
    #include "frontend/ast/ast_decl.h"
    #include "frontend/ast/ast_expr.h"
    #include "frontend/ast/ast_stmt.h"


    // Using rewritten YYLLOC_DEFAULT macro to adapt to SourceRange
    #define YYLLOC_DEFAULT(Res, Rhs, N)                     \
      do                                                    \
        {                                                   \
          if (N)                                            \
            {                                               \
              (Res).begin_ = YYRHSLOC (Rhs, 1).begin_;      \
              (Res).end_   = YYRHSLOC (Rhs, N).end_;        \
            }                                               \
          else                                              \
            {                                               \
              (Res).begin_ = YYRHSLOC (Rhs, 0).end_;        \
              (Res).end_   = YYRHSLOC (Rhs, 0).end_;        \
            }                                               \
        }                                                   \
      while (false)
}

/* Override error and yylex function */
%code {
    void yy::parser::error(const frontend::SourceRange& loc, const std::string& msg) {
        driver.ReportError(loc, msg);
    }
    yy::parser::symbol_type yylex(frontend::BisonParserImpl& driver) {
      auto token = driver.GetLexer()->GetNextToken();
        switch (token.GetTokenType()) {
        #define NOVAL(name, bisontype) \
            case frontend::TokenType::name: { \
                return yy::parser::make_##bisontype(token.GetSourceRange()); \
            }
            #include "token2bison.def"
        #undef NOVAL
        // Then tokens with values
        case frontend::TokenType::INTCONST: {
            return yy::parser::make_INTCONST(frontend::StringToInt(token.GetLexeme()), token.GetSourceRange());
        }
        case frontend::TokenType::FLOATCONST: {
            return yy::parser::make_FLOATCONST(frontend::StringToFloat(token.GetLexeme()), token.GetSourceRange());
        }
        case frontend::TokenType::IDENTIFIER: {
            return yy::parser::make_IDENTIFIER(token.GetLexeme(), token.GetSourceRange());
        }
        default: {
            driver.ReportError(token.GetSourceRange(), "Unknown token");
            // Should not reach here
            return yy::parser::make_YYEOF(token.GetSourceRange());
        }
      }
    }
}

/* Define Tokens we need */
/* Start with tokens without any attribute */

/* KeyWords */
%token IF ELSE WHILE BREAK CONTINUE RETURN INT FLOAT VOID CONST END

/* Algorithm Operators */
%token ADD SUB MUL DIV MOD ASSIGN

/* Logical Operators */
%token AND OR NOT

/* Relational Operators */
%token LT GT LE GE EQ NEQ

/* Tokens with values */
%token <int> INTCONST "integer constant"
%token <float> FLOATCONST "float constant"
%token <std::string> IDENTIFIER "identifier"

/* Some Punctuation */
%token LCBRACE RCBRACE LSBRACE RSBRACE LPAREN RPAREN SEMICOLON COMMA

/* Then define priority */
%left ADD SUB
%left MUL DIV MOD

/* Followed by type definitions */
/* Define the type of non-terminal symbols */
%type <frontend::ASTNodePtr> program
%type <std::vector<frontend::ASTNodePtr>>* compunit
%type <frontend::ASTNodePtr> stmt
%type <frontend::ASTNodePtr> expr
%type <frontend::DeclNodePtr> global_decl
%type <frontend::DeclNodePtr> decl
%type <frontend::ASTNodePtr> funcdef
%type <frontend::VarDefNodePtr> def
%type <std::vector<frontend::VarDefNodePtr>>* def_list


/* Define grammer here */
%%
program:
    compunit
    {
        $$ = new frontend::ast::RootNode(std::move(*$1));
        delete $1;
        driver.GetContext().GetAstContext()->SetRootNode(std::move($$));
    }
    | program END
    {
        YYACCEPT;
    }
    ;

compunit:
    compunit global_decl
    {
      $$ = std::move($1);
      delete $1;
      $$.push_back(std::move($2));
    }
    |
    compunit funcdef
    {
      $$ = std::move($1);
      delete $1;
      $$.push_back(std::move($2));
    }
    |
    global_decl
    {
      $$ = new std::vector<frontend::ASTNodePtr>();
      $$->emplace_back(std::move($1));
    }
    |
    funcdef
    {
      $$ = new std::vector<frontend::ASTNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

global_decl:
    INT def_list SEMICOLON
    {
    }
    |
    FLOAT def_list SEMICOLON
    {
    }
    ;

def_list:
    def_list COMMA def
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($3));
    }
    |
    def
    {
      $$ = new std::vector<frontend::VarDefNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

def:
    IDENTIFIER
    {
      
    }


%%

/* Error handle and yylex */