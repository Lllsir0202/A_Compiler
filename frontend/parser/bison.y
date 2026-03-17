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
/* Used in distinguish if and if-else */
%token THEN

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
%left ASSIGN
/* Lowest ones are: && || not */
%left OR
%left AND
%left EQ NEQ
%left LT GT LE GE
%left ADD SUB
%left MUL DIV MOD
%right NOT UMINUS UPLUS

/* Let ELSE have a higher priority */
/* So that when we meet `else` in ifstmt first move 'else' in ifstmt,
 then move 'if' in ifstmt, and we will not have shift-reduce conflict
 */
%left THEN
%left ELSE

/* Followed by type definitions */
/* Define the type of non-terminal symbols */
%type <frontend::ASTNodePtr> program
%type <std::vector<frontend::ASTNodePtr>*> compunit
%type <frontend::StmtNodePtr> stmt
%type <frontend::DeclNodePtr> global_decl
%type <frontend::DeclNodePtr> decl
%type <frontend::ASTNodePtr> funcdef
%type <std::vector<frontend::StmtNodePtr>*> stmt_list
%type <frontend::VarDefNodePtr> def
%type <std::vector<frontend::VarDefNodePtr>*> def_list
%type <std::vector<frontend::FuncFParamNodePtr>*> func_fparams
%type <frontend::FuncFParamNodePtr> func_fparam
%type <std::vector<frontend::ExprNodePtr>*> func_rparams

/* Initializer */
/* initials: represents the topest initializer */
%type <frontend::InitializerNodePtr> initials
/* initials_list: represents the list of initializers */
%type <std::vector<frontend::InitializerNodePtr>*> initials_list

/* Following are a list of expressions */
%type <frontend::ExprNodePtr> expr
%type <frontend::ExprNodePtr> lval
%type <frontend::ExprNodePtr> funccall

/* Following are a list of statements */
%type <frontend::StmtNodePtr> ifstmt
%type <frontend::StmtNodePtr> whilestmt
%type <frontend::StmtNodePtr> breakstmt
%type <frontend::StmtNodePtr> continuestmt
%type <frontend::StmtNodePtr> returnstmt
%type <frontend::StmtNodePtr> blockstmt
%type <frontend::StmtNodePtr> exprstmt
%type <frontend::StmtNodePtr> assignstmt

/* Following are the definitions about array */
%type <std::vector<frontend::ExprNodePtr>*> array_indices

/* Following are used to define the parameter */
%type <std::vector<frontend::ExprNodePtr>*> func_fparam_indices

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
      $$ = $1;
      $$->emplace_back(std::move($2));
    }
    |
    compunit funcdef
    {
      $$ = $1;
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
      $$ = $1;
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
    |
    /* Array definition */
    IDENTIFIER array_indices 
    {
      $$ = std::make_unique<frontend::ast::VarDefNode>(@$, std::move($1), nullptr, std::move(*$2));
      delete $2;
    }
    |
    IDENTIFIER array_indices ASSIGN initials
    {
      $$ = std::make_unique<frontend::ast::VarDefNode>(@$, std::move($1), std::move($4), std::move(*$2));
      delete $2;
    }
    ;

array_indices:
    /* array_indices [ expr ] */
    array_indices LSBRACE expr RSBRACE
    {
      $$ = $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* A single index */
    /* Note: expr should be a constexpr */
    LSBRACE expr RSBRACE
    {
      $$ = new std::vector<frontend::ExprNodePtr>();
      $$->emplace_back(std::move($2));
    }
    ;

/* The definition of a function */
funcdef:
    /* Function with no parameters */
    VOID IDENTIFIER LPAREN RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetVoidType(), std::move($2), std::move($5));
    }
    |
    INT IDENTIFIER LPAREN RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move($2), std::move($5));
    }
    |
    FLOAT IDENTIFIER LPAREN RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move($2), std::move($5));
    }
    |
    /* Functions with parameters */
    VOID IDENTIFIER LPAREN func_fparams RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetVoidType(), std::move($2), std::move($6), std::move(*$4));
      delete $4;
    }
    |
    INT IDENTIFIER LPAREN func_fparams RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move($2), std::move($6), std::move(*$4));
      delete $4;
    }
    |
    FLOAT IDENTIFIER LPAREN func_fparams RPAREN blockstmt
    {
      $$ = std::make_unique<frontend::ast::FuncDefNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move($2), std::move($6), std::move(*$4));
      delete $4;
    }
    ;
  
/* The formal parameters of a function */
func_fparams:
    /* func_fparams , func_fparam */
    func_fparams COMMA func_fparam
    {
      $$ = $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* A single formal parameter */
    func_fparam
    {
      $$ = new std::vector<frontend::FuncFParamNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

/* A single formal parameter */
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
    |
    /* Array parameter */
    INT IDENTIFIER func_fparam_indices
    {
      $$ = std::make_unique<frontend::ast::FuncFParamNode>(@$, driver.GetContext().GetTypeContext()->GetPtrType(driver.GetContext().GetTypeContext()->GetIntegerType()), std::move($2), std::move(*$3));
      delete $3;
    }
    |
    FLOAT IDENTIFIER func_fparam_indices
    {
      $$ = std::make_unique<frontend::ast::FuncFParamNode>(@$, driver.GetContext().GetTypeContext()->GetPtrType(driver.GetContext().GetTypeContext()->GetFloatType()), std::move($2), std::move(*$3));
      delete $3;
    }
    ;

func_fparam_indices:
    /* func_fparam_indices [ expr ] */
    func_fparam_indices LSBRACE expr RSBRACE
    {
      $$ = $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* The first index is nullptr */
    LSBRACE RSBRACE
    {
      $$ = new std::vector<frontend::ExprNodePtr>();
      $$->emplace_back(nullptr);
    }
    ;

/* Statement list which is a vector of statements */
stmt_list:
    stmt_list stmt
    {
      $$ = $1;
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
    /* Expression statement: expr; */
    exprstmt
    {
      $$ = std::move($1);
    }
    |
    /* AssignStmt: Lval = expr */
    /* Note that we will use "expr = expr" here, and we will check the left one in semantic analysis */
    assignstmt
    {
      $$ = std::move($1);
    }
    |
    /* BlockStmt: { stmt_list } */
    blockstmt
    {
      $$ = std::move($1);
    }
    |
    /* IfStmt: IF(cond)[ELSE] */
    ifstmt
    {
      $$ = std::move($1);
    }
    |
    /* WhileStmt: while(cond) Stmt */
    whilestmt
    {
      $$ = std::move($1);
    }
    |
    /* BreakStmt */
    breakstmt
    {
      $$ = std::move($1);
    }
    |
    /* ContinueStmt */
    continuestmt
    {
      $$ = std::move($1);
    }
    |
    /* ReturnStmt: return [exp]; */
    returnstmt
    {
      $$ = std::move($1);
    }
    /* A special type of statement */
    /* Declaration, I think it can be also considered as a statement */
    |
    decl
    {
      $$ = std::move($1);
    }
    ;

exprstmt:
    /* A single SEMICOLON(;), same as empty statement */
    SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ExprStmtNode>(@$, nullptr);
    }
    |
    expr SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ExprStmtNode>(@$, std::move($1));
    }
    ;

assignstmt:
    /* lval = expr ; */
    lval ASSIGN expr SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::AssignStmtNode>(@$, std::move($1), std::move($3));
    }
    ;

blockstmt:
    /* {stmt_list}(stmt_list can be empty or not) */
    LCBRACE stmt_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::BlockStmtNode>(@$, std::move(*$2));
      delete $2;
    }
    |
    LCBRACE RCBRACE
    {
      $$ = std::make_unique<frontend::ast::BlockStmtNode>(@$, std::vector<frontend::StmtNodePtr>());
    }
    ;
  
breakstmt:
    BREAK SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::BreakStmtNode>(@$);
    }
    ;

continuestmt:
    CONTINUE SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ContinueStmtNode>(@$);
    }
    ;

returnstmt:
    RETURN SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ReturnStmtNode>(@$);
    }
    |
    RETURN expr SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::ReturnStmtNode>(@$, std::move($2));
    }
    ;

ifstmt:
    IF LPAREN expr RPAREN stmt %prec THEN
    {
      $$ = std::make_unique<frontend::ast::IfStmtNode>(@$, std::move($3), std::move($5));
    }
    |
    IF LPAREN expr RPAREN stmt ELSE stmt
    {
      $$ = std::make_unique<frontend::ast::IfStmtNode>(@$, std::move($3), std::move($5), std::move($7));
    }
    ;

whilestmt:
    WHILE LPAREN expr RPAREN stmt
    {
      $$ = std::make_unique<frontend::ast::WhileStmtNode>(@$, std::move($3), std::move($5));
    }
    ;

/* Expressions */
expr:
    /* Algorithm Operator */
    expr ADD expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::PLUS, std::move($1), std::move($3));
    }
    |
    expr SUB expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::SUB, std::move($1), std::move($3));
    }
    |
    expr MUL expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::MUL, std::move($1), std::move($3));
    }
    |
    expr DIV expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::DIV, std::move($1), std::move($3));
    }
    |
    expr MOD expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::MOD, std::move($1), std::move($3));
    }
    |
    SUB expr %prec UMINUS
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NEGATIVE, std::move($2));
    }
    |
    ADD expr %prec UPLUS
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::POSITIVE, std::move($2));
    }
    |
    /* Logical Operator */
    NOT expr
    {
      $$ = std::make_unique<frontend::ast::UnaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NOT, std::move($2));
    }
    |
    expr AND expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::AND, std::move($1), std::move($3));
    }
    |
    expr OR expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::OR, std::move($1), std::move($3));
    }
    |
    /* Relational Operator */
    expr LT expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::LESS, std::move($1), std::move($3));
    }
    |
    expr GT expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::GREATER, std::move($1), std::move($3));
    }
    |
    expr LE expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::LEQUAL, std::move($1), std::move($3));
    }
    |
    expr GE expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::GEQUAL, std::move($1), std::move($3));
    }
    |
    expr EQ expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::EQUAL, std::move($1), std::move($3));
    }
    |
    expr NEQ expr
    {
      $$ = std::make_unique<frontend::ast::BinaryExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NEQUAL, std::move($1), std::move($3));
    }
    |
    /* ( expr ) */
    LPAREN expr RPAREN
    {
      $$ = std::move($2);
    }
    |
    /* Lval */
    lval
    {
      $$ = std::move($1);
    }
    |
    /* Function call */
    funccall
    {
      $$ = std::move($1);
    }
    |
    /* Const values */
    INTCONST
    {
      $$ = std::make_unique<frontend::ast::IntLiteralExprNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), $1);
      $$->GetValue().SetIntValue($1);
    }
    |
    FLOATCONST
    {
      $$ = std::make_unique<frontend::ast::FloatLiteralExprNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), $1);
      $$->GetValue().SetFloatValue($1);
    }
    ;

lval:
    IDENTIFIER
    {
      $$ = std::make_unique<frontend::ast::LValExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), std::move($1));
    }
    |
    /* Array Type */
    IDENTIFIER array_indices
    {
      $$ = std::make_unique<frontend::ast::LValExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), std::move($1), std::move(*$2));
      delete $2;
    }
    ;

funccall:
    /* No parameters */
    IDENTIFIER LPAREN RPAREN
    {
      $$ = std::make_unique<frontend::ast::FuncCallExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), std::move($1));
    }
    |
    /* With parameters */
    IDENTIFIER LPAREN func_rparams RPAREN
    {
      $$ = std::make_unique<frontend::ast::FuncCallExprNode>(@$, driver.GetContext().GetTypeContext()->GetUnknownType(), std::move($1), std::move(*$3));
      delete $3;
    }
    ;

/* FuncRParams represents the Real parameters in FuncCall */
func_rparams:
    /* Note: the parameter is not necessarily a Lval */
    func_rparams COMMA expr
    {
      $$ = $1;
      $$->emplace_back(std::move($3));
    }
    |
    expr
    {
      $$ = new std::vector<frontend::ExprNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;

decl:
    /* Variation */
    INT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::LocalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move(*$2));
      delete $2;
    }
    |
    FLOAT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::LocalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move(*$2));
      delete $2;
    }
    /* Const Variation */
    |
    CONST INT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::LocalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetIntegerType(), std::move(*$3), true);
      delete $3;
    }
    |
    CONST FLOAT def_list SEMICOLON
    {
      $$ = std::make_unique<frontend::ast::LocalVarDeclNode>(@$, driver.GetContext().GetTypeContext()->GetFloatType(), std::move(*$3), true);
      delete $3;
    }
    ;

initials:
    /* Two kinds of initializer */
    /* 1. Single value initializer */
    /* 2. List initializer */
    /* Single initializer */
    expr
    {
      $$ = std::make_unique<frontend::ast::InitValNode>(@$, std::move($1));
    }
    /* List initializer */
    /* i.e., {1, {1,2}, 3, {{1,2,3}}} */
    | LCBRACE initials_list RCBRACE
    {
      $$ = std::make_unique<frontend::ast::InitListNode>(@$, std::move(*$2));
      delete $2;
    }
    | LCBRACE RCBRACE
    {
      $$ = std::make_unique<frontend::ast::InitListNode>(@$);
    }
    ;

initials_list:
    /* initials_list , initials */
    initials_list COMMA initials
    {
      $$ = $1;
      $$->emplace_back(std::move($3));
    }
    |
    /* A single initializer */
    initials
    {
      $$ = new std::vector<frontend::InitializerNodePtr>();
      $$->emplace_back(std::move($1));
    }
    ;
%%