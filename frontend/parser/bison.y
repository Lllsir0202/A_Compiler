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
        class ExprNode;
        class StmtNode;
        struct SourceRange;
    }
}

/* pass parser instance to Parser and yylex */
%param { frontend::BisonParserImpl& driver }

%code {
    #include "frontend/parser/bison_parser_impl.h"
    #include "frontend/token/token.h"
    #include "frontend/ast/ast_base.h"

    // 【新增】告诉 Bison 使用 begin_ 和 end_ 而不是默认的 begin 和 end
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
        return driver.Lex();
    }
}

/* Define Tokens we need */
/* Start with tokens without any attribute */

/* KeyWords */
%token IF ELSE WHILE BREAK CONTINUE RETURN INT FLOAT VOID CONST

/* Algorithm Operators */
%token ADD SUB MUL DIV MOD ASSIGN

/* Logical Operators */
%token AND OR NOT

/* Relational Operators */
%token LT GT LE GE EQ NEQ

/* Tokens with values */
%token <int> INT_LITERAL "integer literal"
%token <float> FLOAT_LITERAL "float literal"
%token <std::string> IDENTIFIER "identifier"

/* Then define priority */
%left ADD SUB
%left MUL DIV MOD

/* Followed by type definitions */
%type <frontend::ASTNodePtr> program


/* Define grammer here */
%%
program:
    {
        driver.GetContext().GetAstContext()->SetRootNode(std::move(nullptr));
    }
%%

/* Error handle and yylex */