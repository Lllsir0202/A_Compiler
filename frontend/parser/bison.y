%require "3.2"
%language "C++"

// Use variant type for semantic values
// This allows us to store different types of values in the same variable
%define api.token.constructor
%define api.value.type variant
%define parser.assert

// Try to use detailed error messages
%define parse.error detailed

// Location tracking: location type is frontend::SourceRange
%locations
%define api.location.type {frontend::SourceRange}

%code requires {
    #include <string>
    #include <memory>

    namespace frontend{
        class BisonParserImpl;
        class ASTNode;
        class ExprNode;
        class StmtNode;
        class SourceRange;
    }
}

/* pass parser instance to Parser and yylex */
%param { frontend::BisonParserImpl& parser }

%code {
    #include "frontend/parser/bison_parser_impl.h"
    #include "frontend/token/token.h"

    yy::parser::symbol_type yylex(frontend::BisonParserImpl& parser);
}

/* Define Tokens we need */



/* Define grammer here */
%%

%%

/* Error handle and yylex */