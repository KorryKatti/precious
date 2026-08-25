#pragma once

/**
 * @file parser.hpp
 * @brief Recursive descent parser for the Precious programming language.
 *
 * Converts a token stream into an AST (see ast.hpp for node definitions).
 * Uses operator precedence climbing for expressions and an ArenaAllocator
 * for all AST node allocations.
 *
 * Implementation files:
 *   - parse_util.hpp  — token stream utilities (peek, consume, etc.)
 *   - parse_expr.hpp  — expression and term parsing
 *   - parse_stmt.hpp  — statement and scope parsing
 */

#include <cassert>
#include <cstdlib>
#include <optional>
#include <vector>

#include "./arena.hpp"
#include "ast.hpp"
#include "tokenization.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens)), m_allocator(1024 * 1024 * 4) {}

    // Utility methods
    void error_expected(const std::string& msg) const;
    std::optional<Token> peek(const int offset = 0) const;
    Token consume();
    Token try_consume_err(TokenType type);
    std::optional<Token> try_consume(TokenType type);

    // Expression parsing
    std::optional<NodeTerm*> parse_term();
    std::optional<NodeExpr*> parse_expr(const int min_prec = 0);
    NodeTermFnCall* parse_fn_call();

    // Statement parsing
    std::optional<NodeScope*> parse_scope();
    std::optional<NodeIfPred*> parse_if_pred();
    std::optional<NodeStmt*> parse_stmt();

    // Program parsing
    //
    // Strict top level: only `fn` definitions are allowed, and exactly one of
    // them must be `fn the_precious() { ... }` — the program entry point
    // (Precious' equivalent of C's `main`). Imported files later on will just
    // be fn libraries without an entry.
    std::optional<NodeProg> parse_prog() {
        NodeProg prog;
        while (peek().has_value()) {
            if (auto stmt = parse_stmt()) {
                if (!std::holds_alternative<NodeStmtFn*>(stmt.value()->var)) {
                    std::cerr << "[ERROR] Only 'fn' definitions may live at top level! Move this inside 'fn the_precious()' (line "
                              << peek(-1).value().line << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
                auto fn = std::get<NodeStmtFn*>(stmt.value()->var);
                if (fn->name.value.value() == "the_precious") {
                    if (prog.entry_fn != nullptr) {
                        std::cerr << "[ERROR] There can be only one precious! Duplicate 'fn the_precious' (line "
                                  << fn->name.line << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    if (!fn->params.empty()) {
                        std::cerr << "[ERROR] The precious takes no arguments! Remove the parameters from 'fn the_precious' (line "
                                  << fn->name.line << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    if (fn->return_type.has_value()) {
                        std::cerr << "[ERROR] The precious needs no return type! Remove '-> "
                                  << fn->return_type.value() << "' from 'fn the_precious' (line "
                                  << fn->name.line << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    prog.entry_fn = fn;
                }
                prog.stmts.push_back(stmt.value());
            } else {
                error_expected("statement");
            }
        }
        if (prog.entry_fn == nullptr) {
            std::cerr << "[ERROR] Where is the precious?! Every program needs an entry point: 'fn the_precious() { ... }'" << std::endl;
            exit(EXIT_FAILURE);
        }
        return prog;
    }

private:
    const std::vector<Token> m_tokens;
    size_t m_index = 0;
    ArenaAllocator m_allocator;
};

// Include implementations in dependency order
#include "parse_util.hpp"
#include "parse_expr.hpp"
#include "parse_stmt.hpp"
