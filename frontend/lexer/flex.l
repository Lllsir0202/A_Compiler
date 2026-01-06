%{
#include "./lexer.h"
#include "../utils/token_factory.h"
%}

%option noyywrap

%%

[0-9]+          { yylval.ival = atoi(yytext); return T_NUMBER; }
[ \t\n]+       { /* ignore whitespace */ }

%%