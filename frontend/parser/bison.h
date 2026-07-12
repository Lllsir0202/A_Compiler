// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton interface for Bison LALR(1) parsers in C++

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

/**
 ** \file frontend/parser/bison.h
 ** Define the yy::parser class.
 */

// C++ LALR(1) parser skeleton written by Akim Demaille.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

#ifndef YY_YY_FRONTEND_PARSER_BISON_H_INCLUDED
#define YY_YY_FRONTEND_PARSER_BISON_H_INCLUDED
// "%code requires" blocks.
#line 16 "frontend/parser/bison.y"

#include <memory>
#include <string>
#include "frontend/ast/ast_base.h"
#include "frontend/ast/ast_decl.h"
#include "frontend/ast/ast_expr.h"
#include "frontend/ast/ast_spec.h"
#include "frontend/ast/ast_stmt.h"
#include "frontend/parser/bison_parser_impl.h"
#include "frontend/parser/parser_utils.h"
#include "frontend/token/token.h"

namespace frontend {
using ASTNodePtr = std::unique_ptr<ast::ASTNode>;
using ExprNodePtr = std::unique_ptr<ast::ExprNode>;
using StmtNodePtr = std::unique_ptr<ast::StmtNode>;
using VarDefNodePtr = std::unique_ptr<ast::VarDefNode>;
using DeclNodePtr = std::unique_ptr<ast::DeclNode>;
using FuncFParamNodePtr = std::unique_ptr<ast::FuncFParamNode>;
using InitializerNodePtr = std::unique_ptr<ast::Initializer>;
}  // namespace frontend

#line 72 "frontend/parser/bison.h"

#include <cstdlib>  // std::abort
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined __cplusplus
#define YY_CPLUSPLUS __cplusplus
#else
#define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
#define YY_MOVE std::move
#define YY_MOVE_OR_COPY move
#define YY_MOVE_REF(Type) Type &&
#define YY_RVREF(Type) Type &&
#define YY_COPY(Type) Type
#else
#define YY_MOVE
#define YY_MOVE_OR_COPY copy
#define YY_MOVE_REF(Type) Type &
#define YY_RVREF(Type) const Type &
#define YY_COPY(Type) const Type &
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
#define YY_NOEXCEPT noexcept
#define YY_NOTHROW
#else
#define YY_NOEXCEPT
#define YY_NOTHROW throw()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
#define YY_CONSTEXPR constexpr
#else
#define YY_CONSTEXPR
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__
#define YY_USE(E) ((void)(E))
#else
#define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && !defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
#if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#else
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                            \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"") \
      _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#endif
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void *)0)
#endif
#endif

/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif

namespace yy {
#line 207 "frontend/parser/bison.h"

/// A Bison parser.
class parser {
 public:
#ifdef YYSTYPE
#ifdef __GNUC__
#pragma GCC message "bison: do not #define YYSTYPE in C++, use %define api.value.type"
#endif
  typedef YYSTYPE value_type;
#else
  /// A buffer to store and retrieve objects.
  ///
  /// Sort of a variant, but does not keep track of the nature
  /// of the stored data, since that knowledge is available
  /// via the current parser state.
  class value_type {
   public:
    /// Type of *this.
    typedef value_type self_type;

    /// Empty construction.
    value_type() YY_NOEXCEPT : yyraw_() {}

    /// Construct and fill.
    template <typename T>
    value_type(YY_RVREF(T) t) {
      new (yyas_<T>()) T(YY_MOVE(t));
    }

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    value_type(const self_type &) = delete;
    /// Non copyable.
    self_type &operator=(const self_type &) = delete;
#endif

    /// Destruction, allowed only if empty.
    ~value_type() YY_NOEXCEPT {}

#if 201103L <= YY_CPLUSPLUS
    /// Instantiate a \a T in here from \a t.
    template <typename T, typename... U>
    T &emplace(U &&...u) {
      return *new (yyas_<T>()) T(std::forward<U>(u)...);
    }
#else
    /// Instantiate an empty \a T in here.
    template <typename T>
    T &emplace() {
      return *new (yyas_<T>()) T();
    }

    /// Instantiate a \a T in here from \a t.
    template <typename T>
    T &emplace(const T &t) {
      return *new (yyas_<T>()) T(t);
    }
#endif

    /// Instantiate an empty \a T in here.
    /// Obsolete, use emplace.
    template <typename T>
    T &build() {
      return emplace<T>();
    }

    /// Instantiate a \a T in here from \a t.
    /// Obsolete, use emplace.
    template <typename T>
    T &build(const T &t) {
      return emplace<T>(t);
    }

    /// Accessor to a built \a T.
    template <typename T>
    T &as() YY_NOEXCEPT {
      return *yyas_<T>();
    }

    /// Const accessor to a built \a T (for %printer).
    template <typename T>
    const T &as() const YY_NOEXCEPT {
      return *yyas_<T>();
    }

    /// Swap the content with \a that, of same type.
    ///
    /// Both variants must be built beforehand, because swapping the actual
    /// data requires reading it (with as()), and this is not possible on
    /// unconstructed variants: it would require some dynamic testing, which
    /// should not be the variant's responsibility.
    /// Swapping between built and (possibly) non-built is done with
    /// self_type::move ().
    template <typename T>
    void swap(self_type &that) YY_NOEXCEPT {
      std::swap(as<T>(), that.as<T>());
    }

    /// Move the content of \a that to this.
    ///
    /// Destroys \a that.
    template <typename T>
    void move(self_type &that) {
#if 201103L <= YY_CPLUSPLUS
      emplace<T>(std::move(that.as<T>()));
#else
      emplace<T>();
      swap<T>(that);
#endif
      that.destroy<T>();
    }

#if 201103L <= YY_CPLUSPLUS
    /// Move the content of \a that to this.
    template <typename T>
    void move(self_type &&that) {
      emplace<T>(std::move(that.as<T>()));
      that.destroy<T>();
    }
#endif

    /// Copy the content of \a that to this.
    template <typename T>
    void copy(const self_type &that) {
      emplace<T>(that.as<T>());
    }

    /// Destroy the stored \a T.
    template <typename T>
    void destroy() {
      as<T>().~T();
    }

   private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    value_type(const self_type &);
    /// Non copyable.
    self_type &operator=(const self_type &);
#endif

    /// Accessor to raw memory as \a T.
    template <typename T>
    T *yyas_() YY_NOEXCEPT {
      void *yyp = yyraw_;
      return static_cast<T *>(yyp);
    }

    /// Const accessor to raw memory as \a T.
    template <typename T>
    const T *yyas_() const YY_NOEXCEPT {
      const void *yyp = yyraw_;
      return static_cast<const T *>(yyp);
    }

    /// An auxiliary type to compute the largest semantic type.
    union union_type {
      // "float constant"
      char dummy1[sizeof(float)];

      // program
      // funcdef
      char dummy2[sizeof(frontend::ASTNodePtr)];

      // global_decl
      // decl
      char dummy3[sizeof(frontend::DeclNodePtr)];

      // expr
      // lval
      // funccall
      char dummy4[sizeof(frontend::ExprNodePtr)];

      // func_fparam
      char dummy5[sizeof(frontend::FuncFParamNodePtr)];

      // initials
      char dummy6[sizeof(frontend::InitializerNodePtr)];

      // stmt
      // exprstmt
      // assignstmt
      // blockstmt
      // breakstmt
      // continuestmt
      // returnstmt
      // ifstmt
      // whilestmt
      char dummy7[sizeof(frontend::StmtNodePtr)];

      // def
      char dummy8[sizeof(frontend::VarDefNodePtr)];

      // "integer constant"
      char dummy9[sizeof(int)];

      // "identifier"
      char dummy10[sizeof(std::string)];

      // compunit
      char dummy11[sizeof(std::vector<frontend::ASTNodePtr> *)];

      // array_indices
      // func_fparam_indices
      // func_rparams
      char dummy12[sizeof(std::vector<frontend::ExprNodePtr> *)];

      // func_fparams
      char dummy13[sizeof(std::vector<frontend::FuncFParamNodePtr> *)];

      // initials_list
      char dummy14[sizeof(std::vector<frontend::InitializerNodePtr> *)];

      // stmt_list
      char dummy15[sizeof(std::vector<frontend::StmtNodePtr> *)];

      // def_list
      char dummy16[sizeof(std::vector<frontend::VarDefNodePtr> *)];
    };

    /// The size of the largest semantic type.
    enum { size = sizeof(union_type) };

    /// A buffer to store semantic values.
    union {
      /// Strongest alignment constraints.
      long double yyalign_me_;
      /// A buffer large enough to store any of the semantic values.
      char yyraw_[size];
    };
  };

#endif
  /// Backward compatibility (Bison 3.8).
  typedef value_type semantic_type;

  /// Symbol locations.
  typedef frontend::SourceRange location_type;

  /// Syntax errors thrown from user actions.
  struct syntax_error : std::runtime_error {
    syntax_error(const location_type &l, const std::string &m) : std::runtime_error(m), location(l) {}

    syntax_error(const syntax_error &s) : std::runtime_error(s.what()), location(s.location) {}

    ~syntax_error() YY_NOEXCEPT YY_NOTHROW;

    location_type location;
  };

  /// Token kinds.
  struct token {
    enum token_kind_type {
      YYEMPTY = -2,
      YYEOF = 0,         // "end of file"
      YYerror = 256,     // error
      YYUNDEF = 257,     // "invalid token"
      IF = 258,          // IF
      ELSE = 259,        // ELSE
      WHILE = 260,       // WHILE
      BREAK = 261,       // BREAK
      CONTINUE = 262,    // CONTINUE
      RETURN = 263,      // RETURN
      INT = 264,         // INT
      FLOAT = 265,       // FLOAT
      VOID = 266,        // VOID
      CONST = 267,       // CONST
      END = 268,         // END
      ADD = 269,         // ADD
      SUB = 270,         // SUB
      MUL = 271,         // MUL
      DIV = 272,         // DIV
      MOD = 273,         // MOD
      ASSIGN = 274,      // ASSIGN
      UMINUS = 275,      // UMINUS
      UPLUS = 276,       // UPLUS
      THEN = 277,        // THEN
      AND = 278,         // AND
      OR = 279,          // OR
      NOT = 280,         // NOT
      LT = 281,          // LT
      GT = 282,          // GT
      LE = 283,          // LE
      GE = 284,          // GE
      EQ = 285,          // EQ
      NEQ = 286,         // NEQ
      INTCONST = 287,    // "integer constant"
      FLOATCONST = 288,  // "float constant"
      IDENTIFIER = 289,  // "identifier"
      LCBRACE = 290,     // LCBRACE
      RCBRACE = 291,     // RCBRACE
      LSBRACE = 292,     // LSBRACE
      RSBRACE = 293,     // RSBRACE
      LPAREN = 294,      // LPAREN
      RPAREN = 295,      // RPAREN
      SEMICOLON = 296,   // SEMICOLON
      COMMA = 297        // COMMA
    };
    /// Backward compatibility alias (Bison 3.6).
    typedef token_kind_type yytokentype;
  };

  /// Token kind, as returned by yylex.
  typedef token::token_kind_type token_kind_type;

  /// Backward compatibility alias (Bison 3.6).
  typedef token_kind_type token_type;

  /// Symbol kinds.
  struct symbol_kind {
    enum symbol_kind_type {
      YYNTOKENS = 43,  ///< Number of tokens.
      S_YYEMPTY = -2,
      S_YYEOF = 0,                 // "end of file"
      S_YYerror = 1,               // error
      S_YYUNDEF = 2,               // "invalid token"
      S_IF = 3,                    // IF
      S_ELSE = 4,                  // ELSE
      S_WHILE = 5,                 // WHILE
      S_BREAK = 6,                 // BREAK
      S_CONTINUE = 7,              // CONTINUE
      S_RETURN = 8,                // RETURN
      S_INT = 9,                   // INT
      S_FLOAT = 10,                // FLOAT
      S_VOID = 11,                 // VOID
      S_CONST = 12,                // CONST
      S_END = 13,                  // END
      S_ADD = 14,                  // ADD
      S_SUB = 15,                  // SUB
      S_MUL = 16,                  // MUL
      S_DIV = 17,                  // DIV
      S_MOD = 18,                  // MOD
      S_ASSIGN = 19,               // ASSIGN
      S_UMINUS = 20,               // UMINUS
      S_UPLUS = 21,                // UPLUS
      S_THEN = 22,                 // THEN
      S_AND = 23,                  // AND
      S_OR = 24,                   // OR
      S_NOT = 25,                  // NOT
      S_LT = 26,                   // LT
      S_GT = 27,                   // GT
      S_LE = 28,                   // LE
      S_GE = 29,                   // GE
      S_EQ = 30,                   // EQ
      S_NEQ = 31,                  // NEQ
      S_INTCONST = 32,             // "integer constant"
      S_FLOATCONST = 33,           // "float constant"
      S_IDENTIFIER = 34,           // "identifier"
      S_LCBRACE = 35,              // LCBRACE
      S_RCBRACE = 36,              // RCBRACE
      S_LSBRACE = 37,              // LSBRACE
      S_RSBRACE = 38,              // RSBRACE
      S_LPAREN = 39,               // LPAREN
      S_RPAREN = 40,               // RPAREN
      S_SEMICOLON = 41,            // SEMICOLON
      S_COMMA = 42,                // COMMA
      S_YYACCEPT = 43,             // $accept
      S_program = 44,              // program
      S_compunit = 45,             // compunit
      S_global_decl = 46,          // global_decl
      S_def_list = 47,             // def_list
      S_def = 48,                  // def
      S_array_indices = 49,        // array_indices
      S_funcdef = 50,              // funcdef
      S_func_fparams = 51,         // func_fparams
      S_func_fparam = 52,          // func_fparam
      S_func_fparam_indices = 53,  // func_fparam_indices
      S_stmt_list = 54,            // stmt_list
      S_stmt = 55,                 // stmt
      S_exprstmt = 56,             // exprstmt
      S_assignstmt = 57,           // assignstmt
      S_blockstmt = 58,            // blockstmt
      S_breakstmt = 59,            // breakstmt
      S_continuestmt = 60,         // continuestmt
      S_returnstmt = 61,           // returnstmt
      S_ifstmt = 62,               // ifstmt
      S_whilestmt = 63,            // whilestmt
      S_expr = 64,                 // expr
      S_lval = 65,                 // lval
      S_funccall = 66,             // funccall
      S_func_rparams = 67,         // func_rparams
      S_decl = 68,                 // decl
      S_initials = 69,             // initials
      S_initials_list = 70         // initials_list
    };
  };

  /// (Internal) symbol kind.
  typedef symbol_kind::symbol_kind_type symbol_kind_type;

  /// The number of tokens.
  static const symbol_kind_type YYNTOKENS = symbol_kind::YYNTOKENS;

  /// A complete symbol.
  ///
  /// Expects its Base type to provide access to the symbol kind
  /// via kind ().
  ///
  /// Provide access to semantic value and location.
  template <typename Base>
  struct basic_symbol : Base {
    /// Alias to Base.
    typedef Base super_type;

    /// Default constructor.
    basic_symbol() YY_NOEXCEPT : value(), location() {}

#if 201103L <= YY_CPLUSPLUS
    /// Move constructor.
    basic_symbol(basic_symbol &&that) : Base(std::move(that)), value(), location(std::move(that.location)) {
      switch (this->kind()) {
        case symbol_kind::S_FLOATCONST:  // "float constant"
          value.move<float>(std::move(that.value));
          break;

        case symbol_kind::S_program:  // program
        case symbol_kind::S_funcdef:  // funcdef
          value.move<frontend::ASTNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_global_decl:  // global_decl
        case symbol_kind::S_decl:         // decl
          value.move<frontend::DeclNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_expr:      // expr
        case symbol_kind::S_lval:      // lval
        case symbol_kind::S_funccall:  // funccall
          value.move<frontend::ExprNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_func_fparam:  // func_fparam
          value.move<frontend::FuncFParamNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_initials:  // initials
          value.move<frontend::InitializerNodePtr>(std::move(that.value));
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
          value.move<frontend::StmtNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_def:  // def
          value.move<frontend::VarDefNodePtr>(std::move(that.value));
          break;

        case symbol_kind::S_INTCONST:  // "integer constant"
          value.move<int>(std::move(that.value));
          break;

        case symbol_kind::S_IDENTIFIER:  // "identifier"
          value.move<std::string>(std::move(that.value));
          break;

        case symbol_kind::S_compunit:  // compunit
          value.move<std::vector<frontend::ASTNodePtr> *>(std::move(that.value));
          break;

        case symbol_kind::S_array_indices:        // array_indices
        case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
        case symbol_kind::S_func_rparams:         // func_rparams
          value.move<std::vector<frontend::ExprNodePtr> *>(std::move(that.value));
          break;

        case symbol_kind::S_func_fparams:  // func_fparams
          value.move<std::vector<frontend::FuncFParamNodePtr> *>(std::move(that.value));
          break;

        case symbol_kind::S_initials_list:  // initials_list
          value.move<std::vector<frontend::InitializerNodePtr> *>(std::move(that.value));
          break;

        case symbol_kind::S_stmt_list:  // stmt_list
          value.move<std::vector<frontend::StmtNodePtr> *>(std::move(that.value));
          break;

        case symbol_kind::S_def_list:  // def_list
          value.move<std::vector<frontend::VarDefNodePtr> *>(std::move(that.value));
          break;

        default:
          break;
      }
    }
#endif

    /// Copy constructor.
    basic_symbol(const basic_symbol &that);

    /// Constructors for typed symbols.
#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, location_type &&l) : Base(t), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const location_type &l) : Base(t), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, float &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const float &v, const location_type &l) : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::ASTNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::ASTNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::DeclNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::DeclNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::ExprNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::ExprNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::FuncFParamNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::FuncFParamNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::InitializerNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::InitializerNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::StmtNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::StmtNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, frontend::VarDefNodePtr &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const frontend::VarDefNodePtr &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, int &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const int &v, const location_type &l) : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::string &&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::string &v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::ASTNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::ASTNodePtr> *&v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::ExprNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::ExprNodePtr> *&v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::FuncFParamNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::FuncFParamNodePtr> *&v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::InitializerNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::InitializerNodePtr> *&v,
                 const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::StmtNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::StmtNodePtr> *&v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

#if 201103L <= YY_CPLUSPLUS
    basic_symbol(typename Base::kind_type t, std::vector<frontend::VarDefNodePtr> *&&v, location_type &&l)
        : Base(t), value(std::move(v)), location(std::move(l)) {}
#else
    basic_symbol(typename Base::kind_type t, const std::vector<frontend::VarDefNodePtr> *&v, const location_type &l)
        : Base(t), value(v), location(l) {}
#endif

    /// Destroy the symbol.
    ~basic_symbol() { clear(); }

    /// Destroy contents, and record that is empty.
    void clear() YY_NOEXCEPT {
      // User destructor.
      symbol_kind_type yykind = this->kind();
      basic_symbol<Base> &yysym = *this;
      (void)yysym;
      switch (yykind) {
        default:
          break;
      }

      // Value type destructor.
      switch (yykind) {
        case symbol_kind::S_FLOATCONST:  // "float constant"
          value.template destroy<float>();
          break;

        case symbol_kind::S_program:  // program
        case symbol_kind::S_funcdef:  // funcdef
          value.template destroy<frontend::ASTNodePtr>();
          break;

        case symbol_kind::S_global_decl:  // global_decl
        case symbol_kind::S_decl:         // decl
          value.template destroy<frontend::DeclNodePtr>();
          break;

        case symbol_kind::S_expr:      // expr
        case symbol_kind::S_lval:      // lval
        case symbol_kind::S_funccall:  // funccall
          value.template destroy<frontend::ExprNodePtr>();
          break;

        case symbol_kind::S_func_fparam:  // func_fparam
          value.template destroy<frontend::FuncFParamNodePtr>();
          break;

        case symbol_kind::S_initials:  // initials
          value.template destroy<frontend::InitializerNodePtr>();
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
          value.template destroy<frontend::StmtNodePtr>();
          break;

        case symbol_kind::S_def:  // def
          value.template destroy<frontend::VarDefNodePtr>();
          break;

        case symbol_kind::S_INTCONST:  // "integer constant"
          value.template destroy<int>();
          break;

        case symbol_kind::S_IDENTIFIER:  // "identifier"
          value.template destroy<std::string>();
          break;

        case symbol_kind::S_compunit:  // compunit
          value.template destroy<std::vector<frontend::ASTNodePtr> *>();
          break;

        case symbol_kind::S_array_indices:        // array_indices
        case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
        case symbol_kind::S_func_rparams:         // func_rparams
          value.template destroy<std::vector<frontend::ExprNodePtr> *>();
          break;

        case symbol_kind::S_func_fparams:  // func_fparams
          value.template destroy<std::vector<frontend::FuncFParamNodePtr> *>();
          break;

        case symbol_kind::S_initials_list:  // initials_list
          value.template destroy<std::vector<frontend::InitializerNodePtr> *>();
          break;

        case symbol_kind::S_stmt_list:  // stmt_list
          value.template destroy<std::vector<frontend::StmtNodePtr> *>();
          break;

        case symbol_kind::S_def_list:  // def_list
          value.template destroy<std::vector<frontend::VarDefNodePtr> *>();
          break;

        default:
          break;
      }

      Base::clear();
    }

    /// The user-facing name of this symbol.
    const char *name() const YY_NOEXCEPT { return parser::symbol_name(this->kind()); }

    /// Backward compatibility (Bison 3.6).
    symbol_kind_type type_get() const YY_NOEXCEPT;

    /// Whether empty.
    bool empty() const YY_NOEXCEPT;

    /// Destructive move, \a s is emptied into this.
    void move(basic_symbol &s);

    /// The semantic value.
    value_type value;

    /// The location.
    location_type location;

   private:
#if YY_CPLUSPLUS < 201103L
    /// Assignment operator.
    basic_symbol &operator=(const basic_symbol &that);
#endif
  };

  /// Type access provider for token (enum) based symbols.
  struct by_kind {
    /// The symbol kind as needed by the constructor.
    typedef token_kind_type kind_type;

    /// Default constructor.
    by_kind() YY_NOEXCEPT;

#if 201103L <= YY_CPLUSPLUS
    /// Move constructor.
    by_kind(by_kind &&that) YY_NOEXCEPT;
#endif

    /// Copy constructor.
    by_kind(const by_kind &that) YY_NOEXCEPT;

    /// Constructor from (external) token numbers.
    by_kind(kind_type t) YY_NOEXCEPT;

    /// Record that this symbol is empty.
    void clear() YY_NOEXCEPT;

    /// Steal the symbol kind from \a that.
    void move(by_kind &that);

    /// The (internal) type number (corresponding to \a type).
    /// \a empty when empty.
    symbol_kind_type kind() const YY_NOEXCEPT;

    /// Backward compatibility (Bison 3.6).
    symbol_kind_type type_get() const YY_NOEXCEPT;

    /// The symbol kind.
    /// \a S_YYEMPTY when empty.
    symbol_kind_type kind_;
  };

  /// Backward compatibility for a private implementation detail (Bison 3.6).
  typedef by_kind by_type;

  /// "External" symbols: returned by the scanner.
  struct symbol_type : basic_symbol<by_kind> {
    /// Superclass.
    typedef basic_symbol<by_kind> super_type;

    /// Empty symbol.
    symbol_type() YY_NOEXCEPT {}

    /// Constructor for valueless symbols, and symbols from each type.
#if 201103L <= YY_CPLUSPLUS
    symbol_type(int tok, location_type l)
        : super_type(token_kind_type(tok), std::move(l))
#else
    symbol_type(int tok, const location_type &l)
        : super_type(token_kind_type(tok), l)
#endif
    {
    }
#if 201103L <= YY_CPLUSPLUS
    symbol_type(int tok, float v, location_type l)
        : super_type(token_kind_type(tok), std::move(v), std::move(l))
#else
    symbol_type(int tok, const float &v, const location_type &l)
        : super_type(token_kind_type(tok), v, l)
#endif
    {
    }
#if 201103L <= YY_CPLUSPLUS
    symbol_type(int tok, int v, location_type l)
        : super_type(token_kind_type(tok), std::move(v), std::move(l))
#else
    symbol_type(int tok, const int &v, const location_type &l)
        : super_type(token_kind_type(tok), v, l)
#endif
    {
    }
#if 201103L <= YY_CPLUSPLUS
    symbol_type(int tok, std::string v, location_type l)
        : super_type(token_kind_type(tok), std::move(v), std::move(l))
#else
    symbol_type(int tok, const std::string &v, const location_type &l)
        : super_type(token_kind_type(tok), v, l)
#endif
    {
    }
  };

  /// Build a parser object.
  parser(frontend::BisonParserImpl &driver_yyarg);
  virtual ~parser();

#if 201103L <= YY_CPLUSPLUS
  /// Non copyable.
  parser(const parser &) = delete;
  /// Non copyable.
  parser &operator=(const parser &) = delete;
#endif

  /// Parse.  An alias for parse ().
  /// \returns  0 iff parsing succeeded.
  int operator()();

  /// Parse.
  /// \returns  0 iff parsing succeeded.
  virtual int parse();

#if YYDEBUG
  /// The current debugging stream.
  std::ostream &debug_stream() const YY_ATTRIBUTE_PURE;
  /// Set the current debugging stream.
  void set_debug_stream(std::ostream &);

  /// Type for debugging levels.
  typedef int debug_level_type;
  /// The current debugging level.
  debug_level_type debug_level() const YY_ATTRIBUTE_PURE;
  /// Set the current debugging level.
  void set_debug_level(debug_level_type l);
#endif

  /// Report a syntax error.
  /// \param loc    where the syntax error is found.
  /// \param msg    a description of the syntax error.
  virtual void error(const location_type &loc, const std::string &msg);

  /// Report a syntax error.
  void error(const syntax_error &err);

  /// The user-facing name of the symbol whose (internal) number is
  /// YYSYMBOL.  No bounds checking.
  static const char *symbol_name(symbol_kind_type yysymbol);

  // Implementation of make_symbol for each token kind.
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_YYEOF(location_type l) { return symbol_type(token::YYEOF, std::move(l)); }
#else
  static symbol_type make_YYEOF(const location_type &l) { return symbol_type(token::YYEOF, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_YYerror(location_type l) { return symbol_type(token::YYerror, std::move(l)); }
#else
  static symbol_type make_YYerror(const location_type &l) { return symbol_type(token::YYerror, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_YYUNDEF(location_type l) { return symbol_type(token::YYUNDEF, std::move(l)); }
#else
  static symbol_type make_YYUNDEF(const location_type &l) { return symbol_type(token::YYUNDEF, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_IF(location_type l) { return symbol_type(token::IF, std::move(l)); }
#else
  static symbol_type make_IF(const location_type &l) { return symbol_type(token::IF, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_ELSE(location_type l) { return symbol_type(token::ELSE, std::move(l)); }
#else
  static symbol_type make_ELSE(const location_type &l) { return symbol_type(token::ELSE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_WHILE(location_type l) { return symbol_type(token::WHILE, std::move(l)); }
#else
  static symbol_type make_WHILE(const location_type &l) { return symbol_type(token::WHILE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_BREAK(location_type l) { return symbol_type(token::BREAK, std::move(l)); }
#else
  static symbol_type make_BREAK(const location_type &l) { return symbol_type(token::BREAK, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_CONTINUE(location_type l) { return symbol_type(token::CONTINUE, std::move(l)); }
#else
  static symbol_type make_CONTINUE(const location_type &l) { return symbol_type(token::CONTINUE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_RETURN(location_type l) { return symbol_type(token::RETURN, std::move(l)); }
#else
  static symbol_type make_RETURN(const location_type &l) { return symbol_type(token::RETURN, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_INT(location_type l) { return symbol_type(token::INT, std::move(l)); }
#else
  static symbol_type make_INT(const location_type &l) { return symbol_type(token::INT, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_FLOAT(location_type l) { return symbol_type(token::FLOAT, std::move(l)); }
#else
  static symbol_type make_FLOAT(const location_type &l) { return symbol_type(token::FLOAT, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_VOID(location_type l) { return symbol_type(token::VOID, std::move(l)); }
#else
  static symbol_type make_VOID(const location_type &l) { return symbol_type(token::VOID, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_CONST(location_type l) { return symbol_type(token::CONST, std::move(l)); }
#else
  static symbol_type make_CONST(const location_type &l) { return symbol_type(token::CONST, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_END(location_type l) { return symbol_type(token::END, std::move(l)); }
#else
  static symbol_type make_END(const location_type &l) { return symbol_type(token::END, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_ADD(location_type l) { return symbol_type(token::ADD, std::move(l)); }
#else
  static symbol_type make_ADD(const location_type &l) { return symbol_type(token::ADD, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_SUB(location_type l) { return symbol_type(token::SUB, std::move(l)); }
#else
  static symbol_type make_SUB(const location_type &l) { return symbol_type(token::SUB, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_MUL(location_type l) { return symbol_type(token::MUL, std::move(l)); }
#else
  static symbol_type make_MUL(const location_type &l) { return symbol_type(token::MUL, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_DIV(location_type l) { return symbol_type(token::DIV, std::move(l)); }
#else
  static symbol_type make_DIV(const location_type &l) { return symbol_type(token::DIV, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_MOD(location_type l) { return symbol_type(token::MOD, std::move(l)); }
#else
  static symbol_type make_MOD(const location_type &l) { return symbol_type(token::MOD, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_ASSIGN(location_type l) { return symbol_type(token::ASSIGN, std::move(l)); }
#else
  static symbol_type make_ASSIGN(const location_type &l) { return symbol_type(token::ASSIGN, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_UMINUS(location_type l) { return symbol_type(token::UMINUS, std::move(l)); }
#else
  static symbol_type make_UMINUS(const location_type &l) { return symbol_type(token::UMINUS, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_UPLUS(location_type l) { return symbol_type(token::UPLUS, std::move(l)); }
#else
  static symbol_type make_UPLUS(const location_type &l) { return symbol_type(token::UPLUS, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_THEN(location_type l) { return symbol_type(token::THEN, std::move(l)); }
#else
  static symbol_type make_THEN(const location_type &l) { return symbol_type(token::THEN, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_AND(location_type l) { return symbol_type(token::AND, std::move(l)); }
#else
  static symbol_type make_AND(const location_type &l) { return symbol_type(token::AND, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_OR(location_type l) { return symbol_type(token::OR, std::move(l)); }
#else
  static symbol_type make_OR(const location_type &l) { return symbol_type(token::OR, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_NOT(location_type l) { return symbol_type(token::NOT, std::move(l)); }
#else
  static symbol_type make_NOT(const location_type &l) { return symbol_type(token::NOT, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_LT(location_type l) { return symbol_type(token::LT, std::move(l)); }
#else
  static symbol_type make_LT(const location_type &l) { return symbol_type(token::LT, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_GT(location_type l) { return symbol_type(token::GT, std::move(l)); }
#else
  static symbol_type make_GT(const location_type &l) { return symbol_type(token::GT, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_LE(location_type l) { return symbol_type(token::LE, std::move(l)); }
#else
  static symbol_type make_LE(const location_type &l) { return symbol_type(token::LE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_GE(location_type l) { return symbol_type(token::GE, std::move(l)); }
#else
  static symbol_type make_GE(const location_type &l) { return symbol_type(token::GE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_EQ(location_type l) { return symbol_type(token::EQ, std::move(l)); }
#else
  static symbol_type make_EQ(const location_type &l) { return symbol_type(token::EQ, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_NEQ(location_type l) { return symbol_type(token::NEQ, std::move(l)); }
#else
  static symbol_type make_NEQ(const location_type &l) { return symbol_type(token::NEQ, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_INTCONST(int v, location_type l) {
    return symbol_type(token::INTCONST, std::move(v), std::move(l));
  }
#else
  static symbol_type make_INTCONST(const int &v, const location_type &l) { return symbol_type(token::INTCONST, v, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_FLOATCONST(float v, location_type l) {
    return symbol_type(token::FLOATCONST, std::move(v), std::move(l));
  }
#else
  static symbol_type make_FLOATCONST(const float &v, const location_type &l) {
    return symbol_type(token::FLOATCONST, v, l);
  }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_IDENTIFIER(std::string v, location_type l) {
    return symbol_type(token::IDENTIFIER, std::move(v), std::move(l));
  }
#else
  static symbol_type make_IDENTIFIER(const std::string &v, const location_type &l) {
    return symbol_type(token::IDENTIFIER, v, l);
  }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_LCBRACE(location_type l) { return symbol_type(token::LCBRACE, std::move(l)); }
#else
  static symbol_type make_LCBRACE(const location_type &l) { return symbol_type(token::LCBRACE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_RCBRACE(location_type l) { return symbol_type(token::RCBRACE, std::move(l)); }
#else
  static symbol_type make_RCBRACE(const location_type &l) { return symbol_type(token::RCBRACE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_LSBRACE(location_type l) { return symbol_type(token::LSBRACE, std::move(l)); }
#else
  static symbol_type make_LSBRACE(const location_type &l) { return symbol_type(token::LSBRACE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_RSBRACE(location_type l) { return symbol_type(token::RSBRACE, std::move(l)); }
#else
  static symbol_type make_RSBRACE(const location_type &l) { return symbol_type(token::RSBRACE, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_LPAREN(location_type l) { return symbol_type(token::LPAREN, std::move(l)); }
#else
  static symbol_type make_LPAREN(const location_type &l) { return symbol_type(token::LPAREN, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_RPAREN(location_type l) { return symbol_type(token::RPAREN, std::move(l)); }
#else
  static symbol_type make_RPAREN(const location_type &l) { return symbol_type(token::RPAREN, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_SEMICOLON(location_type l) { return symbol_type(token::SEMICOLON, std::move(l)); }
#else
  static symbol_type make_SEMICOLON(const location_type &l) { return symbol_type(token::SEMICOLON, l); }
#endif
#if 201103L <= YY_CPLUSPLUS
  static symbol_type make_COMMA(location_type l) { return symbol_type(token::COMMA, std::move(l)); }
#else
  static symbol_type make_COMMA(const location_type &l) { return symbol_type(token::COMMA, l); }
#endif

  class context {
   public:
    context(const parser &yyparser, const symbol_type &yyla);
    const symbol_type &lookahead() const YY_NOEXCEPT { return yyla_; }
    symbol_kind_type token() const YY_NOEXCEPT { return yyla_.kind(); }
    const location_type &location() const YY_NOEXCEPT { return yyla_.location; }

    /// Put in YYARG at most YYARGN of the expected tokens, and return the
    /// number of tokens stored in YYARG.  If YYARG is null, return the
    /// number of expected tokens (guaranteed to be less than YYNTOKENS).
    int expected_tokens(symbol_kind_type yyarg[], int yyargn) const;

   private:
    const parser &yyparser_;
    const symbol_type &yyla_;
  };

 private:
#if YY_CPLUSPLUS < 201103L
  /// Non copyable.
  parser(const parser &);
  /// Non copyable.
  parser &operator=(const parser &);
#endif

  /// Stored state numbers (used for stacks).
  typedef unsigned char state_type;

  /// The arguments of the error message.
  int yy_syntax_error_arguments_(const context &yyctx, symbol_kind_type yyarg[], int yyargn) const;

  /// Generate an error message.
  /// \param yyctx     the context in which the error occurred.
  virtual std::string yysyntax_error_(const context &yyctx) const;
  /// Compute post-reduction state.
  /// \param yystate   the current state
  /// \param yysym     the nonterminal to push on the stack
  static state_type yy_lr_goto_state_(state_type yystate, int yysym);

  /// Whether the given \c yypact_ value indicates a defaulted state.
  /// \param yyvalue   the value to check
  static bool yy_pact_value_is_default_(int yyvalue) YY_NOEXCEPT;

  /// Whether the given \c yytable_ value indicates a syntax error.
  /// \param yyvalue   the value to check
  static bool yy_table_value_is_error_(int yyvalue) YY_NOEXCEPT;

  static const signed char yypact_ninf_;
  static const signed char yytable_ninf_;

  /// Convert a scanner token kind \a t to a symbol kind.
  /// In theory \a t should be a token_kind_type, but character literals
  /// are valid, yet not members of the token_kind_type enum.
  static symbol_kind_type yytranslate_(int t) YY_NOEXCEPT;

  // Tables.
  // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
  // STATE-NUM.
  static const short yypact_[];

  // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
  // Performed when YYTABLE does not specify something else to do.  Zero
  // means the default is an error.
  static const signed char yydefact_[];

  // YYPGOTO[NTERM-NUM].
  static const short yypgoto_[];

  // YYDEFGOTO[NTERM-NUM].
  static const unsigned char yydefgoto_[];

  // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
  // positive, shift that token.  If negative, reduce the rule whose
  // number is the opposite.  If YYTABLE_NINF, syntax error.
  static const unsigned char yytable_[];

  static const short yycheck_[];

  // YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
  // state STATE-NUM.
  static const signed char yystos_[];

  // YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.
  static const signed char yyr1_[];

  // YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.
  static const signed char yyr2_[];

#if YYDEBUG
  // YYRLINE[YYN] -- Source line where rule number YYN was defined.
  static const short yyrline_[];
  /// Report on the debug stream that the rule \a r is going to be reduced.
  virtual void yy_reduce_print_(int r) const;
  /// Print the state stack on the debug stream.
  virtual void yy_stack_print_() const;

  /// Debugging level.
  int yydebug_;
  /// Debug stream.
  std::ostream *yycdebug_;

  /// \brief Display a symbol kind, value and location.
  /// \param yyo    The output stream.
  /// \param yysym  The symbol.
  template <typename Base>
  void yy_print_(std::ostream &yyo, const basic_symbol<Base> &yysym) const;
#endif

  /// \brief Reclaim the memory associated to a symbol.
  /// \param yymsg     Why this token is reclaimed.
  ///                  If null, print nothing.
  /// \param yysym     The symbol.
  template <typename Base>
  void yy_destroy_(const char *yymsg, basic_symbol<Base> &yysym) const;

 private:
  /// Type access provider for state based symbols.
  struct by_state {
    /// Default constructor.
    by_state() YY_NOEXCEPT;

    /// The symbol kind as needed by the constructor.
    typedef state_type kind_type;

    /// Constructor.
    by_state(kind_type s) YY_NOEXCEPT;

    /// Copy constructor.
    by_state(const by_state &that) YY_NOEXCEPT;

    /// Record that this symbol is empty.
    void clear() YY_NOEXCEPT;

    /// Steal the symbol kind from \a that.
    void move(by_state &that);

    /// The symbol kind (corresponding to \a state).
    /// \a symbol_kind::S_YYEMPTY when empty.
    symbol_kind_type kind() const YY_NOEXCEPT;

    /// The state number used to denote an empty symbol.
    /// We use the initial state, as it does not have a value.
    enum { empty_state = 0 };

    /// The state.
    /// \a empty when empty.
    state_type state;
  };

  /// "Internal" symbol: element of the stack.
  struct stack_symbol_type : basic_symbol<by_state> {
    /// Superclass.
    typedef basic_symbol<by_state> super_type;
    /// Construct an empty symbol.
    stack_symbol_type();
    /// Move or copy construction.
    stack_symbol_type(YY_RVREF(stack_symbol_type) that);
    /// Steal the contents from \a sym to build this.
    stack_symbol_type(state_type s, YY_MOVE_REF(symbol_type) sym);
#if YY_CPLUSPLUS < 201103L
    /// Assignment, needed by push_back by some old implementations.
    /// Moves the contents of that.
    stack_symbol_type &operator=(stack_symbol_type &that);

    /// Assignment, needed by push_back by other implementations.
    /// Needed by some other old implementations.
    stack_symbol_type &operator=(const stack_symbol_type &that);
#endif
  };

  /// A stack with random access from its top.
  template <typename T, typename S = std::vector<T> >
  class stack {
   public:
    // Hide our reversed order.
    typedef typename S::iterator iterator;
    typedef typename S::const_iterator const_iterator;
    typedef typename S::size_type size_type;
    typedef typename std::ptrdiff_t index_type;

    stack(size_type n = 200) YY_NOEXCEPT : seq_(n) {}

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    stack(const stack &) = delete;
    /// Non copyable.
    stack &operator=(const stack &) = delete;
#endif

    /// Random access.
    ///
    /// Index 0 returns the topmost element.
    const T &operator[](index_type i) const { return seq_[size_type(size() - 1 - i)]; }

    /// Random access.
    ///
    /// Index 0 returns the topmost element.
    T &operator[](index_type i) { return seq_[size_type(size() - 1 - i)]; }

    /// Steal the contents of \a t.
    ///
    /// Close to move-semantics.
    void push(YY_MOVE_REF(T) t) {
      seq_.push_back(T());
      operator[](0).move(t);
    }

    /// Pop elements from the stack.
    void pop(std::ptrdiff_t n = 1) YY_NOEXCEPT {
      for (; 0 < n; --n) seq_.pop_back();
    }

    /// Pop all elements from the stack.
    void clear() YY_NOEXCEPT { seq_.clear(); }

    /// Number of elements on the stack.
    index_type size() const YY_NOEXCEPT { return index_type(seq_.size()); }

    /// Iterator on top of the stack (going downwards).
    const_iterator begin() const YY_NOEXCEPT { return seq_.begin(); }

    /// Bottom of the stack.
    const_iterator end() const YY_NOEXCEPT { return seq_.end(); }

    /// Present a slice of the top of a stack.
    class slice {
     public:
      slice(const stack &stack, index_type range) YY_NOEXCEPT : stack_(stack), range_(range) {}

      const T &operator[](index_type i) const { return stack_[range_ - i]; }

     private:
      const stack &stack_;
      index_type range_;
    };

   private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    stack(const stack &);
    /// Non copyable.
    stack &operator=(const stack &);
#endif
    /// The wrapped container.
    S seq_;
  };

  /// Stack type.
  typedef stack<stack_symbol_type> stack_type;

  /// The stack.
  stack_type yystack_;

  /// Push a new state on the stack.
  /// \param m    a debug message to display
  ///             if null, no trace is output.
  /// \param sym  the symbol
  /// \warning the contents of \a s.value is stolen.
  void yypush_(const char *m, YY_MOVE_REF(stack_symbol_type) sym);

  /// Push a new look ahead token on the state on the stack.
  /// \param m    a debug message to display
  ///             if null, no trace is output.
  /// \param s    the state
  /// \param sym  the symbol (for its value and location).
  /// \warning the contents of \a sym.value is stolen.
  void yypush_(const char *m, state_type s, YY_MOVE_REF(symbol_type) sym);

  /// Pop \a n symbols from the stack.
  void yypop_(int n = 1) YY_NOEXCEPT;

  /// Constants.
  enum {
    yylast_ = 494,  ///< Last index in yytable_.
    yynnts_ = 28,   ///< Number of nonterminal symbols.
    yyfinal_ = 17   ///< Termination state number.
  };

  // User arguments.
  frontend::BisonParserImpl &driver;
};

inline parser::symbol_kind_type parser::yytranslate_(int t) YY_NOEXCEPT {
  // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
  // TOKEN-NUM as returned by yylex.
  static const signed char translate_table[] = {
      0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
      2,  2,  2,  2,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
      25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42};
  // Last valid token kind.
  const int code_max = 297;

  if (t <= 0)
    return symbol_kind::S_YYEOF;
  else if (t <= code_max)
    return static_cast<symbol_kind_type>(translate_table[t]);
  else
    return symbol_kind::S_YYUNDEF;
}

// basic_symbol.
template <typename Base>
parser::basic_symbol<Base>::basic_symbol(const basic_symbol &that) : Base(that), value(), location(that.location) {
  switch (this->kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.copy<float>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.copy<frontend::ASTNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.copy<frontend::DeclNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.copy<frontend::ExprNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.copy<frontend::FuncFParamNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials:  // initials
      value.copy<frontend::InitializerNodePtr>(YY_MOVE(that.value));
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
      value.copy<frontend::StmtNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def:  // def
      value.copy<frontend::VarDefNodePtr>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.copy<int>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.copy<std::string>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_compunit:  // compunit
      value.copy<std::vector<frontend::ASTNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.copy<std::vector<frontend::ExprNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.copy<std::vector<frontend::FuncFParamNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.copy<std::vector<frontend::InitializerNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.copy<std::vector<frontend::StmtNodePtr> *>(YY_MOVE(that.value));
      break;

    case symbol_kind::S_def_list:  // def_list
      value.copy<std::vector<frontend::VarDefNodePtr> *>(YY_MOVE(that.value));
      break;

    default:
      break;
  }
}

template <typename Base>
parser::symbol_kind_type parser::basic_symbol<Base>::type_get() const YY_NOEXCEPT {
  return this->kind();
}

template <typename Base>
bool parser::basic_symbol<Base>::empty() const YY_NOEXCEPT {
  return this->kind() == symbol_kind::S_YYEMPTY;
}

template <typename Base>
void parser::basic_symbol<Base>::move(basic_symbol &s) {
  super_type::move(s);
  switch (this->kind()) {
    case symbol_kind::S_FLOATCONST:  // "float constant"
      value.move<float>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_program:  // program
    case symbol_kind::S_funcdef:  // funcdef
      value.move<frontend::ASTNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_global_decl:  // global_decl
    case symbol_kind::S_decl:         // decl
      value.move<frontend::DeclNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_expr:      // expr
    case symbol_kind::S_lval:      // lval
    case symbol_kind::S_funccall:  // funccall
      value.move<frontend::ExprNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_func_fparam:  // func_fparam
      value.move<frontend::FuncFParamNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_initials:  // initials
      value.move<frontend::InitializerNodePtr>(YY_MOVE(s.value));
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
      value.move<frontend::StmtNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_def:  // def
      value.move<frontend::VarDefNodePtr>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_INTCONST:  // "integer constant"
      value.move<int>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_IDENTIFIER:  // "identifier"
      value.move<std::string>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_compunit:  // compunit
      value.move<std::vector<frontend::ASTNodePtr> *>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_array_indices:        // array_indices
    case symbol_kind::S_func_fparam_indices:  // func_fparam_indices
    case symbol_kind::S_func_rparams:         // func_rparams
      value.move<std::vector<frontend::ExprNodePtr> *>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_func_fparams:  // func_fparams
      value.move<std::vector<frontend::FuncFParamNodePtr> *>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_initials_list:  // initials_list
      value.move<std::vector<frontend::InitializerNodePtr> *>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_stmt_list:  // stmt_list
      value.move<std::vector<frontend::StmtNodePtr> *>(YY_MOVE(s.value));
      break;

    case symbol_kind::S_def_list:  // def_list
      value.move<std::vector<frontend::VarDefNodePtr> *>(YY_MOVE(s.value));
      break;

    default:
      break;
  }

  location = YY_MOVE(s.location);
}

// by_kind.
inline parser::by_kind::by_kind() YY_NOEXCEPT : kind_(symbol_kind::S_YYEMPTY) {}

#if 201103L <= YY_CPLUSPLUS
inline parser::by_kind::by_kind(by_kind &&that) YY_NOEXCEPT : kind_(that.kind_) { that.clear(); }
#endif

inline parser::by_kind::by_kind(const by_kind &that) YY_NOEXCEPT : kind_(that.kind_) {}

inline parser::by_kind::by_kind(token_kind_type t) YY_NOEXCEPT : kind_(yytranslate_(t)) {}

inline void parser::by_kind::clear() YY_NOEXCEPT { kind_ = symbol_kind::S_YYEMPTY; }

inline void parser::by_kind::move(by_kind &that) {
  kind_ = that.kind_;
  that.clear();
}

inline parser::symbol_kind_type parser::by_kind::kind() const YY_NOEXCEPT { return kind_; }

inline parser::symbol_kind_type parser::by_kind::type_get() const YY_NOEXCEPT { return this->kind(); }

}  // namespace yy
#line 2568 "frontend/parser/bison.h"

#endif  // !YY_YY_FRONTEND_PARSER_BISON_H_INCLUDED
