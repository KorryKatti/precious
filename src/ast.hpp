#pragma once

/**
 * @file ast.hpp
 * @brief AST (Abstract Syntax Tree) node definitions for the Precious language.
 *
 * The AST is what the parser builds and what the code generator reads. It is a
 * plain tree of small structs, each one describing one thing in the program.
 *
 * There are three layers:
 *
 *   NodeProg   the whole file
 *     └─ stmts: the top level, which may only contain NodeStmtFn (one of which
 *        must be the entry point, `the_precious`)
 *
 *   NodeStmt   a statement -- an action
 *     └─ var: one of the NodeStmt* structs
 *
 *   NodeExpr   a value
 *     └─ var: either a NodeTerm* (a single value) or a NodeBinExpr*
 *                (two values joined by an operator)
 *
 * `var` is a std::variant, which is just a pointer that remembers which kind of
 * struct it points at. To read it:
 *
 *   if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {   // "is it an if?"
 *       auto node = std::get<NodeStmtIf*>(stmt->var);      // "give me the if"
 *   }
 *
 * The three statements worth knowing about, because they wrap other statements:
 *
 *   NodeScope  a { ... } block, holding a list of statements
 *   NodeStmtIf if / elif / else, holding NodeScopes
 *   NodeStmtFn fn name(...) { ... }, holding parameters and a NodeScope
 *
 * Memory: every node is created with plain `new` and never deleted. The compiler
 * is a short-lived process, so the operating system reclaims everything when it
 * exits -- there is no cleanup to get wrong.
 */

#include <optional>
#include <variant>
#include <vector>

#include "tokenization.hpp"

// ============================================================================
// Forward declarations (needed because nodes reference each other)
// ============================================================================

struct NodeExpr;
struct NodeScope;

// ============================================================================
// Binary operator enum
// ============================================================================

/**
 * @enum BinOp
 * @brief All binary operators the language supports.
 *
 * Precedence is NOT encoded here — see bin_prec() in tokenization.hpp.
 * This enum just tags what operation a NodeBinExpr represents.
 */
enum class BinOp {
    Add, Sub, Mul, Div, Mod,    // arithmetic
    Eq, NotEq,                // equality
    Lt, Gt, LtEq, GtEq,      // comparison
    And, Or,                   // logical
};

// ============================================================================
// Term nodes — the atomic units of expressions
// ============================================================================

struct NodeTermIntLit {
    Token int_lit;
};

struct NodeTermIdent { ///< a variable name
    Token ident;
};

struct NodeTermParen { ///< ( expr )
    NodeExpr* expr;
};

struct NodeTermNot { ///< ! expr
    NodeExpr* expr;
};

struct NodeTermStringLit { ///< "hello"
    Token string_lit;
};

/// One parameter of a function: `name`, `name: type`, or `name: type[]`.
struct NodeFnParam {
    Token name;
    bool isArray = false;  ///< written with `[]`, passed as std::vector<T>&
    std::optional<TokenType> type_annotation;
};

struct NodeTermFnCall { ///< name(arg, arg)  -- also how `ask(...)` is represented
    Token name;
    std::vector<NodeExpr*> args;
};

struct NodeTermArrayLit { ///< [1, 2, 3]
    std::vector<NodeExpr*> elements;
};

struct NodeTermArrayIndex { ///< arr[i]  -- and s[i] for strings
    NodeExpr* ident;
    NodeExpr* index;
};

struct NodeTermUnaryMinus { ///< - expr
    NodeExpr* expr;
};

// ++ and --, in all four forms. `expr` is always a NodeTermIdent, because ++
// and -- only make sense on a named variable -- not on a literal, and not on a
// function call result.
struct NodeTermPreInc { ///< ++x
    NodeExpr* expr;
};

struct NodeTermPostInc { ///< x++
    NodeExpr* expr;
};

struct NodeTermPreDec { ///< --x
    NodeExpr* expr;
};

struct NodeTermPostDec { ///< x--
    NodeExpr* expr;
};

// ============================================================================
// Term — a single value, with a note of which kind it is
// ============================================================================

struct NodeTerm {
    std::variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*, NodeTermNot*,
                 NodeTermStringLit*, NodeTermFnCall*, NodeTermArrayLit*,
                 NodeTermArrayIndex*, NodeTermUnaryMinus*, NodeTermPreInc*,
                 NodeTermPostInc*, NodeTermPreDec*, NodeTermPostDec*>
        var;
};

// ============================================================================
// Binary expression — one operator, two sides
// ============================================================================

struct NodeBinExpr {
    BinOp op;
    NodeExpr* lhs;
    NodeExpr* rhs;
};

// ============================================================================
// Expression — either a single term or a binary expression
// ============================================================================

struct NodeExpr {
    std::variant<NodeTerm*, NodeBinExpr*> var;
};

// ============================================================================
// Statement nodes — the top-level units of code
//
// Each one is named after the Precious syntax it came from, and each is listed
// in the same order as the NodeStmt variant at the bottom of this file.
// ============================================================================

struct NodeStmtExit { ///< gives expr;
    NodeExpr* expr;
};

struct NodeStmtLet { ///< my x [: type] = expr;
    Token ident;
    NodeExpr* expr;
    std::optional<TokenType> type_annotation;  ///< std::nullopt = no `: type`
    bool is_array = false;                      ///< written as `type[]` or `type[3]`
    std::optional<Token> array_size;            ///< the `3` in `number[3]`
};

struct NodeStmt;
struct NodeIfPred;

/// A { ... } block. Also used for a function body and for if/while/for bodies.
struct NodeScope {
    std::vector<NodeStmt*> stmts;
};

/// elif (cond) { ... }, which may itself be followed by another elif or an else.
struct NodeIfPredElif {
    NodeExpr* expr;
    NodeScope* scope;
    std::optional<NodeIfPred*> pred;  ///< the rest of the chain, if any
};

struct NodeIfPredElse { ///< else { ... }
    NodeScope* scope;
};

/// The tail of an if/elif/else chain: either another branch, or the end.
struct NodeIfPred {
    std::variant<NodeIfPredElif*, NodeIfPredElse*> var;
};

struct NodeStmtIf { ///< if (cond) { ... }
    NodeExpr* expr{};
    NodeScope* scope{};
    std::optional<NodeIfPred*> pred;  ///< the elif/else tail, if any
};

struct NodeStmtAssign { ///< x = expr;
    Token ident;
    NodeExpr* expr;
};

struct NodeStmtWhile { ///< while (cond) { ... }
    NodeExpr* expr;
    NodeScope* scope;
};

struct NodeStmtPrint {
    NodeExpr* expr;
};

struct NodeStmtExpr {
    NodeExpr* expr;
};

struct NodeStmtFn {
    Token name;
    std::vector<NodeFnParam> params;
    std::optional<TokenType> return_type;  // std::nullopt means "no -> type"
    NodeScope* body;
};

struct NodeStmtArrayAssign { ///< arr[i] = expr;
    Token ident;
    NodeExpr* index;
    NodeExpr* expr;
};

/// break;  -- carries nothing. If a future `break value;` is wanted, an
/// `std::optional<NodeExpr*> expr;` field goes here (the commented-out line in
/// the roadmap's "break with value" entry).
struct NodeStmtBreak {};

struct NodeCase { ///< one `case 1: { ... }` branch
    NodeExpr* value;  ///< Must be an integer literal, because C++ needs a constant.
    NodeScope* body;
};

struct NodeStmtSwitch { ///< switch (x) { case ...: ... default: ... }
    NodeExpr* expr;                          ///< The value being matched.
    std::vector<NodeCase*> cases;            ///< The case branches.
    std::optional<NodeScope*> default_body;  ///< The default branch, if written.
};

struct NodeStmtContinue {}; ///< continue;  -- carries nothing

struct NodeStmtFor { ///< for (init; cond; update) { ... }
    NodeStmt* init;      ///< e.g. `my i = 0`, or nullptr for an empty slot
    NodeExpr* condition; ///< e.g. `i < 5`, or nullptr for "always true"
    NodeStmt* update;    ///< e.g. `i = i + 1`, `i++`, or nullptr for an empty slot
    NodeScope* body;
};

struct NodeStmtForEach { ///< for (item in arr) { ... }
    Token element;   ///< The name each value is bound to
    NodeExpr* array; ///< The array being walked
    NodeScope* body;
};

struct NodeStmtPush { ///< push arr, value;
    Token ident;    ///< The array
    NodeExpr* expr; ///< The value to add
};

struct NodeStmtPop { ///< pop arr;
    Token ident;    ///< The array
};

struct NodeStmtCompoundAssign { ///< x += expr;  (also -= *= /= %=)
    Token ident;    ///< The variable to modify
    TokenType op;   ///< pluseq, minuseq, stareq, fslasheq or moduloeq
    NodeExpr* expr;
};

// ============================================================================
// Statement — any statement, with a note of which kind it is
//
// The list below is the full set of statements in the language. When you add
// one, add it here too, then handle it in gen_stmt() (which will tell you if
// you forget).
// ============================================================================

struct NodeStmt {
    std::variant<NodeStmtExit*, NodeStmtLet*, NodeScope*, NodeStmtIf*,
                 NodeStmtAssign*, NodeStmtCompoundAssign*, NodeStmtWhile*,
                 NodeStmtPrint*, NodeStmtFn*, NodeStmtExpr*, NodeStmtArrayAssign*,
                 NodeStmtBreak*, NodeStmtContinue*, NodeStmtSwitch*, NodeStmtFor*,
                 NodeStmtForEach*, NodeStmtPush*, NodeStmtPop*>
        var;
};

// ============================================================================
// Program — top-level function definitions plus the entry function
// ============================================================================

struct NodeProg {
    std::vector<NodeStmt*> stmts;   ///< Top level: only NodeStmtFn allowed.
    NodeStmtFn* entry_fn = nullptr; ///< `fn the_precious()` — the program entry.
                                    ///< Also present in stmts. Null while parsing.
};
