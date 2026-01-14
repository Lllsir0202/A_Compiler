// We add all necessary includes in frontend.h
// so we do not need to include them one by one in main.cpp

#include "ast/ast_base.h"
#include "ast/ast_expr.h"
#include "ast/ast_spec.h"
#include "ast/ast_stmt.h"
#include "ast/ast_utils.h"
#include "ast/astvisitor.h"
#include "lexer/flex_lexer_impl.h"
#include "lexer/lexer.h"
#include "parser/parser_utils.h"
