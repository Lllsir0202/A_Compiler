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


%%

[0-9]+          {return this->MakeToken(TokenType::INTCONST, yytext, yylineno);}
"const"      {return this->MakeToken(TokenType::CONST, yytext, yylineno);}
"int"        {return this->MakeToken(TokenType::INTEGAR, yytext, yylineno);}
"void"       {return this->MakeToken(TokenType::VOID, yytext, yylineno);}
"if"         {return this->MakeToken(TokenType::IF, yytext, yylineno);}
"else"       {return this->MakeToken(TokenType::ELSE, yytext, yylineno);}
"while"      {return this->MakeToken(TokenType::WHILE, yytext, yylineno);}
"break"      {return this->MakeToken(TokenType::BREAK, yytext, yylineno);}
"continue"   {return this->MakeToken(TokenType::CONTINUE, yytext, yylineno);}
"return"     {return this->MakeToken(TokenType::RETURN, yytext, yylineno);}

<<EOF>>         {return this->MakeToken(TokenType::END, yytext, yylineno);}
[\n]            {this->yycolumn_ = 0; /* New line resets column */}
[ \t\r]+        { /* Ignore whitespace */ }

[.]+           {return this->MakeToken(TokenType::UNKNOWN, yytext, yylineno);}

%%