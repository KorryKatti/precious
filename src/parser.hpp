#pragma once

/**
 * @file parser.hpp
 * @brief Recursive descent parser for the Precious programming language.
 *
 * Converts a token stream into an AST (see ast.hpp for node definitions).
 * Uses operator precedence climbing for expressions and an ArenaAllocator
 * for all AST node allocations.
 */

#include <cassert>
#include <cstdlib>
#include <optional>
#include <vector>

#include "./arena.hpp"
#include "ast.hpp"

class Parser {
public:
    /**
     * @brief Constructs a parser with the given token stream.
     * @param tokens The vector of tokens to parse.
     *
     * Initializes the arena allocator with 4 MB of memory for AST nodes.
     */
    explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens)), m_allocator(1024 * 1024 * 4) {}

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
        // negative handling
        if (peek().has_value() && peek().value().type == TokenType::minus) {
            consume();  // consume the '-'
            auto expr = parse_expr(5);  // parse the expression after the unary minus
            if (!expr.has_value()) {
                error_expected("expression after unary minus");
            }
            auto term_unary_minus = m_allocator.emplace<NodeTermUnaryMinus>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_unary_minus);
            return term;
        }

        if (peek().has_value() && peek().value().type == TokenType::ident) {
            // Could be a variable, function call, or array index — peek ahead to decide
            if (peek(1).has_value() && peek(1).value().type == TokenType::open_paren) {
                auto fn_call = parse_fn_call();
                auto term = m_allocator.emplace<NodeTerm>(fn_call);
                return term;
            }
            // Array index access: ident[expr]
            // Example: arr[0], arr[i+1], arr[myFunc()]
            // Lookahead: peek(1) checks if '[' follows the identifier
            if (peek(1).has_value() && peek(1).value().type == TokenType::open_square) {
                auto arr_index = m_allocator.emplace<NodeTermArrayIndex>();
                auto ident_token = consume();  // consume the array name (e.g., 'arr')
                // Create NodeTermIdent, wrap in NodeTerm, then wrap in NodeExpr
                auto ident_ident = m_allocator.emplace<NodeTermIdent>(ident_token);
                auto ident_term = m_allocator.emplace<NodeTerm>(ident_ident);
                arr_index->ident = m_allocator.emplace<NodeExpr>(ident_term);
                consume();  // consume '['

                // Parse the index expression (can be any expression: literal, variable, arithmetic)
                if (auto index_expr = parse_expr()) {
                    arr_index->index = index_expr.value();
                } else {
                    error_expected("index expression");
                }

                // Expect closing ']'
                if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                    error_expected("']'");
                }
                consume();  // consume ']'
                auto term = m_allocator.emplace<NodeTerm>(arr_index);
                return term;
            }
            // Normal variable
            auto expr_ident = m_allocator.emplace<NodeTermIdent>(consume());
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
            auto expr = parse_expr(5);
            if (!expr.has_value()) {
                error_expected("expression");
            }
            auto term_not = m_allocator.emplace<NodeTermNot>(expr.value());
            auto term = m_allocator.emplace<NodeTerm>(term_not);
            return term;
        }
        if (auto string_lit = try_consume(TokenType::string_lit)) {
            auto term_string_lit = m_allocator.emplace<NodeTermStringLit>(string_lit.value());
            auto term = m_allocator.emplace<NodeTerm>(term_string_lit);
            return term;
        }
        // array literal: [expr, expr, ...]
        if (peek().has_value() && peek().value().type == TokenType::open_square) {
            auto arr_lit = m_allocator.emplace<NodeTermArrayLit>();
            consume();  // consume '['
            while (peek().has_value() && peek().value().type != TokenType::close_square) {
                if (auto expr = parse_expr()) {
                    arr_lit->elements.push_back(expr.value());
                } else {
                    error_expected("expression in array literal");
                }
                if (peek().has_value() && peek().value().type == TokenType::comma_) {
                    consume();  // consume ','
                }
            }
            if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                error_expected("closing square bracket for array literal");
            }
            consume();  // consume ']'
            auto term = m_allocator.emplace<NodeTerm>(arr_lit);
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

            BinOp op;
            switch (type) {
                case TokenType::plus:   op = BinOp::Add; break;
                case TokenType::minus:  op = BinOp::Sub; break;
                case TokenType::star:   op = BinOp::Mul; break;
                case TokenType::fslash: op = BinOp::Div; break;
                case TokenType::eqeq:   op = BinOp::Eq; break;
                case TokenType::noteq:  op = BinOp::NotEq; break;
                case TokenType::lt:     op = BinOp::Lt; break;
                case TokenType::gt:     op = BinOp::Gt; break;
                case TokenType::lteq:   op = BinOp::LtEq; break;
                case TokenType::gteq:   op = BinOp::GtEq; break;
                case TokenType::and_:   op = BinOp::And; break;
                case TokenType::or_:    op = BinOp::Or; break;
                default: assert(false);
            }
            auto expr_lhs2 = m_allocator.emplace<NodeExpr>();
            expr_lhs2->var = expr_lhs->var;
            auto bin = m_allocator.emplace<NodeBinExpr>(op, expr_lhs2, expr_rhs.value());
            expr_lhs->var = bin;
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
    std::optional<NodeIfPred*> parse_if_pred() {
        if (try_consume(TokenType::elif)) {
            try_consume_err(TokenType::open_paren);
            const auto elif = m_allocator.alloc<NodeIfPredElif>();
            if (auto expr = parse_expr()) {
                elif->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (auto scope = parse_scope()) {
                elif->scope = scope.value();
            } else {
                error_expected("scope");
            }
            elif->pred = parse_if_pred();
            auto pred = m_allocator.emplace<NodeIfPred>(elif);
            return pred;
        }
        if (try_consume(TokenType::else_)) {
            auto else_ = m_allocator.alloc<NodeIfPredElse>();
            if (const auto scope = parse_scope()) {
                else_->scope = scope.value();
            } else {
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
     * - my <name> = <expr>; (variable declaration)
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

        // my <ident> [: type] = <expr>;
        // Also handles array types: my <ident>: type[] = <expr>; or my <ident>: type[size] = <expr>;
        if (peek().has_value() && peek().value().type == TokenType::let && peek(1).has_value() &&
            peek(1).value().type == TokenType::ident && peek(2).has_value() &&
            (peek(2).value().type == TokenType::eq || peek(2).value().type == TokenType::colon_)) {
            consume();
            auto stmt_let = m_allocator.alloc<NodeStmtLet>();
            stmt_let->ident = consume();
            // type check
            if (peek().has_value() && peek().value().type == TokenType::colon_) {
                consume();  // consume the ':'
                // next token must be a type keyword
                if (peek().has_value() && (peek().value().type == TokenType::type_number_ ||
                                           peek().value().type == TokenType::type_word_ ||
                                           peek().value().type == TokenType::type_question_ ||
                                           peek().value().type == TokenType::type_decimal_ ||
                                           peek().value().type == TokenType::type_letter)) {
                    stmt_let->type_annotation = consume().type;
                    
                    // Check for array type suffix: type[] or type[size]
                    if (peek().has_value() && peek().value().type == TokenType::open_square) {
                        consume();  // consume '['
                        stmt_let->is_array = true;
                        
                        // Check if dynamic array (type[]) or fixed-size (type[5])
                        if (peek().has_value() && peek().value().type == TokenType::close_square) {
                            consume();  // consume ']' — dynamic array
                        } else if (peek().has_value() && peek().value().type == TokenType::int_lit) {
                            stmt_let->array_size = consume();  // consume the size (e.g., '5')
                            // expect ']'
                            if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                                error_expected("']'");
                            }
                            consume();  // consume ']'
                        } else {
                            error_expected("']' or array size");
                        }
                    }
                } else {
                    error_expected("type annotation (number, word, question, decimal, letter)");
                }
            } else {
                stmt_let->type_annotation = std::nullopt;  // no type annotation
            }

            consume();  // consumes the '='
            if (const auto expr = parse_expr()) {
                stmt_let->expr = expr.value();
                // Strict rule: array literals MUST have explicit type annotation
                if (std::holds_alternative<NodeTerm*>(expr.value()->var)) {
                    auto term = std::get<NodeTerm*>(expr.value()->var);
                    if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
                        if (!stmt_let->type_annotation.has_value()) {
                            std::cerr << "[ERROR] Arrays need a type, precious! Use 'my "
                                      << stmt_let->ident.value.value() << ": number[] = ...' (line "
                                      << stmt_let->ident.line << ")" << std::endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_let);
            return stmt;
        }

        // Array assignment: ident[expr] = expr;
        // Example: arr[0] = 99; arr[i] = x + 1;
        // Must come BEFORE normal assignment (x = expr;) because we check for '[' after ident
        // Lookahead: peek(0) = ident, peek(1) = '['
        if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
            peek(1).value().type == TokenType::open_square) {
            auto arr_assign = m_allocator.emplace<NodeStmtArrayAssign>();
            arr_assign->ident = consume();  // consume the array name (e.g., 'arr')

            consume();  // consume '['
            // Parse the index expression (which element to assign to)
            if (auto index_expr = parse_expr()) {
                arr_assign->index = index_expr.value();
            } else {
                error_expected("index expression");
            }

            // Expect closing ']'
            if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                error_expected("']'");
            }
            consume();  // consume ']'

            // Expect '=' assignment operator
            if (!peek().has_value() || peek().value().type != TokenType::eq) {
                error_expected("'='");
            }
            consume();  // consume '='

            // Parse the value expression (what to assign)
            if (auto expr = parse_expr()) {
                arr_assign->expr = expr.value();
            } else {
                error_expected("expression");
            }

            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(arr_assign);
            return stmt;
        }

        // <ident> = <expr>;
        if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
            peek(1).value().type == TokenType::eq) {
            const auto assign = m_allocator.alloc<NodeStmtAssign>();
            assign->ident = consume();
            consume();
            if (auto expr = parse_expr()) {
                assign->expr = expr.value();

            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(assign);
            return stmt;
        }

        // <ident>(...); — function call as statement
        if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
            peek(1).value().type == TokenType::open_paren) {
            auto fn_call = parse_fn_call();
            try_consume_err(TokenType::semi);
            auto term = m_allocator.emplace<NodeTerm>(fn_call);
            auto expr = m_allocator.emplace<NodeExpr>(term);
            auto stmt_expr = m_allocator.emplace<NodeStmtExpr>(expr);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_expr);
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
        if (auto while_ = try_consume(TokenType::while_)) {
            try_consume_err(TokenType::open_paren);
            auto stmt_while = m_allocator.alloc<NodeStmtWhile>();
            if (const auto expr = parse_expr()) {
                stmt_while->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            if (const auto scope = parse_scope()) {
                stmt_while->scope = scope.value();
            } else {
                error_expected("scope");
            }
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_while);
            return stmt;
        }

        // should i pass arguments ? for now i guess not
        if (peek().has_value() && peek().value().type == TokenType::print_ && peek(1).has_value() &&
            peek(1).value().type == TokenType::open_paren) {
            consume();
            consume();
            auto stmt_print = m_allocator.alloc<NodeStmtPrint>();
            if (const auto expr = parse_expr()) {
                stmt_print->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::close_paren);
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_print);
            return stmt;
        }

        // function calling
        if (peek().has_value() && peek().value().type == TokenType::fn_) {
            auto fn_stmt = m_allocator.emplace<NodeStmtFn>();
            consume();  // consume 'fn'
            // expect ident (function name)
            if (!peek().has_value() || peek().value().type != TokenType::ident) {
                error_expected("function name");
            }
            fn_stmt->name = consume();
            // expect open paren
            if (!peek().has_value() || peek().value().type != TokenType::open_paren) {
                error_expected("open paren");
            }
            consume();  // consume '('

            // parse parameters
            while (peek().has_value() && peek().value().type != TokenType::close_paren) {
                if (peek().has_value() && peek().value().type == TokenType::ident) {
                    auto param = m_allocator.alloc<NodeFnParam>();
                    param->name = consume();
                    // optional type annotation on param
                    if (peek().has_value() && peek().value().type == TokenType::colon_) {
                        consume();  // consume ':'
                        if (peek().has_value() &&
                            (peek().value().type == TokenType::type_number_ ||
                             peek().value().type == TokenType::type_word_ ||
                             peek().value().type == TokenType::type_question_ ||
                             peek().value().type == TokenType::type_decimal_ ||
                             peek().value().type == TokenType::type_letter)) {
                            param->type_annotation = consume().type;
                        } else {
                            error_expected(
                                "type annotation (number, word, question, decimal, letter)");
                        }
                    } else {
                        param->type_annotation = std::nullopt;
                    }
                    fn_stmt->params.push_back(*param);
                }
                if (peek().has_value() && peek().value().type == TokenType::comma_) {
                    consume();
                }
            }
            consume();  // consume ')'
            // expect '{'
            auto body = parse_scope();
            if (!body.has_value()) {
                error_expected("function body");
            }
            fn_stmt->body = body.value();
            auto stmt = m_allocator.emplace<NodeStmt>(fn_stmt);
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
        return m_tokens.at(m_index + offset);
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

    // Parses: ident(args...) — expects ident and '(' already peeked
    // Used by parse_term() and parse_stmt() to avoid duplication
    NodeTermFnCall* parse_fn_call() {
        auto fn_call = m_allocator.emplace<NodeTermFnCall>();
        fn_call->name = consume();  // consume ident
        consume();                  // consume '('

        while (peek().has_value() && peek().value().type != TokenType::close_paren) {
            auto arg_expr = parse_expr();
            if (!arg_expr.has_value()) {
                error_expected("expression");
            }
            fn_call->args.push_back(arg_expr.value());

            if (peek().has_value() && peek().value().type == TokenType::comma_) {
                consume();
            } else {
                break;
            }
        }

        try_consume_err(TokenType::close_paren);
        return fn_call;
    }

    const std::vector<Token> m_tokens;  ///< The input token stream.
    size_t m_index = 0;                 ///< Current position in the token stream.
    ArenaAllocator m_allocator;         ///< Arena allocator for AST node memory.
};