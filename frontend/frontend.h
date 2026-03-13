// We add all necessary includes in frontend.h
// so we do not need to include them one by one in main.cpp
#include "ast/ast_base.h"
#include "ast/ast_decl.h"
#include "ast/ast_spec.h"
#include "ast/ast_stmt.h"
#include "ast/ast_utils.h"
#include "lexer/flex_lexer_impl.h"
#include "lexer/lexer.h"
#include "parser/bison_parser_impl.h"
#include "parser/parser.h"
#include "parser/parser_utils.h"

// Following are visitors
#include "ast/astvisitor.h"
#include "ast/visitor/printer/astprinter.h"
#include "ast/visitor/semantic/sem_analysis.h"