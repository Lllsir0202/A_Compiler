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
    #include "frontend/parser/bison_parser_impl.h"
    #include "frontend/parser/parser_utils.h"
    #include "frontend/token/token.h"
    #include "frontend/ast/ast_base.h"
    #include "frontend/ast/ast_spec.h"
    #include "frontend/ast/ast_decl.h"
    #include "frontend/ast/ast_expr.h"
    #include "frontend/ast/ast_stmt.h"

    namespace frontend{
        using ASTNodePtr = std::unique_ptr<ast::ASTNode>;
        using ExprNodePtr = std::unique_ptr<ast::ExprNode>;
        using StmtNodePtr = std::unique_ptr<ast::StmtNode>;
        using VarDefNodePtr = std::unique_ptr<ast::VarDefNode>;
        using DeclNodePtr = std::unique_ptr<ast::DeclNode>;
        using FuncFParamNodePtr = std::unique_ptr<ast::FuncFParamNode>;
        using InitializerNodePtr = std::unique_ptr<ast::Initializer>;
    }
}

/* pass parser instance to Parser and yylex */
%param { frontend::BisonParserImpl& driver }

%code {
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

/* Virtual Terminals, used to set Priority */
%token UMINUS UPLUS

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
/* Since we don't consider a=b=c, we ignore ASSIGN token directly */
/* Lowest ones are: && || not */
%left OR
%left AND
%left EQ NEQ
%left LT GT LE GE
%left ADD SUB
%left MUL DIV MOD
%right NOT UMINUS UPLUS

/* Followed by type definitions */
/* Define the type of non-terminal symbols */
%type <frontend::ASTNodePtr> program
%type <std::vector<frontend::ASTNodePtr>*> compunit
%type <frontend::StmtNodePtr> stmt
%type <frontend::ExprNodePtr> expr
%type <frontend::DeclNodePtr> global_decl
%type <frontend::DeclNodePtr> decl
%type <frontend::ASTNodePtr> funcdef
%type <std::vector<frontend::StmtNodePtr>*> stmt_list
%type <frontend::VarDefNodePtr> def
%type <std::vector<frontend::VarDefNodePtr>*> def_list
%type <std::vector<frontend::FuncFParamNodePtr>*> func_fparams
%type <frontend::FuncFParamNodePtr> func_fparam
%type <frontend::InitializerNodePtr> initials


/* Define grammer here */
%%
/* Topest unit of a file */
program:
    compunit
    {
        $$ = std::make_unique<frontend::ast::RootNode>(std::move(*$1));
        delete $1;
        driver.GetContext().GetAstContext()->SetRootNode(std::move($$));
    }
    | program END
    {
        YYACCEPT;
    }
    ;

/* Every program is a compunint */
compunit:
    compunit global_decl
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($2));
    }
    |
    compunit funcdef
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($2));
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

/* Global variable definition */
global_decl:
    /* Variable Definition */
    INT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::GlobalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move(*$2));
      delete $2;
    }
    |
    FLOAT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::GlobalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move(*$2));
      delete $2;
    }
    |
    /* Const Variable Definition */
    CONST INT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::GlobalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move(*$3), true);
      delete $3;
    }
    |
    CONST FLOAT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::GlobalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move(*$3), true);
      delete $3;
    } 
    ;

/* Since a declaration can have a list of defition */
/* We import def_list to record multiple definition */
def_list:
    /* def_list , def */
    def_list COMMA def
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* A single definition */
    def
    {
      $$ = new std::vector<frontend::VarDefNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

/* Every definition can have initializer or not */
/* TODO: Add array definition */
def:
    IDENTIFIER
    {
      $$ = std::make_unique<frontend::ast::VarDefNode>(@$, std::move($1), nullptr);
    }
    |
    IDENTIFIER ASSIGN initials
    {
      $$ = std::make_unique<frontend::ast::VarDefNode>(@$, std::move($1), std::move($3));
    }
    ;

/* The definition of a function */
funcdef:
    /* Function with no paramaters */
    VOID IDENTIFIER LPAREN RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetVoidType(), std::move($2), std::move(*$6));
      delete $6;
    }
    |
    INT IDENTIFIER LPAREN RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move($2), std::move(*$6));
      delete $6;
    }
    |
    FLOAT IDENTIFIER LPAREN RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move($2), std::move(*$6));
      delete $6;
    }
    |
    /* Functions with paramaters */
    VOID IDENTIFIER LPAREN func_fparams RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetVoidType(), std::move($2), std::move(*$7), std::move(*$4));
      delete $4;
      delete $7;
    }
    |
    INT IDENTIFIER LPAREN func_fparams RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move($2), std::move(*$7), std::move(*$4));
      delete $4;
      delete $7;
    }
    |
    FLOAT IDENTIFIER LPAREN func_fparams RPAREN LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move($2), std::move(*$7), std::move(*$4));
      delete $4;
      delete $7;
    }
    ;
  
/* The formal paramaters of a function */
func_fparams:
    /* func_fparams , func_fparam */
    func_fparams COMMA func_fparam
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* A single formal paramater */
    func_fparam
    {
      $$ = new std::vector<frontend::FuncFParamNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

/* A single formal paramater */
/* TODO: Add array type(and with initializers) */
func_fparam:
    INT IDENTIFIER
    {
      $$ = std::make_unique<frontend::ast::FuncFParamNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move($2));
    }
    |
    FLOAT IDENTIFIER
    {
      $$ = std::make_unique<frontend::ast::FuncFParamNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move($2));
    }
    ;

/* Statement list which is a vector of statements */
stmt_list:
    stmt_list stmt
    {
      $$ = std::move($1);
      delete $1;
      $$->emplace_back(std::move($2));
    }
    |
    stmt
    {
      $$ = new std::vector<frontend::StmtNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

/* Statements include different types of statements */
stmt:
    /* Equals to a empty statement, only a single ;*/
    SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ExprStmtNode>(@$);
    }
    ;

/* Expressions */
expr:
    /* Algorithm Operator */
    expr ADD expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::ADD, std::move($1), std::move($3));
    }
    |expr SUB expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::SUB, std::move($1), std::move($3));
    }
    |
    expr MUL expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::MUL, std::move($1), std::move($3));
    }
    |
    expr DIV expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::DIV, std::move($1), std::move($3));
    }
    |
    expr MOD expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::MOD, std::move($1), std::move($3));
    }
    |
    SUB expr %prec UMINUS
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::NEG, std::move($2));
    }
    |
    ADD expr %prec UPLUS
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::POS, std::move($2));
    }
    |
    /* Logical Operator */
    NOT expr
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::NOT, std::move($2));
    }
    |
    expr AND expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::AND, std::move($1), std::move($3));
    }
    |
    expr OR expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), OpCode::OR, std::move($1), std::move($3));
    }
    |
    /* Relational Operator */
    
    |
    {
      driver.ReportError(@$, "Expression expected");
    }
    ;
  
decl:
    /*  */
    {
      driver.ReportError(@$, "Declaration expected");
    }
    ;

initials:
    /*  */
    {
      driver.ReportError(@$, "Initializer expected");
    }
    ;

%%

/* Error handle and yylex */