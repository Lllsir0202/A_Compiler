// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

#include "bison.h"

// Unqualified %code blocks.
#line 42 "frontend/parser/bison.y"

// Using rewritten YYLLOC_DEFAULT macro to adapt to SourceRange
#define YYLLOC_DEFAULT(Res, Rhs, N)           \
  do {                                        \
    if (N) {                                  \
      (Res).begin_ = YYRHSLOC(Rhs, 1).begin_; \
      (Res).end_ = YYRHSLOC(Rhs, N).end_;     \
    } else {                                  \
      (Res).begin_ = YYRHSLOC(Rhs, 0).end_;   \
      (Res).end_ = YYRHSLOC(Rhs, 0).end_;     \
    }                                         \
  } while (false)
#line 62 "frontend/parser/bison.y"

void yy::parser::error(const frontend::SourceRange &loc, const std::string &msg) { driver.ReportError(loc, msg); }
yy::parser::symbol_type yylex(frontend::BisonParserImpl &driver) {
  auto token = driver.GetLexer()->GetNextToken();
  switch (token.GetTokenType()) {
#define NOVAL(name, bisontype)                                   \
  case frontend::TokenType::name: {                              \
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

#line 96 "frontend/parser/bison.cpp"

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h>  // FIXME: INFRINGES ON USER NAME SPACE.
#define YY_(msgid) dgettext("bison-runtime", msgid)
#endif
#endif
#ifndef YY_
#define YY_(msgid) msgid
#endif
#endif

// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
#if defined __GNUC__ && !defined __EXCEPTIONS
#define YY_EXCEPTIONS 0
#else
#define YY_EXCEPTIONS 1
#endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N)                       \
  do                                                          \
    if (N) {                                                  \
      (Current).begin = YYRHSLOC(Rhs, 1).begin;               \
      (Current).end = YYRHSLOC(Rhs, N).end;                   \
    } else {                                                  \
      (Current).begin = (Current).end = YYRHSLOC(Rhs, 0).end; \
    }                                                         \
  while (false)
#endif

// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
#define YYCDEBUG \
  if (yydebug_) (*yycdebug_)

#define YY_SYMBOL_PRINT(Title, Symbol) \
  do {                                 \
    if (yydebug_) {                    \
      *yycdebug_ << Title << ' ';      \
      yy_print_(*yycdebug_, Symbol);   \
      *yycdebug_ << '\n';              \
    }                                  \
  } while (false)

#define YY_REDUCE_PRINT(Rule)             \
  do {                                    \
    if (yydebug_) yy_reduce_print_(Rule); \
  } while (false)

#define YY_STACK_PRINT()             \
  do {                               \
    if (yydebug_) yy_stack_print_(); \
  } while (false)

#else  // !YYDEBUG

#define YYCDEBUG \
  if (false) std::cerr
#define YY_SYMBOL_PRINT(Title, Symbol) YY_USE(Symbol)
#define YY_REDUCE_PRINT(Rule) static_cast<void>(0)
#define YY_STACK_PRINT() static_cast<void>(0)

#endif  // !YYDEBUG

#define yyerrok (yyerrstatus_ = 0)
#define yyclearin (yyla.clear())

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab
#define YYRECOVERING() (!!yyerrstatus_)

namespace yy {
#line 188 "frontend/parser/bison.cpp"

/// Build a parser object.
parser::parser(frontend::BisonParserImpl &driver_yyarg)
#if YYDEBUG
    : yydebug_(false),
      yycdebug_(&std::cerr),
#else
    :
#endif
      driver(driver_yyarg) {
}

parser::~parser() {}

parser::syntax_error::~syntax_error() YY_NOEXCEPT YY_NOTHROW {}

/*---------.
| symbol.  |
`---------*/

// by_state.
parser::by_state::by_state() YY_NOEXCEPT : state(empty_state) {}

parser::by_state::by_state(const by_state &that) YY_NOEXCEPT : state(that.state) {}

void parser::by_state::clear() YY_NOEXCEPT { state = empty_state; }

void parser::by_state::move(by_state &that) {
  state = that.state;
  that.clear();
}

parser::by_state::by_state(state_type s) YY_NOEXCEPT : state(s) {}

parser::symbol_kind_type parser::by_state::kind() const YY_NOEXCEPT {
  if (state == empty_state)
    return symbol_kind::S_YYEMPTY;
  else
    return YY_CAST(symbol_kind_type, yystos_[+state]);
}

parser::stack_symbol_type::stack_symbol_type() {}

parser::stack_symbol_type::stack_symbol_type(YY_RVREF(stack_symbol_type) that)
    : super_type(YY_MOVE(that.state), YY_MOVE(that.location)) {
  switch (that.kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.YY_MOVE_OR_COPY<float>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.YY_MOVE_OR_COPY<frontend::ASTNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.YY_MOVE_OR_COPY<frontend::DeclNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.YY_MOVE_OR_COPY<frontend::ExprNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.YY_MOVE_OR_COPY<frontend::FuncFParamNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials:  // initials
      value.YY_MOVE_OR_COPY<frontend::InitializerNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_stmt:          // stmt
    case symbol_kind::S_exprstmt:      // exprstmt
    case symbol_kind::S_assignstmt:    // assignstmt
    case symbol_kind::S_blockstmt:     // blockstmt
    case symbol_kind::S_breakstmt:     // breakstmt
    case symbol_kind::S_continuestmt:  // continuestmt
    case symbol_kind::S_returnstmt:    // returnstmt
    case symbol_kind::S_ifstmt:        // ifstmt
    case symbol_kind::S_whilestmt:     // whilestmt
      value.YY_MOVE_OR_COPY<frontend::StmtNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def:  // def
      value.YY_MOVE_OR_COPY<frontend::VarDefNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.YY_MOVE_OR_COPY<int>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.YY_MOVE_OR_COPY<std::string>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_compunit:  // compunit
      value.YY_MOVE_OR_COPY<std::vector<frontend::ASTNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.YY_MOVE_OR_COPY<std::vector<frontend::ExprNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.YY_MOVE_OR_COPY<std::vector<frontend::FuncFParamNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.YY_MOVE_OR_COPY<std::vector<frontend::InitializerNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.YY_MOVE_OR_COPY<std::vector<frontend::StmtNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def_list:  // def_list
      value.YY_MOVE_OR_COPY<std::vector<frontend::VarDefNodePtr> *>(YY_MOVE(that.value));
      break;

    default:
      break;
  }

#if 201103L <= YY_CPLUSPLUS
  // that is emptied.
  that.state = empty_state;
#endif
}

parser::stack_symbol_type::stack_symbol_type(state_type s, YY_MOVE_REF(symbol_type) that)
    : super_type(s, YY_MOVE(that.location)) {
  switch (that.kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.move<float>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.move<frontend::ASTNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.move<frontend::DeclNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.move<frontend::ExprNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.move<frontend::FuncFParamNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials:  // initials
      value.move<frontend::InitializerNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_stmt:          // stmt
    case symbol_kind::S_exprstmt:      // exprstmt
    case symbol_kind::S_assignstmt:    // assignstmt
    case symbol_kind::S_blockstmt:     // blockstmt
    case symbol_kind::S_breakstmt:     // breakstmt
    case symbol_kind::S_continuestmt:  // continuestmt
    case symbol_kind::S_returnstmt:    // returnstmt
    case symbol_kind::S_ifstmt:        // ifstmt
    case symbol_kind::S_whilestmt:     // whilestmt
      value.move<frontend::StmtNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def:  // def
      value.move<frontend::VarDefNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.move<int>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.move<std::string>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_compunit:  // compunit
      value.move<std::vector<frontend::ASTNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.move<std::vector<frontend::ExprNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.move<std::vector<frontend::FuncFParamNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.move<std::vector<frontend::InitializerNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.move<std::vector<frontend::StmtNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def_list:  // def_list
      value.move<std::vector<frontend::VarDefNodePtr> *>(YY_MOVE(that.value));
      break;

    default:
      break;
  }

  // that is emptied.
  that.kind_ = symbol_kind::S_YYEMPTY;
}

#if YY_CPLUSPLUS < 201103L
parser::stack_symbol_type &parser::stack_symbol_type::operator=(const stack_symbol_type &that) {
  state = that.state;
  switch (that.kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.copy<float>(that.value);
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.copy<frontend::ASTNodePtr>(that.value);
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.copy<frontend::DeclNodePtr>(that.value);
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.copy<frontend::ExprNodePtr>(that.value);
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.copy<frontend::FuncFParamNodePtr>(that.value);
      break;

    case symbol_kind::S_initials:  // initials
      value.copy<frontend::InitializerNodePtr>(that.value);
      break;

    case symbol_kind::S_stmt:          // stmt
    case symbol_kind::S_exprstmt:      // exprstmt
    case symbol_kind::S_assignstmt:    // assignstmt
    case symbol_kind::S_blockstmt:     // blockstmt
    case symbol_kind::S_breakstmt:     // breakstmt
    case symbol_kind::S_continuestmt:  // continuestmt
    case symbol_kind::S_returnstmt:    // returnstmt
    case symbol_kind::S_ifstmt:        // ifstmt
    case symbol_kind::S_whilestmt:     // whilestmt
      value.copy<frontend::StmtNodePtr>(that.value);
      break;

    case symbol_kind::S_def:  // def
      value.copy<frontend::VarDefNodePtr>(that.value);
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.copy<int>(that.value);
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.copy<std::string>(that.value);
      break;

    case symbol_kind::S_compunit:  // compunit
      value.copy<std::vector<frontend::ASTNodePtr> *>(that.value);
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.copy<std::vector<frontend::ExprNodePtr> *>(that.value);
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.copy<std::vector<frontend::FuncFParamNodePtr> *>(that.value);
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.copy<std::vector<frontend::InitializerNodePtr> *>(that.value);
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.copy<std::vector<frontend::StmtNodePtr> *>(that.value);
      break;

    case symbol_kind::S_def_list:  // def_list
      value.copy<std::vector<frontend::VarDefNodePtr> *>(that.value);
      break;

    default:
      break;
  }

  location = that.location;
  return *this;
}

parser::stack_symbol_type &parser::stack_symbol_type::operator=(stack_symbol_type &that) {
  state = that.state;
  switch (that.kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.move<float>(that.value);
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.move<frontend::ASTNodePtr>(that.value);
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.move<frontend::DeclNodePtr>(that.value);
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.move<frontend::ExprNodePtr>(that.value);
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.move<frontend::FuncFParamNodePtr>(that.value);
      break;

    case symbol_kind::S_initials:  // initials
      value.move<frontend::InitializerNodePtr>(that.value);
      break;

    case symbol_kind::S_stmt:          // stmt
    case symbol_kind::S_exprstmt:      // exprstmt
    case symbol_kind::S_assignstmt:    // assignstmt
    case symbol_kind::S_blockstmt:     // blockstmt
    case symbol_kind::S_breakstmt:     // breakstmt
    case symbol_kind::S_continuestmt:  // continuestmt
    case symbol_kind::S_returnstmt:    // returnstmt
    case symbol_kind::S_ifstmt:        // ifstmt
    case symbol_kind::S_whilestmt:     // whilestmt
      value.move<frontend::StmtNodePtr>(that.value);
      break;

    case symbol_kind::S_def:  // def
      value.move<frontend::VarDefNodePtr>(that.value);
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.move<int>(that.value);
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.move<std::string>(that.value);
      break;

    case symbol_kind::S_compunit:  // compunit
      value.move<std::vector<frontend::ASTNodePtr> *>(that.value);
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.move<std::vector<frontend::ExprNodePtr> *>(that.value);
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.move<std::vector<frontend::FuncFParamNodePtr> *>(that.value);
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.move<std::vector<frontend::InitializerNodePtr> *>(that.value);
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.move<std::vector<frontend::StmtNodePtr> *>(that.value);
      break;

    case symbol_kind::S_def_list:  // def_list
      value.move<std::vector<frontend::VarDefNodePtr> *>(that.value);
      break;

    default:
      break;
  }

  location = that.location;
  // that is emptied.
  that.state = empty_state;
  return *this;
}
#endif

template <typename Base>
void parser::yy_destroy_(const char *yymsg, basic_symbol<Base> &yysym) const {
  if (yymsg) YY_SYMBOL_PRINT(yymsg, yysym);
}

#if YYDEBUG
template <typename Base>
void parser::yy_print_(std::ostream &yyo, const basic_symbol<Base> &yysym) const {
  std::ostream &yyoutput = yyo;
  YY_USE(yyoutput);
  if (yysym.empty())
    yyo << "empty symbol";
  else {
    symbol_kind_type yykind = yysym.kind();
    yyo << (yykind < YYNTOKENS ? "token" : "nterm") << ' ' << yysym.name() << " (" << yysym.location << ": ";
    YY_USE(yykind);
    yyo << ')';
  }
}
#endif

void parser::yypush_(const char *m, YY_MOVE_REF(stack_symbol_type) sym) {
  if (m) YY_SYMBOL_PRINT(m, sym);
  yystack_.push(YY_MOVE(sym));
}

void parser::yypush_(const char *m, state_type s, YY_MOVE_REF(symbol_type) sym) {
#if 201103L <= YY_CPLUSPLUS
  yypush_(m, stack_symbol_type(s, std::move(sym)));
#else
  stack_symbol_type ss(s, sym);
  yypush_(m, ss);
#endif
}

void parser::yypop_(int n) YY_NOEXCEPT { yystack_.pop(n); }

#if YYDEBUG
std::ostream &parser::debug_stream() const { return *yycdebug_; }

void parser::set_debug_stream(std::ostream &o) { yycdebug_ = &o; }

parser::debug_level_type parser::debug_level() const { return yydebug_; }

void parser::set_debug_level(debug_level_type l) { yydebug_ = l; }
#endif  // YYDEBUG

parser::state_type parser::yy_lr_goto_state_(state_type yystate, int yysym) {
  int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
    return yytable_[yyr];
  else
    return yydefgoto_[yysym - YYNTOKENS];
}

bool parser::yy_pact_value_is_default_(int yyvalue) YY_NOEXCEPT { return yyvalue == yypact_ninf_; }

bool parser::yy_table_value_is_error_(int yyvalue) YY_NOEXCEPT { return yyvalue == yytable_ninf_; }

int parser::operator()() { return parse(); }

int parser::parse() {
  int yyn;
  /// Length of the RHS of the rule being reduced.
  int yylen = 0;

  // Error handling.
  int yynerrs_ = 0;
  int yyerrstatus_ = 0;

  /// The lookahead symbol.
  symbol_type yyla;

  /// The locations where the error started and ended.
  stack_symbol_type yyerror_range[3];

  /// The return value of parse ().
  int yyresult;

#if YY_EXCEPTIONS
  try
#endif  // YY_EXCEPTIONS
  {
    YYCDEBUG << "Starting parse\n";

    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear();
    yypush_(YY_NULLPTR, 0, YY_MOVE(yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int(yystack_[0].state) << '\n';
    YY_STACK_PRINT();

    // Accept?
    if (yystack_[0].state == yyfinal_) YYACCEPT;

    goto yybackup;

  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_(yyn)) goto yydefault;

    // Read a lookahead token.
    if (yyla.empty()) {
      YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
      try
#endif  // YY_EXCEPTIONS
      {
        symbol_type yylookahead(yylex(driver));
        yyla.move(yylookahead);
      }
#if YY_EXCEPTIONS
      catch (const syntax_error &yyexc) {
        YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
        error(yyexc);
        goto yyerrlab1;
      }
#endif  // YY_EXCEPTIONS
    }
    YY_SYMBOL_PRINT("Next token is", yyla);

    if (yyla.kind() == symbol_kind::S_YYerror) {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind()) {
      goto yydefault;
    }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0) {
      if (yy_table_value_is_error_(yyn)) goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_) --yyerrstatus_;

    // Shift the lookahead token.
    yypush_("Shifting", state_type(yyn), YY_MOVE(yyla));
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0) goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn]) {
        case symbol_kind::S_FLOATCONST:  // "float constant"
          yylhs.value.emplace<float>();
          break;

        case symbol_kind::S_program:  // program
        case symbol_kind::S_funcdef:  // funcdef
          yylhs.value.emplace<frontend::ASTNodePtr>();
          break;

        case symbol_kind::S_global_decl:  // global_decl
        case symbol_kind::S_decl:         // decl
          yylhs.value.emplace<frontend::DeclNodePtr>();
          break;

        case symbol_kind::S_expr:      // expr
        case symbol_kind::S_lval:      // lval
        case symbol_kind::S_funccall:  // funccall
          yylhs.value.emplace<frontend::ExprNodePtr>();
          break;

        case symbol_kind::S_func_fparam:  // func_fparam
          yylhs.value.emplace<frontend::FuncFParamNodePtr>();
          break;

        case symbol_kind::S_initials:  // initials
          yylhs.value.emplace<frontend::InitializerNodePtr>();
          break;

        case symbol_kind::S_stmt:          // stmt
        case symbol_kind::S_exprstmt:      // exprstmt
        case symbol_kind::S_assignstmt:    // assignstmt
        case symbol_kind::S_blockstmt:     // blockstmt
        case symbol_kind::S_breakstmt:     // breakstmt
        case symbol_kind::S_continuestmt:  // continuestmt
        case symbol_kind::S_returnstmt:    // returnstmt
        case symbol_kind::S_ifstmt:        // ifstmt
        case symbol_kind::S_whilestmt:     // whilestmt
          yylhs.value.emplace<frontend::StmtNodePtr>();
          break;

        case symbol_kind::S_def:  // def
          yylhs.value.emplace<frontend::VarDefNodePtr>();
          break;

        case symbol_kind::S_INTCONST:  // "integer constant"
          yylhs.value.emplace<int>();
          break;

        case symbol_kind::S_IDENTIFIER:  // "identifier"
          yylhs.value.emplace<std::string>();
          break;

        case symbol_kind::S_compunit:  // compunit
          yylhs.value.emplace<std::vector<frontend::ASTNodePtr> *>();
          break;

        case symbol_kind::S_array_indices:        // array_indices
        case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
        case symbol_kind::S_func_rparams:         // func_rparams
          yylhs.value.emplace<std::vector<frontend::ExprNodePtr> *>();
          break;

        case symbol_kind::S_func_fparams:  // func_fparams
          yylhs.value.emplace<std::vector<frontend::FuncFParamNodePtr> *>();
          break;

        case symbol_kind::S_initials_list:  // initials_list
          yylhs.value.emplace<std::vector<frontend::InitializerNodePtr> *>();
          break;

        case symbol_kind::S_stmt_list:  // stmt_list
          yylhs.value.emplace<std::vector<frontend::StmtNodePtr> *>();
          break;

        case symbol_kind::S_def_list:  // def_list
          yylhs.value.emplace<std::vector<frontend::VarDefNodePtr> *>();
          break;

        default:
          break;
      }

      // Default location.
      {
        stack_type::slice range(yystack_, yylen);
        YYLLOC_DEFAULT(yylhs.location, range, yylen);
        yyerror_range[1].location = yylhs.location;
      }

      // Perform the reduction.
      YY_REDUCE_PRINT(yyn);
#if YY_EXCEPTIONS
      try
#endif  // YY_EXCEPTIONS
      {
        switch (yyn) {
          case 2:  // program: compunit
#line 188 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::RootNode>(
                std::move(*yystack_[0].value.as<std::vector<frontend::ASTNodePtr> *>()));
            delete yystack_[0].value.as<std::vector<frontend::ASTNodePtr> *>();
            driver.GetContext().GetAstContext()->SetRootNode(std::move(yylhs.value.as<frontend::ASTNodePtr>()));
          }
#line 976 "frontend/parser/bison.cpp"
          break;

          case 3:  // program: program END
#line 194 "frontend/parser/bison.y"
          {
            YYACCEPT;
          }
#line 984 "frontend/parser/bison.cpp"
          break;

          case 4:  // compunit: compunit global_decl
#line 202 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>() =
                yystack_[1].value.as<std::vector<frontend::ASTNodePtr> *>();
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::DeclNodePtr>()));
          }
#line 993 "frontend/parser/bison.cpp"
          break;

          case 5:  // compunit: compunit funcdef
#line 208 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>() =
                yystack_[1].value.as<std::vector<frontend::ASTNodePtr> *>();
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::ASTNodePtr>()));
          }
#line 1002 "frontend/parser/bison.cpp"
          break;

          case 6:  // compunit: global_decl
#line 214 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>() = new std::vector<frontend::ASTNodePtr>();
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::DeclNodePtr>()));
          }
#line 1011 "frontend/parser/bison.cpp"
          break;

          case 7:  // compunit: funcdef
#line 220 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>() = new std::vector<frontend::ASTNodePtr>();
            yylhs.value.as<std::vector<frontend::ASTNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::ASTNodePtr>()));
          }
#line 1020 "frontend/parser/bison.cpp"
          break;

          case 8:  // global_decl: INT def_list SEMICOLON
#line 230 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::GlobalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1029 "frontend/parser/bison.cpp"
          break;

          case 9:  // global_decl: FLOAT def_list SEMICOLON
#line 236 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::GlobalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1038 "frontend/parser/bison.cpp"
          break;

          case 10:  // global_decl: CONST INT def_list SEMICOLON
#line 243 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::GlobalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()), true);
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1047 "frontend/parser/bison.cpp"
          break;

          case 11:  // global_decl: CONST FLOAT def_list SEMICOLON
#line 249 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::GlobalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()), true);
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1056 "frontend/parser/bison.cpp"
          break;

          case 12:  // def_list: def_list COMMA def
#line 260 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::VarDefNodePtr> *>() =
                yystack_[2].value.as<std::vector<frontend::VarDefNodePtr> *>();
            yylhs.value.as<std::vector<frontend::VarDefNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::VarDefNodePtr>()));
          }
#line 1065 "frontend/parser/bison.cpp"
          break;

          case 13:  // def_list: def
#line 267 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::VarDefNodePtr> *>() = new std::vector<frontend::VarDefNodePtr>();
            yylhs.value.as<std::vector<frontend::VarDefNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::VarDefNodePtr>()));
          }
#line 1074 "frontend/parser/bison.cpp"
          break;

          case 14:  // def: "identifier"
#line 276 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::VarDefNodePtr>() = std::make_unique<frontend::ast::VarDefNode>(
                yylhs.location, std::move(yystack_[0].value.as<std::string>()), nullptr);
          }
#line 1082 "frontend/parser/bison.cpp"
          break;

          case 15:  // def: "identifier" ASSIGN initials
#line 281 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::VarDefNodePtr>() = std::make_unique<frontend::ast::VarDefNode>(
                yylhs.location, std::move(yystack_[2].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::InitializerNodePtr>()));
          }
#line 1090 "frontend/parser/bison.cpp"
          break;

          case 16:  // def: "identifier" array_indices
#line 287 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::VarDefNodePtr>() = std::make_unique<frontend::ast::VarDefNode>(
                yylhs.location, std::move(yystack_[1].value.as<std::string>()), nullptr,
                std::move(*yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1099 "frontend/parser/bison.cpp"
          break;

          case 17:  // def: "identifier" array_indices ASSIGN initials
#line 293 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::VarDefNodePtr>() = std::make_unique<frontend::ast::VarDefNode>(
                yylhs.location, std::move(yystack_[3].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::InitializerNodePtr>()),
                std::move(*yystack_[2].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[2].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1108 "frontend/parser/bison.cpp"
          break;

          case 18:  // array_indices: array_indices LSBRACE expr RSBRACE
#line 302 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() =
                yystack_[3].value.as<std::vector<frontend::ExprNodePtr> *>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(
                std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1117 "frontend/parser/bison.cpp"
          break;

          case 19:  // array_indices: LSBRACE expr RSBRACE
#line 310 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() = new std::vector<frontend::ExprNodePtr>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(
                std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1126 "frontend/parser/bison.cpp"
          break;

          case 20:  // funcdef: VOID "identifier" LPAREN RPAREN blockstmt
#line 320 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetVoidType(),
                std::move(yystack_[3].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1134 "frontend/parser/bison.cpp"
          break;

          case 21:  // funcdef: INT "identifier" LPAREN RPAREN blockstmt
#line 325 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(yystack_[3].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1142 "frontend/parser/bison.cpp"
          break;

          case 22:  // funcdef: FLOAT "identifier" LPAREN RPAREN blockstmt
#line 330 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(yystack_[3].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1150 "frontend/parser/bison.cpp"
          break;

          case 23:  // funcdef: VOID "identifier" LPAREN func_fparams RPAREN blockstmt
#line 336 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetVoidType(),
                std::move(yystack_[4].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()),
                std::move(*yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>()));
            delete yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>();
          }
#line 1159 "frontend/parser/bison.cpp"
          break;

          case 24:  // funcdef: INT "identifier" LPAREN func_fparams RPAREN blockstmt
#line 342 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(yystack_[4].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()),
                std::move(*yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>()));
            delete yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>();
          }
#line 1168 "frontend/parser/bison.cpp"
          break;

          case 25:  // funcdef: FLOAT "identifier" LPAREN func_fparams RPAREN blockstmt
#line 348 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ASTNodePtr>() = std::make_unique<frontend::ast::FuncDefNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(yystack_[4].value.as<std::string>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()),
                std::move(*yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>()));
            delete yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>();
          }
#line 1177 "frontend/parser/bison.cpp"
          break;

          case 26:  // func_fparams: func_fparams COMMA func_fparam
#line 358 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::FuncFParamNodePtr> *>() =
                yystack_[2].value.as<std::vector<frontend::FuncFParamNodePtr> *>();
            yylhs.value.as<std::vector<frontend::FuncFParamNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::FuncFParamNodePtr>()));
          }
#line 1186 "frontend/parser/bison.cpp"
          break;

          case 27:  // func_fparams: func_fparam
#line 365 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::FuncFParamNodePtr> *>() =
                new std::vector<frontend::FuncFParamNodePtr>();
            yylhs.value.as<std::vector<frontend::FuncFParamNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::FuncFParamNodePtr>()));
          }
#line 1195 "frontend/parser/bison.cpp"
          break;

          case 28:  // func_fparam: INT "identifier"
#line 374 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::FuncFParamNodePtr>() = std::make_unique<frontend::ast::FuncFParamNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(yystack_[0].value.as<std::string>()));
          }
#line 1203 "frontend/parser/bison.cpp"
          break;

          case 29:  // func_fparam: FLOAT "identifier"
#line 379 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::FuncFParamNodePtr>() = std::make_unique<frontend::ast::FuncFParamNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(yystack_[0].value.as<std::string>()));
          }
#line 1211 "frontend/parser/bison.cpp"
          break;

          case 30:  // func_fparam: INT "identifier" func_fparam_indices
#line 386 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::FuncFParamNodePtr>() = std::make_unique<frontend::ast::FuncFParamNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(yystack_[1].value.as<std::string>()),
                std::move(*yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1220 "frontend/parser/bison.cpp"
          break;

          case 31:  // func_fparam: FLOAT "identifier" func_fparam_indices
#line 392 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::FuncFParamNodePtr>() = std::make_unique<frontend::ast::FuncFParamNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(yystack_[1].value.as<std::string>()),
                std::move(*yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1229 "frontend/parser/bison.cpp"
          break;

          case 32:  // func_fparam_indices: func_fparam_indices LSBRACE expr RSBRACE
#line 401 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() =
                yystack_[3].value.as<std::vector<frontend::ExprNodePtr> *>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(
                std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1238 "frontend/parser/bison.cpp"
          break;

          case 33:  // func_fparam_indices: LSBRACE RSBRACE
#line 408 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() = new std::vector<frontend::ExprNodePtr>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(nullptr);
          }
#line 1247 "frontend/parser/bison.cpp"
          break;

          case 34:  // stmt_list: stmt_list stmt
#line 417 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::StmtNodePtr> *>() =
                yystack_[1].value.as<std::vector<frontend::StmtNodePtr> *>();
            yylhs.value.as<std::vector<frontend::StmtNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1256 "frontend/parser/bison.cpp"
          break;

          case 35:  // stmt_list: stmt
#line 423 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::StmtNodePtr> *>() = new std::vector<frontend::StmtNodePtr>();
            yylhs.value.as<std::vector<frontend::StmtNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1265 "frontend/parser/bison.cpp"
          break;

          case 36:  // stmt: exprstmt
#line 433 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1273 "frontend/parser/bison.cpp"
          break;

          case 37:  // stmt: assignstmt
#line 440 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1281 "frontend/parser/bison.cpp"
          break;

          case 38:  // stmt: blockstmt
#line 446 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1289 "frontend/parser/bison.cpp"
          break;

          case 39:  // stmt: ifstmt
#line 452 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1297 "frontend/parser/bison.cpp"
          break;

          case 40:  // stmt: whilestmt
#line 458 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1305 "frontend/parser/bison.cpp"
          break;

          case 41:  // stmt: breakstmt
#line 464 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1313 "frontend/parser/bison.cpp"
          break;

          case 42:  // stmt: continuestmt
#line 470 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1321 "frontend/parser/bison.cpp"
          break;

          case 43:  // stmt: returnstmt
#line 476 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::StmtNodePtr>());
          }
#line 1329 "frontend/parser/bison.cpp"
          break;

          case 44:  // stmt: decl
#line 483 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::move(yystack_[0].value.as<frontend::DeclNodePtr>());
          }
#line 1337 "frontend/parser/bison.cpp"
          break;

          case 45:  // exprstmt: SEMICOLON
#line 491 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() =
                std::make_unique<frontend::ast::ExprStmtNode>(yylhs.location, nullptr);
          }
#line 1345 "frontend/parser/bison.cpp"
          break;

          case 46:  // exprstmt: expr SEMICOLON
#line 496 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::ExprStmtNode>(
                yylhs.location, std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1353 "frontend/parser/bison.cpp"
          break;

          case 47:  // assignstmt: lval ASSIGN expr SEMICOLON
#line 504 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::AssignStmtNode>(
                yylhs.location, std::move(yystack_[3].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1361 "frontend/parser/bison.cpp"
          break;

          case 48:  // blockstmt: LCBRACE stmt_list RCBRACE
#line 512 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::BlockStmtNode>(
                yylhs.location, std::move(*yystack_[1].value.as<std::vector<frontend::StmtNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::StmtNodePtr> *>();
          }
#line 1370 "frontend/parser/bison.cpp"
          break;

          case 49:  // blockstmt: LCBRACE RCBRACE
#line 518 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() =
                std::make_unique<frontend::ast::BlockStmtNode>(yylhs.location, std::vector<frontend::StmtNodePtr>());
          }
#line 1378 "frontend/parser/bison.cpp"
          break;

          case 50:  // breakstmt: BREAK SEMICOLON
#line 525 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::BreakStmtNode>(yylhs.location);
          }
#line 1386 "frontend/parser/bison.cpp"
          break;

          case 51:  // continuestmt: CONTINUE SEMICOLON
#line 532 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::ContinueStmtNode>(yylhs.location);
          }
#line 1394 "frontend/parser/bison.cpp"
          break;

          case 52:  // returnstmt: RETURN SEMICOLON
#line 539 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::ReturnStmtNode>(yylhs.location);
          }
#line 1402 "frontend/parser/bison.cpp"
          break;

          case 53:  // returnstmt: RETURN expr SEMICOLON
#line 544 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::ReturnStmtNode>(
                yylhs.location, std::move(yystack_[1].value.as<frontend::ExprNodePtr>()));
          }
#line 1410 "frontend/parser/bison.cpp"
          break;

          case 54:  // ifstmt: IF LPAREN expr RPAREN stmt
#line 551 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::IfStmtNode>(
                yylhs.location, std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1418 "frontend/parser/bison.cpp"
          break;

          case 55:  // ifstmt: IF LPAREN expr RPAREN stmt ELSE stmt
#line 556 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::IfStmtNode>(
                yylhs.location, std::move(yystack_[4].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[2].value.as<frontend::StmtNodePtr>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1426 "frontend/parser/bison.cpp"
          break;

          case 56:  // whilestmt: WHILE LPAREN expr RPAREN stmt
#line 563 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::StmtNodePtr>() = std::make_unique<frontend::ast::WhileStmtNode>(
                yylhs.location, std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::StmtNodePtr>()));
          }
#line 1434 "frontend/parser/bison.cpp"
          break;

          case 57:  // expr: expr ADD expr
#line 572 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::PLUS,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1442 "frontend/parser/bison.cpp"
          break;

          case 58:  // expr: expr SUB expr
#line 577 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::SUB,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1450 "frontend/parser/bison.cpp"
          break;

          case 59:  // expr: expr MUL expr
#line 582 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::MUL,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1458 "frontend/parser/bison.cpp"
          break;

          case 60:  // expr: expr DIV expr
#line 587 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::DIV,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1466 "frontend/parser/bison.cpp"
          break;

          case 61:  // expr: expr MOD expr
#line 592 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::MOD,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1474 "frontend/parser/bison.cpp"
          break;

          case 62:  // expr: SUB expr
#line 597 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::UnaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NEGATIVE,
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1482 "frontend/parser/bison.cpp"
          break;

          case 63:  // expr: ADD expr
#line 602 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::UnaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::POSITIVE,
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1490 "frontend/parser/bison.cpp"
          break;

          case 64:  // expr: NOT expr
#line 608 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::UnaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NOT,
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1498 "frontend/parser/bison.cpp"
          break;

          case 65:  // expr: expr AND expr
#line 613 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::AND,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1506 "frontend/parser/bison.cpp"
          break;

          case 66:  // expr: expr OR expr
#line 618 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::OR,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1514 "frontend/parser/bison.cpp"
          break;

          case 67:  // expr: expr LT expr
#line 624 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::LESS,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1522 "frontend/parser/bison.cpp"
          break;

          case 68:  // expr: expr GT expr
#line 629 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::GREATER,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1530 "frontend/parser/bison.cpp"
          break;

          case 69:  // expr: expr LE expr
#line 634 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::LEQUAL,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1538 "frontend/parser/bison.cpp"
          break;

          case 70:  // expr: expr GE expr
#line 639 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::GEQUAL,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1546 "frontend/parser/bison.cpp"
          break;

          case 71:  // expr: expr EQ expr
#line 644 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::EQUAL,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1554 "frontend/parser/bison.cpp"
          break;

          case 72:  // expr: expr NEQ expr
#line 649 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::BinaryExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(), frontend::ast::OpCode::NEQUAL,
                std::move(yystack_[2].value.as<frontend::ExprNodePtr>()),
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1562 "frontend/parser/bison.cpp"
          break;

          case 73:  // expr: LPAREN expr RPAREN
#line 655 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::move(yystack_[1].value.as<frontend::ExprNodePtr>());
          }
#line 1570 "frontend/parser/bison.cpp"
          break;

          case 74:  // expr: lval
#line 661 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::move(yystack_[0].value.as<frontend::ExprNodePtr>());
          }
#line 1578 "frontend/parser/bison.cpp"
          break;

          case 75:  // expr: funccall
#line 667 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::move(yystack_[0].value.as<frontend::ExprNodePtr>());
          }
#line 1586 "frontend/parser/bison.cpp"
          break;

          case 76:  // expr: "integer constant"
#line 673 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::IntLiteralExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(), yystack_[0].value.as<int>());
            yylhs.value.as<frontend::ExprNodePtr>()->GetValue().SetIntValue(yystack_[0].value.as<int>());
          }
#line 1595 "frontend/parser/bison.cpp"
          break;

          case 77:  // expr: "float constant"
#line 679 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::FloatLiteralExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(), yystack_[0].value.as<float>());
            yylhs.value.as<frontend::ExprNodePtr>()->GetValue().SetFloatValue(yystack_[0].value.as<float>());
          }
#line 1604 "frontend/parser/bison.cpp"
          break;

          case 78:  // lval: "identifier"
#line 687 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::LValExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(),
                std::move(yystack_[0].value.as<std::string>()));
          }
#line 1612 "frontend/parser/bison.cpp"
          break;

          case 79:  // lval: "identifier" array_indices
#line 693 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::LValExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(),
                std::move(yystack_[1].value.as<std::string>()),
                std::move(*yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[0].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1621 "frontend/parser/bison.cpp"
          break;

          case 80:  // funccall: "identifier" LPAREN RPAREN
#line 702 "frontend/parser/bison.y"
          {
            if (yystack_[2].value.as<std::string>() == "starttime") {
              int line = yystack_[2].location.GetBegin().line_;
              auto tmp = std::make_unique<frontend::ast::IntLiteralExprNode>(
                  yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(), line);
              tmp->GetValue().SetIntValue(line);
              auto rparams = new std::vector<frontend::ExprNodePtr>();
              rparams->emplace_back(std::move(tmp));
              yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::FuncCallExprNode>(
                  yylhs.location, driver.GetContext().GetTypeContext()->GetVoidType(), "_sysy_starttime",
                  std::move(*rparams));
              delete rparams;
            } else if (yystack_[2].value.as<std::string>() == "stoptime") {
              int line = yystack_[2].location.GetBegin().line_;
              auto tmp = std::make_unique<frontend::ast::IntLiteralExprNode>(
                  yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(), line);
              tmp->GetValue().SetIntValue(line);
              auto rparams = new std::vector<frontend::ExprNodePtr>();
              rparams->emplace_back(std::move(tmp));
              yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::FuncCallExprNode>(
                  yylhs.location, driver.GetContext().GetTypeContext()->GetVoidType(), "_sysy_stoptime",
                  std::move(*rparams));
              delete rparams;
            } else {
              yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::FuncCallExprNode>(
                  yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(),
                  std::move(yystack_[2].value.as<std::string>()));
            }
          }
#line 1647 "frontend/parser/bison.cpp"
          break;

          case 81:  // funccall: "identifier" LPAREN func_rparams RPAREN
#line 726 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::ExprNodePtr>() = std::make_unique<frontend::ast::FuncCallExprNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetUnknownType(),
                std::move(yystack_[3].value.as<std::string>()),
                std::move(*yystack_[1].value.as<std::vector<frontend::ExprNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::ExprNodePtr> *>();
          }
#line 1656 "frontend/parser/bison.cpp"
          break;

          case 82:  // func_rparams: func_rparams COMMA expr
#line 736 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() =
                yystack_[2].value.as<std::vector<frontend::ExprNodePtr> *>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1665 "frontend/parser/bison.cpp"
          break;

          case 83:  // func_rparams: expr
#line 742 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>() = new std::vector<frontend::ExprNodePtr>();
            yylhs.value.as<std::vector<frontend::ExprNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1674 "frontend/parser/bison.cpp"
          break;

          case 84:  // decl: INT def_list SEMICOLON
#line 751 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::LocalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1683 "frontend/parser/bison.cpp"
          break;

          case 85:  // decl: FLOAT def_list SEMICOLON
#line 757 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::LocalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1692 "frontend/parser/bison.cpp"
          break;

          case 86:  // decl: CONST INT def_list SEMICOLON
#line 764 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::LocalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetIntegerType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()), true);
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1701 "frontend/parser/bison.cpp"
          break;

          case 87:  // decl: CONST FLOAT def_list SEMICOLON
#line 770 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::DeclNodePtr>() = std::make_unique<frontend::ast::LocalVarDeclNode>(
                yylhs.location, driver.GetContext().GetTypeContext()->GetFloatType(),
                std::move(*yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>()), true);
            delete yystack_[1].value.as<std::vector<frontend::VarDefNodePtr> *>();
          }
#line 1710 "frontend/parser/bison.cpp"
          break;

          case 88:  // initials: expr
#line 782 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::InitializerNodePtr>() = std::make_unique<frontend::ast::InitValNode>(
                yylhs.location, std::move(yystack_[0].value.as<frontend::ExprNodePtr>()));
          }
#line 1718 "frontend/parser/bison.cpp"
          break;

          case 89:  // initials: LCBRACE initials_list RCBRACE
#line 788 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::InitializerNodePtr>() = std::make_unique<frontend::ast::InitListNode>(
                yylhs.location, std::move(*yystack_[1].value.as<std::vector<frontend::InitializerNodePtr> *>()));
            delete yystack_[1].value.as<std::vector<frontend::InitializerNodePtr> *>();
          }
#line 1727 "frontend/parser/bison.cpp"
          break;

          case 90:  // initials: LCBRACE RCBRACE
#line 793 "frontend/parser/bison.y"
          {
            yylhs.value.as<frontend::InitializerNodePtr>() =
                std::make_unique<frontend::ast::InitListNode>(yylhs.location);
          }
#line 1735 "frontend/parser/bison.cpp"
          break;

          case 91:  // initials_list: initials_list COMMA initials
#line 801 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::InitializerNodePtr> *>() =
                yystack_[2].value.as<std::vector<frontend::InitializerNodePtr> *>();
            yylhs.value.as<std::vector<frontend::InitializerNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::InitializerNodePtr>()));
          }
#line 1744 "frontend/parser/bison.cpp"
          break;

          case 92:  // initials_list: initials
#line 808 "frontend/parser/bison.y"
          {
            yylhs.value.as<std::vector<frontend::InitializerNodePtr> *>() =
                new std::vector<frontend::InitializerNodePtr>();
            yylhs.value.as<std::vector<frontend::InitializerNodePtr> *>()->emplace_back(
                std::move(yystack_[0].value.as<frontend::InitializerNodePtr>()));
          }
#line 1753 "frontend/parser/bison.cpp"
          break;

#line 1757 "frontend/parser/bison.cpp"

          default:
            break;
        }
      }
#if YY_EXCEPTIONS
      catch (const syntax_error &yyexc) {
        YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
        error(yyexc);
        YYERROR;
      }
#endif  // YY_EXCEPTIONS
      YY_SYMBOL_PRINT("-> $$ =", yylhs);
      yypop_(yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_(YY_NULLPTR, YY_MOVE(yylhs));
    }
    goto yynewstate;

  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_) {
      ++yynerrs_;
      context yyctx(*this, yyla);
      std::string msg = yysyntax_error_(yyctx);
      error(yyla.location, YY_MOVE(msg));
    }

    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3) {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      // Return failure if at end of input.
      if (yyla.kind() == symbol_kind::S_YYEOF)
        YYABORT;
      else if (!yyla.empty()) {
        yy_destroy_("Error: discarding", yyla);
        yyla.clear();
      }
    }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;

  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false) YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_(yylen);
    yylen = 0;
    YY_STACK_PRINT();
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;  // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;) {
      yyn = yypact_[+yystack_[0].state];
      if (!yy_pact_value_is_default_(yyn)) {
        yyn += symbol_kind::S_YYerror;
        if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == symbol_kind::S_YYerror) {
          yyn = yytable_[yyn];
          if (0 < yyn) break;
        }
      }

      // Pop the current state because it cannot handle the error token.
      if (yystack_.size() == 1) YYABORT;

      yyerror_range[1].location = yystack_[0].location;
      yy_destroy_("Error: popping", yystack_[0]);
      yypop_();
      YY_STACK_PRINT();
    }
    {
      stack_symbol_type error_token;

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT(error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = state_type(yyn);
      yypush_("Shifting", YY_MOVE(error_token));
    }
    goto yynewstate;

  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty()) yy_destroy_("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_(yylen);
    YY_STACK_PRINT();
    while (1 < yystack_.size()) {
      yy_destroy_("Cleanup: popping", yystack_[0]);
      yypop_();
    }

    return yyresult;
  }
#if YY_EXCEPTIONS
  catch (...) {
    YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
    // Do not try to display the values of the reclaimed symbols,
    // as their printers might throw an exception.
    if (!yyla.empty()) yy_destroy_(YY_NULLPTR, yyla);

    while (1 < yystack_.size()) {
      yy_destroy_(YY_NULLPTR, yystack_[0]);
      yypop_();
    }
    throw;
  }
#endif  // YY_EXCEPTIONS
}

void parser::error(const syntax_error &yyexc) { error(yyexc.location, yyexc.what()); }

const char *parser::symbol_name(symbol_kind_type yysymbol) {
  static const char *const yy_sname[] = {"end of file",
                                         "error",
                                         "invalid token",
                                         "IF",
                                         "ELSE",
                                         "WHILE",
                                         "BREAK",
                                         "CONTINUE",
                                         "RETURN",
                                         "INT",
                                         "FLOAT",
                                         "VOID",
                                         "CONST",
                                         "END",
                                         "ADD",
                                         "SUB",
                                         "MUL",
                                         "DIV",
                                         "MOD",
                                         "ASSIGN",
                                         "UMINUS",
                                         "UPLUS",
                                         "THEN",
                                         "AND",
                                         "OR",
                                         "NOT",
                                         "LT",
                                         "GT",
                                         "LE",
                                         "GE",
                                         "EQ",
                                         "NEQ",
                                         "integer constant",
                                         "float constant",
                                         "identifier",
                                         "LCBRACE",
                                         "RCBRACE",
                                         "LSBRACE",
                                         "RSBRACE",
                                         "LPAREN",
                                         "RPAREN",
                                         "SEMICOLON",
                                         "COMMA",
                                         "$accept",
                                         "program",
                                         "compunit",
                                         "global_decl",
                                         "def_list",
                                         "def",
                                         "array_indices",
                                         "funcdef",
                                         "func_fparams",
                                         "func_fparam",
                                         "func_fparam_indices",
                                         "stmt_list",
                                         "stmt",
                                         "exprstmt",
                                         "assignstmt",
                                         "blockstmt",
                                         "breakstmt",
                                         "continuestmt",
                                         "returnstmt",
                                         "ifstmt",
                                         "whilestmt",
                                         "expr",
                                         "lval",
                                         "funccall",
                                         "func_rparams",
                                         "decl",
                                         "initials",
                                         "initials_list",
                                         YY_NULLPTR};
  return yy_sname[yysymbol];
}

// parser::context.
parser::context::context(const parser &yyparser, const symbol_type &yyla) : yyparser_(yyparser), yyla_(yyla) {}

int parser::context::expected_tokens(symbol_kind_type yyarg[], int yyargn) const {
  // Actual number of expected tokens
  int yycount = 0;

  const int yyn = yypact_[+yyparser_.yystack_[0].state];
  if (!yy_pact_value_is_default_(yyn)) {
    /* Start YYX at -YYN if negative to avoid negative indexes in
       YYCHECK.  In other words, skip the first -YYN actions for
       this state because they are default actions.  */
    const int yyxbegin = yyn < 0 ? -yyn : 0;
    // Stay within bounds of both yycheck and yytname.
    const int yychecklim = yylast_ - yyn + 1;
    const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
    for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
      if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror &&
          !yy_table_value_is_error_(yytable_[yyx + yyn])) {
        if (!yyarg)
          ++yycount;
        else if (yycount == yyargn)
          return 0;
        else
          yyarg[yycount++] = YY_CAST(symbol_kind_type, yyx);
      }
  }

  if (yyarg && yycount == 0 && 0 < yyargn) yyarg[0] = symbol_kind::S_YYEMPTY;
  return yycount;
}

int parser::yy_syntax_error_arguments_(const context &yyctx, symbol_kind_type yyarg[], int yyargn) const {
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yyla) is
       if this state is a consistent state with a default action.
       Thus, detecting the absence of a lookahead is sufficient to
       determine that there is no unexpected or expected token to
       report.  In that case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is
       a consistent state with a default action.  There might have
       been a previous inconsistent state, consistent state with a
       non-default action, or user semantic action that manipulated
       yyla.  (However, yyla is currently not documented for users.)
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */

  if (!yyctx.lookahead().empty()) {
    if (yyarg) yyarg[0] = yyctx.token();
    int yyn = yyctx.expected_tokens(yyarg ? yyarg + 1 : yyarg, yyargn - 1);
    return yyn + 1;
  }
  return 0;
}

// Generate an error message.
std::string parser::yysyntax_error_(const context &yyctx) const {
  // Its maximum.
  enum { YYARGS_MAX = 5 };
  // Arguments of yyformat.
  symbol_kind_type yyarg[YYARGS_MAX];
  int yycount = yy_syntax_error_arguments_(yyctx, yyarg, YYARGS_MAX);

  char const *yyformat = YY_NULLPTR;
  switch (yycount) {
#define YYCASE_(N, S) \
  case N:             \
    yyformat = S;     \
    break
    default:  // Avoid compiler warnings.
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
  }

  std::string yyres;
  // Argument number.
  std::ptrdiff_t yyi = 0;
  for (char const *yyp = yyformat; *yyp; ++yyp)
    if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount) {
      yyres += symbol_name(yyarg[yyi++]);
      ++yyp;
    } else
      yyres += *yyp;
  return yyres;
}

const signed char parser::yypact_ninf_ = -109;

const signed char parser::yytable_ninf_ = -1;

const short parser::yypact_[] = {
    73,   -27,  1,    6,    22,   24,   73,   -109, -109, -14,  77,   -109, -10,  109,  -19,  30,   30,   -109, -109,
    -109, -109, 343,  347,  2,    7,    -109, 30,   29,   -109, 40,   28,   111,  135,  347,  347,  347,  -109, -109,
    50,   331,  347,  427,  -109, -109, -109, 373,  39,   44,   -7,   37,   -109, 343,  347,  -109, -7,   48,   -7,
    57,   -109, -109, -109, -109, -109, 115,  59,   -109, -109, -6,   277,  347,  347,  347,  347,  347,  347,  347,
    347,  347,  347,  347,  347,  347,  -109, 105,  105,  102,  -109, -7,   169,  -109, 391,  -109, -7,   -109, -7,
    -109, 427,  64,   -109, 343,  -109, 54,   54,   -109, -109, -109, 463,  445,  108,  108,  108,  108,  157,  157,
    63,   107,  107,  100,  120,  125,  127,  61,   30,   30,   172,  -109, -109, 155,  -109, -109, -109, -109, -109,
    -109, -109, -109, -109, 220,  184,  -109, -109, -109, -109, -109, -109, -109, 347,  -109, -109, 347,  347,  347,
    -109, -109, -109, 239,  151,  167,  30,   30,   -109, -109, -109, 347,  427,  409,  295,  313,  -109, -109, -109,
    170,  173,  258,  -109, 192,  192,  -109, -109, -109, 142,  -109, 192,  -109};

const signed char parser::yydefact_[] = {
    0,  0,  0,  0,  0,  0,  2,  6,  7,  14, 0,  13, 14, 0,  0,  0,  0,  1,  3,  4,  5,  0,  0,  0,  16, 8,  0,
    0,  9,  0,  14, 0,  0,  0,  0,  0,  76, 77, 78, 0,  0,  88, 74, 75, 15, 0,  0,  0,  0,  0,  27, 0,  0,  12,
    0,  0,  0,  0,  10, 11, 63, 62, 64, 0,  79, 90, 92, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  19, 28, 29, 0,  21, 0,  0,  17, 0,  22, 0,  20, 0,  80, 83, 0,  89, 0,  73, 57, 58, 59, 60, 61, 65, 66,
    67, 68, 69, 70, 71, 72, 0,  30, 31, 0,  0,  0,  0,  0,  0,  0,  0,  49, 45, 0,  35, 36, 37, 38, 41, 42, 43,
    39, 40, 0,  74, 44, 24, 26, 18, 25, 23, 81, 0,  91, 33, 0,  0,  0,  50, 51, 52, 0,  0,  0,  0,  0,  48, 34,
    46, 0,  82, 0,  0,  0,  53, 84, 85, 0,  0,  0,  32, 0,  0,  86, 87, 47, 54, 56, 0,  55};

const short parser::yypgoto_[] = {-109, -109, -109, 199,  -2,   187,  178,  204, 86,  130,  136,  -109, -108, -109,
                                  -109, -46,  -109, -109, -109, -109, -109, -18, -84, -109, -109, -109, -33,  -109};

const unsigned char parser::yydefgoto_[] = {0,   5,   6,   7,   10,  11,  24,  8,   49, 50, 115, 127, 128, 129,
                                            130, 131, 132, 133, 134, 135, 136, 137, 42, 43, 97,  139, 44,  67};

const unsigned char parser::yytable_[] = {
    13,  138, 86,  41,  45,  21,  66,  9,   91,  21,  93,  46,  47,  31,  32,  60,  61,  62,  89,  161, 29,  41,  68,
    22,  17,  23,  51,  22,  85,  27,  98,  15,  16,  41,  90,  12,  99,  18,  46,  47,  14,  140, 48,  138, 52,  96,
    143, 21,  144, 46,  47,  101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 30,  22,  147, 180, 181,
    54,  71,  72,  73,  83,  183, 33,  34,  87,  84,  88,  56,  41,  1,   2,   3,   4,   35,  22,  92,  63,  88,  138,
    138, 36,  37,  38,  52,  94,  138, 88,  40,  148, 154, 155, 145, 117, 146, 118, 119, 120, 121, 122, 123, 55,  124,
    57,  33,  34,  25,  26,  156, 157, 69,  70,  71,  72,  73,  35,  164, 33,  34,  165, 166, 167, 36,  37,  38,  85,
    125, 150, 35,  40,  114, 126, 149, 173, 182, 36,  37,  38,  28,  26,  58,  26,  40,  95,  171, 172, 117, 151, 118,
    119, 120, 121, 122, 123, 152, 124, 153, 33,  34,  69,  70,  71,  72,  73,  59,  26,  46,  47,  35,  158, 159, 76,
    77,  78,  79,  36,  37,  38,  85,  160, 169, 26,  40,  117, 126, 118, 119, 120, 121, 122, 123, 163, 124, 19,  33,
    34,  170, 26,  20,  177, 26,  53,  178, 26,  64,  35,  141, 0,   116, 0,   0,   0,   36,  37,  38,  85,  0,   0,
    0,   40,  0,   126, 69,  70,  71,  72,  73,  0,   0,   0,   0,   74,  75,  0,   76,  77,  78,  79,  80,  81,  0,
    69,  70,  71,  72,  73,  0,   0,   0,   162, 74,  75,  0,   76,  77,  78,  79,  80,  81,  0,   69,  70,  71,  72,
    73,  0,   0,   0,   168, 74,  75,  0,   76,  77,  78,  79,  80,  81,  0,   69,  70,  71,  72,  73,  0,   0,   0,
    179, 74,  75,  0,   76,  77,  78,  79,  80,  81,  69,  70,  71,  72,  73,  0,   0,   0,   100, 74,  75,  0,   76,
    77,  78,  79,  80,  81,  69,  70,  71,  72,  73,  0,   0,   0,   175, 74,  75,  0,   76,  77,  78,  79,  80,  81,
    33,  34,  0,   0,   0,   0,   0,   0,   176, 0,   0,   35,  33,  34,  0,   0,   33,  34,  36,  37,  38,  39,  65,
    35,  0,   40,  0,   35,  0,   0,   36,  37,  38,  39,  36,  37,  38,  40,  0,   0,   0,   40,  69,  70,  71,  72,
    73,  0,   0,   0,   0,   74,  75,  0,   76,  77,  78,  79,  80,  81,  69,  70,  71,  72,  73,  0,   82,  0,   0,
    74,  75,  0,   76,  77,  78,  79,  80,  81,  69,  70,  71,  72,  73,  0,   142, 0,   0,   74,  75,  0,   76,  77,
    78,  79,  80,  81,  69,  70,  71,  72,  73,  0,   174, 0,   0,   74,  75,  0,   76,  77,  78,  79,  80,  81,  69,
    70,  71,  72,  73,  0,   0,   0,   0,   74,  0,   0,   76,  77,  78,  79,  80,  81,  69,  70,  71,  72,  73,  0,
    0,   0,   0,   0,   0,   0,   76,  77,  78,  79,  80,  81};

const short parser::yycheck_[] = {
    2,  85, 48, 21,  22, 19, 39,  34,  54,  19, 56, 9,  10, 15, 16, 33, 34,  35,  51,  127, 39,  39,  40, 37,  0,
    39, 19, 37, 35,  39, 36, 9,   10,  51,  52, 34, 42, 13, 9,  10, 34, 87,  40,  127, 37,  63,  92,  19, 94,  9,
    10, 69, 70, 71,  72, 73, 74,  75,  76,  77, 78, 79, 80, 81, 34, 37, 99,  175, 176, 40,  16,  17,  18, 34,  182,
    14, 15, 40, 34,  42, 40, 99,  9,   10,  11, 12, 25, 37, 40, 39, 42, 175, 176, 32,  33,  34,  37,  40, 182, 42,
    39, 38, 41, 121, 40, 3,  42,  5,   6,   7,  8,  9,  10, 27, 12, 29, 14,  15,  41,  42,  122, 123, 14, 15,  16,
    17, 18, 25, 146, 14, 15, 149, 150, 151, 32, 33, 34, 35, 36, 39, 25, 39,  37,  41,  37,  163, 4,   32, 33,  34,
    41, 42, 41, 42,  39, 40, 158, 159, 3,   39, 5,  6,  7,  8,  9,  10, 41,  12,  41,  14,  15,  14,  15, 16,  17,
    18, 41, 42, 9,   10, 25, 9,   10,  26,  27, 28, 29, 32, 33, 34, 35, 36,  41,  42,  39,  3,   41,  5,  6,   7,
    8,  9,  10, 19,  12, 6,  14,  15,  41,  42, 6,  41, 42, 26, 41, 42, 38,  25,  88,  -1,  84,  -1,  -1, -1,  32,
    33, 34, 35, -1,  -1, -1, 39,  -1,  41,  14, 15, 16, 17, 18, -1, -1, -1,  -1,  23,  24,  -1,  26,  27, 28,  29,
    30, 31, -1, 14,  15, 16, 17,  18,  -1,  -1, -1, 41, 23, 24, -1, 26, 27,  28,  29,  30,  31,  -1,  14, 15,  16,
    17, 18, -1, -1,  -1, 41, 23,  24,  -1,  26, 27, 28, 29, 30, 31, -1, 14,  15,  16,  17,  18,  -1,  -1, -1,  41,
    23, 24, -1, 26,  27, 28, 29,  30,  31,  14, 15, 16, 17, 18, -1, -1, -1,  40,  23,  24,  -1,  26,  27, 28,  29,
    30, 31, 14, 15,  16, 17, 18,  -1,  -1,  -1, 40, 23, 24, -1, 26, 27, 28,  29,  30,  31,  14,  15,  -1, -1,  -1,
    -1, -1, -1, 40,  -1, -1, 25,  14,  15,  -1, -1, 14, 15, 32, 33, 34, 35,  36,  25,  -1,  39,  -1,  25, -1,  -1,
    32, 33, 34, 35,  32, 33, 34,  39,  -1,  -1, -1, 39, 14, 15, 16, 17, 18,  -1,  -1,  -1,  -1,  23,  24, -1,  26,
    27, 28, 29, 30,  31, 14, 15,  16,  17,  18, -1, 38, -1, -1, 23, 24, -1,  26,  27,  28,  29,  30,  31, 14,  15,
    16, 17, 18, -1,  38, -1, -1,  23,  24,  -1, 26, 27, 28, 29, 30, 31, 14,  15,  16,  17,  18,  -1,  38, -1,  -1,
    23, 24, -1, 26,  27, 28, 29,  30,  31,  14, 15, 16, 17, 18, -1, -1, -1,  -1,  23,  -1,  -1,  26,  27, 28,  29,
    30, 31, 14, 15,  16, 17, 18,  -1,  -1,  -1, -1, -1, -1, -1, 26, 27, 28,  29,  30,  31};

const signed char parser::yystos_[] = {
    0,  9,  10, 11, 12, 44, 45, 46, 50, 34, 47, 48, 34, 47, 34, 9,  10, 0,  13, 46, 50, 19, 37, 39, 49, 41, 42,
    39, 41, 39, 34, 47, 47, 14, 15, 25, 32, 33, 34, 35, 39, 64, 65, 66, 69, 64, 9,  10, 40, 51, 52, 19, 37, 48,
    40, 51, 40, 51, 41, 41, 64, 64, 64, 39, 49, 36, 69, 70, 64, 14, 15, 16, 17, 18, 23, 24, 26, 27, 28, 29, 30,
    31, 38, 34, 34, 35, 58, 40, 42, 69, 64, 58, 40, 58, 40, 40, 64, 67, 36, 42, 40, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 37, 53, 53, 3,  5,  6,  7,  8,  9,  10, 12, 36, 41, 54, 55, 56, 57, 58, 59, 60, 61,
    62, 63, 64, 65, 68, 58, 52, 38, 58, 58, 40, 42, 69, 38, 37, 39, 39, 41, 41, 41, 64, 47, 47, 9,  10, 36, 55,
    41, 19, 64, 64, 64, 64, 41, 41, 41, 47, 47, 64, 38, 40, 40, 41, 41, 41, 55, 55, 4,  55};

const signed char parser::yyr1_[] = {0,  43, 44, 44, 45, 45, 45, 45, 46, 46, 46, 46, 47, 47, 48, 48, 48, 48, 49,
                                     49, 50, 50, 50, 50, 50, 50, 51, 51, 52, 52, 52, 52, 53, 53, 54, 54, 55, 55,
                                     55, 55, 55, 55, 55, 55, 55, 56, 56, 57, 58, 58, 59, 60, 61, 61, 62, 62, 63,
                                     64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                                     64, 64, 65, 65, 66, 66, 67, 67, 68, 68, 68, 68, 69, 69, 69, 70, 70};

const signed char parser::yyr2_[] = {0, 2, 1, 2, 2, 2, 1, 1, 3, 3, 4, 4, 3, 1, 1, 3, 2, 4, 4, 3, 5, 5, 5, 6,
                                     6, 6, 3, 1, 2, 2, 3, 3, 4, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 4,
                                     3, 2, 2, 2, 2, 3, 5, 7, 5, 3, 3, 3, 3, 3, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3,
                                     3, 3, 1, 1, 1, 1, 1, 2, 3, 4, 3, 1, 3, 3, 4, 4, 1, 3, 2, 3, 1};

#if YYDEBUG
const short parser::yyrline_[] = {0,   187, 187, 193, 201, 207, 213, 219, 229, 235, 242, 248, 259, 266, 275, 280,
                                  286, 292, 301, 309, 319, 324, 329, 335, 341, 347, 357, 364, 373, 378, 385, 391,
                                  400, 407, 416, 422, 432, 439, 445, 451, 457, 463, 469, 475, 482, 490, 495, 503,
                                  511, 517, 524, 531, 538, 543, 550, 555, 562, 571, 576, 581, 586, 591, 596, 601,
                                  607, 612, 617, 623, 628, 633, 638, 643, 648, 654, 660, 666, 672, 678, 686, 692,
                                  701, 725, 735, 741, 750, 756, 763, 769, 781, 787, 792, 800, 807};

void parser::yy_stack_print_() const {
  *yycdebug_ << "Stack now";
  for (stack_type::const_iterator i = yystack_.begin(), i_end = yystack_.end(); i != i_end; ++i)
    *yycdebug_ << ' ' << int(i->state);
  *yycdebug_ << '\n';
}

void parser::yy_reduce_print_(int yyrule) const {
  int yylno = yyrline_[yyrule];
  int yynrhs = yyr2_[yyrule];
  // Print the symbols being reduced, and their result.
  *yycdebug_ << "Reducing stack by rule " << yyrule - 1 << " (line " << yylno << "):\n";
  // The symbols being reduced.
  for (int yyi = 0; yyi < yynrhs; yyi++) YY_SYMBOL_PRINT("   $" << yyi + 1 << " =", yystack_[(yynrhs) - (yyi + 1)]);
}
#endif  // YYDEBUG

}  // namespace yy
#line 2363 "frontend/parser/bison.cpp"

#line 813 "frontend/parser/bison.y"
