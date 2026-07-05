#pragma once

/**
 * @file parser.hpp
 * @brief Parser and AST node definitions for the Precious programming language.
 *
 * This file defines:
 * 1. AST (Abstract Syntax Tree) node types representing the language grammar
 * 2. A recursive descent parser that converts tokens into an AST
 *
 * The parser uses operator precedence climbing for expressions and supports:
 * - Integer literals and identifiers
 * - Binary expressions (+, -, *, /)
 * - Parenthesized expressions
 * - Variable declarations (we_haves)
 * - Variable assignment
 * - Exit/return statements (gives)
 * - Scoped blocks ({ ... })
 * - If/elif/else control flow
 *
 * Memory management is handled via an ArenaAllocator, which allocates AST nodes
 * in bulk and frees them all when the parser is destroyed.
 */

#include <cassert>
#include <cstdlib>
#include <optional>
#include <variant>
#include <vector>

#include "./arena.hpp"
#include "tokenization.hpp"

// ============================================================================
// AST Node Definitions
// ============================================================================

/**
 * @struct NodeTermIntLit
 * @brief AST node for an integer literal (e.g., 42).
 */
struct NodeTermIntLit {
    Token int_lit;  ///< The integer literal token.
};

/**
 * @struct NodeTermIdent
 * @brief AST node for an identifier reference (e.g., variable name).
 */
struct NodeTermIdent {
    Token ident;  ///< The identifier token.
};

struct NodeExpr;

/**
 * @struct NodeTermParen
 * @brief AST node for a parenthesized expression (e.g., (expr)).
 */
struct NodeTermParen {
    NodeExpr* expr;  ///< The expression inside the parentheses.
};

/**
 * @struct NodeBinExprAdd
 * @brief AST node for addition: lhs + rhs.
 */
struct NodeBinExprAdd {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
};

/**
 * @struct NodeBinExprMulti
 * @brief AST node for multiplication: lhs * rhs.
 */
struct NodeBinExprMulti {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
};

/**
 * @struct NodeBinExprSub
 * @brief AST node for subtraction: lhs - rhs.
 */
struct NodeBinExprSub {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
};

/**
 * @struct NodeBinExprDiv
 * @brief AST node for division: lhs / rhs.
 */
struct NodeBinExprDiv {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
};

/**
* @struct NodeBinExprEq
* @brief AST node for equality comparison: lhs == rhs.
 */
 struct NodeBinExprEq {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

 /**
 * @struct NodeBinExprNotEq
 * @brief AST node for inequality comparison: lhs != rhs.
 */
 struct NodeBinExprNotEq {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprLt
 * @brief AST node for less-than comparison: lhs < rhs.
 */
 struct NodeBinExprLt {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprGt
 * @brief AST node for greater-than comparison: lhs > rhs.
 */
 struct NodeBinExprGt {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprLtEq
 * @brief AST node for less-than-or-equal comparison: lhs <= rhs.
 */
 struct NodeBinExprLtEq {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprGtEq
 * @brief AST node for greater-than-or-equal comparison: lhs >= rhs.
 */
 struct NodeBinExprGtEq {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };


/**
* @struct NodeBinExprAnd
 * @brief AST node for logical AND: lhs and rhs.
 */
 struct NodeBinExprAnd {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprOr
 * @brief AST node for logical OR: lhs or rhs.
 */
 struct NodeBinExprOr {
    NodeExpr* lhs;  ///< Left-hand side expression.
    NodeExpr* rhs;  ///< Right-hand side expression.
 };

  /**
 * @struct NodeBinExprNot
 * @brief AST node for logical NOT: !expr.
 */
 struct NodeTermNot {
    NodeExpr* expr;  ///< Expression to negate.
 };



/**
 * @struct NodeBinExpr
 * @brief AST node for a binary expression (dispatched to specific operation types).
 */
struct NodeBinExpr {
    std::variant<NodeBinExprAdd*, NodeBinExprMulti*, NodeBinExprSub*, NodeBinExprDiv*, NodeBinExprEq*, NodeBinExprNotEq*, NodeBinExprLt*, NodeBinExprGt*, NodeBinExprLtEq*, NodeBinExprGtEq*, NodeBinExprAnd*, NodeBinExprOr*> var;
};

/**
 * @struct NodeTerm
 * @brief AST node for a term (the basic unit of an expression).
 *
 * A term can be an integer literal, an identifier, or a parenthesized expression.
 */
struct NodeTerm {
    std::variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*, NodeTermNot*> var;
};

/**
 * @struct NodeExpr
 * @brief AST node for an expression.
 *
 * An expression is either a single term or a binary expression combining two sub-expressions.
 */
struct NodeExpr {
    std::variant<NodeTerm*, NodeBinExpr*> var;
};

/**
 * @struct NodeStmtExit
 * @brief AST node for a "gives" (exit/return) statement.
 *
 * Usage: gives <expression>;
 * The expression is evaluated and used as the exit code.
 */
struct NodeStmtExit {
    NodeExpr* expr;  ///< The expression whose value becomes the exit code.
};

/**
 * @struct NodeStmtLet
 * @brief AST node for a variable declaration ("we_haves" statement).
 *
 * Usage: we_haves <name> = <expression>;
 */
struct NodeStmtLet {
    Token ident;    ///< The variable name token.
    NodeExpr* expr; ///< The initializer expression.
};

struct NodeStmt;

/**
 * @struct NodeScope
 * @brief AST node for a scoped block of statements ({ ... }).
 */
struct NodeScope {
    std::vector<NodeStmt*> stmts;  ///< Statements within this scope.
};

struct NodeIfPred;

/**
 * @struct NodeIfPredElif
 * @brief AST node for an "elif" branch in an if/elif/else chain.
 */
struct NodeIfPredElif {
    NodeExpr* expr;                          ///< The elif condition.
    NodeScope* scope;                        ///< The scope executed if condition is true.
    std::optional<NodeIfPred*> pred;         ///< Optional subsequent elif/else branch.
};

/**
 * @struct NodeIfPredElse
 * @brief AST node for an "else" branch (final fallback in if/elif/else chain).
 */
struct NodeIfPredElse {
    NodeScope* scope;  ///< The scope executed when no prior conditions matched.
};

/**
 * @struct NodeIfPred
 * @brief AST node for an if/elif/else predicate chain.
 */
struct NodeIfPred{
    std::variant<NodeIfPredElif*,NodeIfPredElse*> var;
};

/**
 * @struct NodeStmtIf
 * @brief AST node for an if/elif/else statement.
 *
 * Usage: if (<condition>) { ... } elif (<condition>) { ... } else { ... }
 */
struct NodeStmtIf {
    NodeExpr* expr{};                      ///< The if condition.
    NodeScope* scope{};                    ///< The scope executed if condition is true.
    std::optional<NodeIfPred*> pred;       ///< Optional elif/else branches.
};

/**
 * @struct NodeStmtAssign
 * @brief AST node for variable assignment.
 *
 * Usage: <name> = <expression>;
 */
struct NodeStmtAssign{
    Token ident;    ///< The variable name to assign to.
    NodeExpr* expr; ///< The expression to assign.
};

/**
 * @struct NodeStmt
 * @brief AST node for a statement (top-level unit of code).
 */
struct NodeStmt {
    std::variant<NodeStmtExit*, NodeStmtLet*, NodeScope*, NodeStmtIf*,NodeStmtAssign*> var;
};

/**
 * @struct NodeProg
 * @brief AST node for an entire program (a sequence of statements).
 */
struct NodeProg {
    std::vector<NodeStmt*> stmts;  ///< Top-level statements in the program.
};

// ============================================================================
// Parser
// ============================================================================

/**
 * @class Parser
 * @brief Recursive descent parser that converts tokens into an AST.
 *
 * The parser consumes a vector of tokens and builds an AST using operator
 * precedence climbing for expressions. It uses an ArenaAllocator for all
 * AST node allocations, ensuring efficient memory management.
 *
 * Usage:
 * @code
 *   Parser parser(std::move(tokens));
 *   auto prog = parser.parse_prog();
 * @endcode
 */
class Parser {
public:
    /**
     * @brief Constructs a parser with the given token stream.
     * @param tokens The vector of tokens to parse.
     *
     * Initializes the arena allocator with 4 MB of memory for AST nodes.
     */
    explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens))
          , m_allocator(1024 * 1024 * 4) {
    }

    /**
     * @brief Reports a parse error and exits.
     * @param msg Description of what was expected.
     *
     * Prints an error message with the line number and exits the program.
     */
    void error_expected(const std::string& msg) const {
        std::cerr << "[ERROR] Trickses! Trickses! Expected " << msg
                  << " but the precious found something else on line " << peek(-1).value().line
                  << "!" << std::endl;
        exit(EXIT_FAILURE);
    }

    /**
     * @brief Parses a single term (literal, identifier, or parenthesized expression).
     * @return The parsed term node, or std::nullopt if no term is found.
     *
     * Terms are the atomic units of expressions:
     * - Integer literals (e.g., 42)
     * - Identifiers (e.g., variable names)
     * - Parenthesized expressions (e.g., (expr))
     */
    std::optional<NodeTerm*> parse_term() {
        if (auto int_lit = try_consume(TokenType::int_lit)) {
            auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(int_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
            return term;
        }
        if (auto ident = try_consume(TokenType::ident)) {
            auto expr_ident = m_allocator.emplace<NodeTermIdent>(ident.value());
            auto term = m_allocator.emplace<NodeTerm>(expr_ident);
            return term;
        }
        if (auto open_parent = try_consume(TokenType::open_paren)) {
            auto expr = parse_expr();
            if (!expr.has_value()) {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            auto term_paren = m_allocator.emplace<NodeTermParen>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_paren);
            return term;
        }
        if (auto bang = try_consume(TokenType::bang)) {
            auto expr = parse_expr(5);  // NOT has highest precedence
            if (!expr.has_value()) {
                error_expected("expression");
            }
            auto term_not = m_allocator.emplace<NodeTermNot>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_not);
            return term;
        }
        return {};
    }

    /**
     * @brief Parses an expression using operator precedence climbing.
     * @param min_prec Minimum precedence level to bind (default 0).
     * @return The parsed expression node, or std::nullopt if no expression is found.
     *
     * Implements the Pratt parsing algorithm for binary expressions.
     * Operator precedence and comparsion precedence
     * - Level 0: ==, !=, <, >, <=, >= (comparison operators)
     * - Level 1: +, - (addition, subtraction)
     * - Level 2: *, / (multiplication, division)
     */
    std::optional<NodeExpr*> parse_expr(const int min_prec = 0) {
        std::optional<NodeTerm*> term_lhs = parse_term();
        if (!term_lhs.has_value()) {
            return {};
        }
        auto expr_lhs = m_allocator.emplace<NodeExpr>(term_lhs.value());

        while (true) {
            std::optional<Token> curr_token = peek();
            if (!curr_token.has_value()) {
                break;
            }
            std::optional<int> prec = bin_prec(curr_token.value().type);
            if (!prec.has_value() || prec.value() < min_prec) {
                break;
            }

            const auto [type, line, value] = consume();
            const int next_min_prec = prec.value() + 1;
            auto expr_rhs = parse_expr(next_min_prec);

            if (!expr_rhs.has_value()) {
                error_expected("expression");
            }

            auto expr = m_allocator.emplace<NodeBinExpr>();

            auto expr_lhs2 = m_allocator.emplace<NodeExpr>();

            if (type == TokenType::plus) {
                expr_lhs2->var = expr_lhs->var;
                auto add = m_allocator.emplace<NodeBinExprAdd>(expr_lhs2, expr_rhs.value());
                expr->var = add;
            } else if (type == TokenType::star) {
                expr_lhs2->var = expr_lhs->var;
                auto multi = m_allocator.emplace<NodeBinExprMulti>(expr_lhs2, expr_rhs.value());
                expr->var = multi;
            } else if (type == TokenType::minus) {
                expr_lhs2->var = expr_lhs->var;
                auto sub = m_allocator.emplace<NodeBinExprSub>(expr_lhs2, expr_rhs.value());
                expr->var = sub;
            } else if (type == TokenType::fslash) {
                expr_lhs2->var = expr_lhs->var;
                auto div = m_allocator.emplace<NodeBinExprDiv>(expr_lhs2, expr_rhs.value());
                expr->var = div;
            }
            else if (type == TokenType::eqeq) {
                expr_lhs2->var = expr_lhs->var;
                auto eqeq = m_allocator.emplace<NodeBinExprEq>(expr_lhs2, expr_rhs.value());
                expr->var = eqeq;
            }
            else if (type == TokenType::noteq) {
                expr_lhs2->var = expr_lhs->var;
                auto noteq = m_allocator.emplace<NodeBinExprNotEq>(expr_lhs2, expr_rhs.value());
                expr->var = noteq;
            }
            else if (type == TokenType::lt) {
                expr_lhs2->var = expr_lhs->var;
                auto lt = m_allocator.emplace<NodeBinExprLt>(expr_lhs2, expr_rhs.value());
                expr->var = lt;
            }
            else if (type == TokenType::gt) {
                expr_lhs2->var = expr_lhs->var;
                auto gt = m_allocator.emplace<NodeBinExprGt>(expr_lhs2, expr_rhs.value());
                expr->var = gt;
            }
            else if (type == TokenType::lteq) {
                expr_lhs2->var = expr_lhs->var;
                auto lteq = m_allocator.emplace<NodeBinExprLtEq>(expr_lhs2, expr_rhs.value());
                expr->var = lteq;
            }
            else if (type == TokenType::gteq) {
                expr_lhs2->var = expr_lhs->var;
                auto gteq = m_allocator.emplace<NodeBinExprGtEq>(expr_lhs2, expr_rhs.value());
                expr->var = gteq;
            }else if (type==TokenType::and_){
                expr_lhs2->var = expr_lhs->var;
                auto and_ = m_allocator.emplace<NodeBinExprAnd>(expr_lhs2, expr_rhs.value());
                expr->var = and_;
            }else if (type==TokenType::or_){
                expr_lhs2->var = expr_lhs->var;
                auto or_ = m_allocator.emplace<NodeBinExprOr>(expr_lhs2, expr_rhs.value());
                expr->var = or_;
            }
            else {
                assert(false);  // unreachable
            }
            expr_lhs->var = expr;
        }
        return expr_lhs;
    }

    /**
     * @brief Parses a scoped block of statements.
     * @return The parsed scope node, or std::nullopt if no '{' is found.
     *
     * Expects: { <statement>* }
     */
    std::optional<NodeScope*> parse_scope() {
        if (!try_consume(TokenType::open_curly).has_value()) {
            return {};
        }
        auto scope = m_allocator.emplace<NodeScope>();
        while (auto stmt = parse_stmt()) {
            scope->stmts.push_back(stmt.value());
        }
        try_consume_err(TokenType::close_curly);
        return scope;
    }

    /**
     * @brief Parses an elif or else predicate in an if/elif/else chain.
     * @return The parsed predicate node, or std::nullopt if neither elif nor else is found.
     *
     * Handles:
     * - elif (<expr>) { ... } [elif/else...]
     * - else { ... }
     */
    std::optional<NodeIfPred*> parse_if_pred(){
        if (try_consume(TokenType::elif)){
            try_consume_err(TokenType::open_paren);
            const auto elif = m_allocator.alloc<NodeIfPredElif>();
            if (auto expr = parse_expr()){
                elif->expr=expr.value();
            }else{
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (auto scope = parse_scope()){
                elif->scope = scope.value();
            }else{
                error_expected("scope");
            }
            elif->pred = parse_if_pred();
            auto pred = m_allocator.emplace<NodeIfPred>(elif);
            return pred;
        }
        if (try_consume(TokenType::else_)){
            auto else_ = m_allocator.alloc<NodeIfPredElse>();
            if (const auto scope = parse_scope()){
                else_->scope = scope.value();
            }
            else{
                error_expected("scope");
            }
            auto pred = m_allocator.emplace<NodeIfPred>(else_);
            return pred;
        }
        return {};
    }

    /**
     * @brief Parses a single statement.
     * @return The parsed statement node, or std::nullopt if no statement is found.
     *
     * Supported statement types:
     * - gives <expr>; (exit/return)
     * - we_haves <name> = <expr>; (variable declaration)
     * - <name> = <expr>; (variable assignment)
     * - { ... } (scoped block)
     * - if (<expr>) { ... } [elif ...] [else ...]
     */
    std::optional<NodeStmt*> parse_stmt() {
        // gives <expr>;
        if (peek().has_value() && peek().value().type == TokenType::exit && peek(1).has_value() &&
            peek(1).value().type == TokenType::open_paren) {
            consume();
            consume();
            auto stmt_exit = m_allocator.alloc<NodeStmtExit>();
            if (const auto node_expr = parse_expr()) {
                stmt_exit->expr = node_expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_exit);
            return stmt;
        }


        // we_haves <ident> = <expr>;
        if (peek().has_value() && peek().value().type == TokenType::let &&
                   peek(1).has_value() && peek(1).value().type == TokenType::ident &&
                   peek(2).has_value() && peek(2).value().type == TokenType::eq) {
            consume();
            auto stmt_let = m_allocator.alloc<NodeStmtLet>();
            stmt_let->ident = consume();
            consume();
            if (const auto expr = parse_expr()) {
                stmt_let->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_let);
            return stmt;
        }

        // <ident> = <expr>;
        if (peek().has_value()&&peek().value().type == TokenType::ident && peek(1).has_value() && peek(1).value().type==TokenType::eq){
            
            const auto assign = m_allocator.alloc<NodeStmtAssign>();
            assign->ident=consume();
            consume();
            if (auto expr = parse_expr()){
                assign->expr=expr.value();

            }else{
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(assign);
            return stmt;
        }

        // { ... } (scoped block)
        if (peek().has_value() && peek().value().type == TokenType::open_curly) {
            if (auto scope = parse_scope()) {
                auto stmt = m_allocator.emplace<NodeStmt>(scope.value());
                return stmt;
            }
            error_expected("scope");
        }
        // if (<expr>) { ... } [elif ...] [else ...]
        if (auto if_ = try_consume(TokenType::if_)) {
            try_consume_err(TokenType::open_paren);
            auto stmt_if = m_allocator.alloc<NodeStmtIf>();
            if (const auto expr = parse_expr()) {
                stmt_if->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (const auto scope = parse_scope()) {
                stmt_if->scope = scope.value();
            } else {
                error_expected("scope");
            }
            stmt_if->pred = parse_if_pred();
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_if);
            return stmt;
        }
        return {};
    }

    /**
     * @brief Parses an entire program (sequence of top-level statements).
     * @return The parsed program node, or std::nullopt if parsing fails.
     *
     * Continuously parses statements until the token stream is exhausted.
     */
    std::optional<NodeProg> parse_prog() {
        NodeProg prog;
        while (peek().has_value()) {
            if (auto stmt = parse_stmt()) {
                prog.stmts.push_back(stmt.value());
            } else {
                error_expected("statement");
            }
        }
        return prog;
    }

private:
    /**
     * @brief Peeks at a token at the given offset from current position.
     * @param offset Number of tokens ahead to peek (default 0 = current).
     * @return The token at that position, or std::nullopt if out of bounds.
     */
    [[nodiscard]] std::optional<Token> peek(const int offset = 0) const {
        if (m_index + offset < 0 || m_index + offset >= m_tokens.size()) {
            return {};
        }
        return m_tokens.at(m_index+offset);
    }

    /**
     * @brief Consumes and returns the current token, advancing the position.
     * @return The consumed token.
     */
    Token consume() { return m_tokens.at(m_index++); }

    /**
     * @brief Attempts to consume a token of the expected type, or reports an error.
     * @param type The expected token type.
     * @return The consumed token if it matched, otherwise exits with an error.
     */
    Token try_consume_err(TokenType type) {
        if (peek().has_value() && peek().value().type == type) {
            return consume();
        }
        error_expected(to_string(type));
        return {};
    }

    /**
     * @brief Attempts to consume a token of the expected type without error.
     * @param type The expected token type.
     * @return The consumed token if it matched, or std::nullopt if not.
     */
    std::optional<Token> try_consume(TokenType type) {
        if (peek().has_value() && peek().value().type == type) {
            return consume();
        }
        return {};
    }

    const std::vector<Token> m_tokens;  ///< The input token stream.
    size_t m_index = 0;                 ///< Current position in the token stream.
    ArenaAllocator m_allocator;         ///< Arena allocator for AST node memory.
};