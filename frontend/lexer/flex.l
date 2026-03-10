%{
#include "flex_lexer_impl.h"

using namespace frontend;

#define YY_USER_ACTION \
{ \
    /* Update column number */\
    this->yycolumn_ += yyleng; \
}
%}

%option c++
%option yyclass="frontend::FlexLexerImpl"
%option yylineno
%option noyywrap

%x COMMENT


%%

%{
    // ---- comment ---- //
%}
%{
    // Signle line comment //    
%}
"//".*                                              {this->yycolumn_ = 1;}

%{
    // Multi-line comment //    
%}
"/*"                                                {BEGIN(COMMENT);}
<COMMENT>"*/"                                       {BEGIN(INITIAL);}
<COMMENT>.                                          { /* Ignore comment content */ }
<COMMENT>\n                                         { this->yycolumn_ = 1; }


%{
    // ---- Keywords ---- //
%}
"const"                                             {return this->MakeToken(TokenType::CONST, yytext, yylineno);}
"if"                                                {return this->MakeToken(TokenType::IF, yytext, yylineno);}
"else"                                              {return this->MakeToken(TokenType::ELSE, yytext, yylineno);}
"while"                                             {return this->MakeToken(TokenType::WHILE, yytext, yylineno);}
"for"                                               {return this->MakeToken(TokenType::FOR, yytext, yylineno);} 
"break"                                             {return this->MakeToken(TokenType::BREAK, yytext, yylineno);}
"continue"                                          {return this->MakeToken(TokenType::CONTINUE, yytext, yylineno);}
"return"                                            {return this->MakeToken(TokenType::RETURN, yytext, yylineno);}

%{
    // ---- Value def ---- //
%}
"void"                                              {return this->MakeToken(TokenType::VOID, yytext, yylineno);}
"int"                                               {return this->MakeToken(TokenType::INTEGER, yytext, yylineno);}
"float"                                             {return this->MakeToken(TokenType::FLOAT, yytext, yylineno);}

%{
    // ---- Signals ---- //
%}
"{"                                                 {return this->MakeToken(TokenType::LCBRACE, yytext, yylineno);}
"}"                                                 {return this->MakeToken(TokenType::RCBRACE, yytext, yylineno);}
"["                                                 {return this->MakeToken(TokenType::LSBRACE, yytext, yylineno);}
"]"                                                 {return this->MakeToken(TokenType::RSBRACE, yytext, yylineno);}
"("                                                 {return this->MakeToken(TokenType::LPAREN, yytext, yylineno);}
")"                                                 {return this->MakeToken(TokenType::RPAREN, yytext, yylineno);}
";"                                                 {return this->MakeToken(TokenType::SEMICOLON, yytext, yylineno);}
","                                                 {return this->MakeToken(TokenType::COMMA, yytext, yylineno);}

%{
    // ---- Algorithm Operators ---- //
%}
"="                                                 {return this->MakeToken(TokenType::ASSIGN, yytext, yylineno);}
"+"                                                 {return this->MakeToken(TokenType::PLUS, yytext, yylineno);}
"-"                                                 {return this->MakeToken(TokenType::MINUS, yytext, yylineno);}
"*"                                                 {return this->MakeToken(TokenType::STAR, yytext, yylineno);}
"/"                                                 {return this->MakeToken(TokenType::DIV, yytext, yylineno);}
"%"                                                 {return this->MakeToken(TokenType::MOD, yytext, yylineno);}

%{
    // ---- Relational Operators ---- //
%}
"=="                                                {return this->MakeToken(TokenType::EQUAL, yytext, yylineno);}
"!="                                                {return this->MakeToken(TokenType::NEQUAL, yytext, yylineno);}
"<"                                                 {return this->MakeToken(TokenType::LESS, yytext, yylineno);}
">"                                                 {return this->MakeToken(TokenType::GREATER, yytext, yylineno);}
"<="                                                {return this->MakeToken(TokenType::LEQUAL, yytext, yylineno);}
">="                                                {return this->MakeToken(TokenType::GEQUAL, yytext, yylineno);}

%{
    // ---- Logical Operators ---- //
%}
"!"                                                 {return this->MakeToken(TokenType::NOT, yytext, yylineno);}
"&&"                                                {return this->MakeToken(TokenType::AND, yytext, yylineno);}
"||"                                                {return this->MakeToken(TokenType::OR, yytext, yylineno);}

%{
    // Error handling for unclosed comment    
%}
<COMMENT><<EOF>>                                   {return this->MakeToken(TokenType::UNKNOWN, "Unclosed comment", yylineno);}
<INITIAL>"*/"                                      {return this->MakeToken(TokenType::UNKNOWN, "Unmatched comment closer", yylineno);}


%{
    // ---- End of File ---- //
%}
<<EOF>>                                             {return this->MakeToken(TokenType::END, yytext, yylineno);}

%{
    // ---- Some special signals ---- //
%}
[\n]                                                {this->yycolumn_ = 1; /* New line resets column */}
[ \t\r]+                                            { /* Ignore whitespace since we have done calculation before any action*/ }

%{
    // ---- IDENTIFER ---- //
%}
[a-zA-Z_]+[0-9a-zA-Z_]*                             {return this->MakeToken(TokenType::IDENTIFIER, yytext, yylineno);}


%{
    // ---- Number ---- //
%}
%{
    // Integer    
%}
%{
    // demical
    // because we need to distinguish hex and demical
%}
[0-9]|[1-9][0-9]+                                   {return this->MakeToken(TokenType::INTCONST, yytext, yylineno);}

%{
    // hexadecimal    
%}
0[xX][0-9a-fA-F]+                                   {return this->MakeToken(TokenType::INTCONST, yytext, yylineno);}

%{
    // octal
%}
0[0-7]+                                             {return this->MakeToken(TokenType::INTCONST, yytext, yylineno);}

%{
    // float point number    
%}
%{
    // float with decimal point
    // there are two types:
    // 1. fractional_part exponent_part? -> 
    // (1) fractional_part: digit-sequence[opt] '.' digit-sequence or digit-sequence '.'
    // 2. digit-sequence exponent-part
    // (1) digit-sequence: [digit]+
    
    // exponent: [e/E] [+/-]? digit-sequence
%}

%{
    // Start with fraction_part exponent_part
%}
%{
    // digit-sequence '.' digit-sequence [exponent_part?]   
%}
[0-9]+\.([0-9]+)([eE][+-]?[0-9]+)?                          {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // '.' digit-sequence [exponent_part?]
%}
\.[0-9]+([eE][+-]?[0-9]+)?                                  {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // digit-sequence '.' [exponent_part?]
%}
[0-9]+\.([eE][+-]?[0-9]+)?                                  {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}


%{
    // Then digit-sequence exponent_part    
%}
[0-9]+[eE][+-]?[0-9]+                                       {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}



%{
    // float with hex point    
    // there are two types:
    // 1. hexadecimal_fractional_part hexadecimal_exponent_part?
    // (1) hexadecimal_fractional_part: hex-digit-sequence[opt] '.' hex-digit-sequence or hex-digit-sequence '.'
    // 2. hex-digit-sequence hexadecimal_exponent_part
    // (1) hex-digit-sequence: [0-9a-fA-F]+
%}

%{
    // hexadecimal_fractional_part hexadecimal_exponent_part?
%}
%{
    // hex-digit-sequence '.' hex-digit-sequence [hexadecimal_exponent_part?]    
%}
0[xX]([0-9a-fA-F]+)\.([0-9a-fA-F]+)([pP][+-]?[0-9]+)?       {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // '.' hex-digit-sequence [hexadecimal_exponent_part?]
%}
0[xX]\.([0-9a-fA-F]+)([pP][+-]?[0-9]+)?                     {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // hex-digit-sequence '.' [hexadecimal_exponent_part?]
%}
0[xX]([0-9a-fA-F]+)\.([pP][+-]?[0-9]+)?                     {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // hex-digit-sequence '.' [hexadecimal_exponent_part?]
%}
0[xX]([0-9a-fA-F]+)([pP][+-]?[0-9]+)?                       {return this->MakeToken(TokenType::FLOATCONST, yytext, yylineno);}

%{
    // ---- ERROR Handle ---- //
%}

.                                                           {return this->MakeToken(TokenType::UNKNOWN, yytext, yylineno);}

%%