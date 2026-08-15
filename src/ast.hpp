#pragma once

/**
 * @file ast.hpp
 * @brief AST (Abstract Syntax Tree) node definitions for the Precious language.
 *
 * This file defines every node type the parser can produce. The hierarchy:
 *
 *   NodeProg
 *     └─ NodeStmt* ──┬─ NodeStmtExit       (gives expr;)
 *                     ├─ NodeStmtLet         (my x = expr;)
 *                     ├─ NodeStmtAssign      (x = expr;)
 *                     ├─ NodeStmtWhile       (while (expr) { ... })
 *                     ├─ NodeStmtIf          (if / elif / else)
 *                     ├─ NodeScope           ({ ... })
 *                     ├─ NodeStmtPrint       (say(expr))
 *                     ├─ NodeStmtFn          (fn name(params) { ... })
 *                     ├─ NodeStmtExpr        (function call as statement)
 *                     └─ NodeStmtArrayAssign (arr[i] = expr;)
 *
 *   NodeExpr
 *     └─ variant<NodeTerm*, NodeBinExpr*>
 *
 *   NodeTerm
 *     └─ variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*,
 *                 NodeTermNot*, NodeTermStringLit*, NodeTermFnCall*,
 *                 NodeTermArrayLit*, NodeTermArrayIndex*>
 *
 *   NodeBinExpr
 *     └─ { BinOp op, NodeExpr* lhs, NodeExpr* rhs }
 *
 * Memory: all nodes are allocated via ArenaAllocator (see arena.hpp).
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

struct NodeTermIdent {
    Token ident;
};

struct NodeTermParen {
    NodeExpr* expr;
};

struct NodeTermNot {
    NodeExpr* expr;
};

struct NodeTermStringLit {
    Token string_lit;
};

struct NodeFnParam {
    Token name;
    bool isArray = false; // used to iundicate if passed paramater is an array.
    std::optional<TokenType> type_annotation;
};

struct NodeTermFnCall {
    Token name;
    std::vector<NodeExpr*> args;
};

struct NodeTermArrayLit {
    std::vector<NodeExpr*> elements;
};

struct NodeTermArrayIndex {
    NodeExpr* ident;
    NodeExpr* index;
};

struct NodeTermUnaryMinus {
    NodeExpr* expr;
};

// ============================================================================
// Term — wraps all possible term types in a variant
// ============================================================================

struct NodeTerm {
    std::variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*, NodeTermNot*,
                 NodeTermStringLit*, NodeTermFnCall*, NodeTermArrayLit*,
                 NodeTermArrayIndex*, NodeTermUnaryMinus*>
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
// ============================================================================

struct NodeStmtExit {
    NodeExpr* expr;
};

struct NodeStmtLet {
    Token ident;
    NodeExpr* expr;
    std::optional<TokenType> type_annotation;
    bool is_array = false;
    std::optional<Token> array_size;
};

struct NodeStmt;
struct NodeIfPred;

struct NodeScope {
    std::vector<NodeStmt*> stmts;
};

struct NodeIfPredElif {
    NodeExpr* expr;
    NodeScope* scope;
    std::optional<NodeIfPred*> pred;
};

struct NodeIfPredElse {
    NodeScope* scope;
};

struct NodeIfPred {
    std::variant<NodeIfPredElif*, NodeIfPredElse*> var;
};

struct NodeStmtIf {
    NodeExpr* expr{};
    NodeScope* scope{};
    std::optional<NodeIfPred*> pred;
};

struct NodeStmtAssign {
    Token ident;
    NodeExpr* expr;
};

struct NodeStmtWhile {
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
    std::optional<std::string> return_type;
    NodeScope* body;
};

struct NodeStmtArrayAssign {
    Token ident;
    NodeExpr* index;
    NodeExpr* expr;
};

struct NodeStmtBreak {
    // NodeExpr* expr;
};

struct NodeCase {
    NodeExpr* value;      ///< The case value (must be compile-time constant int).
    NodeScope* body;      ///< The scope of statements for this case.
};

struct NodeStmtSwitch {
    NodeExpr* expr;                            ///< The value to match on.
    std::vector<NodeCase*> cases;              ///< The case branches.
    std::optional<NodeScope*> default_body;    ///< Optional default branch.
};

struct NodeStmtContinue {
    // NodeExpr* expr;
};

struct NodeStmtFor {
    NodeStmt* init;      ///< Init statement (e.g., my i = 0)
    NodeExpr* condition; ///< Loop condition
    NodeStmt* update;    ///< Update statement (e.g., i = i + 1)
    NodeScope* body;     ///< Loop body
};

struct NodeStmtForEach {
    Token element;       ///< Element variable name
    NodeExpr* array;     ///< Array expression to iterate over
    NodeScope* body;     ///< Loop body
};
// ============================================================================
// Statement — wraps all possible statement types in a variant
// ============================================================================

struct NodeStmt {
    std::variant<NodeStmtExit*, NodeStmtLet*, NodeScope*, NodeStmtIf*,
                 NodeStmtAssign*, NodeStmtWhile*, NodeStmtPrint*, NodeStmtFn*,
                 NodeStmtExpr*, NodeStmtArrayAssign*, NodeStmtBreak*, NodeStmtContinue*,
                 NodeStmtSwitch*, NodeStmtFor*, NodeStmtForEach*>
        var;
};

// ============================================================================
// Program — a sequence of top-level statements
// ============================================================================

struct NodeProg {
    std::vector<NodeStmt*> stmts;
};
