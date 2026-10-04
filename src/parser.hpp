#pragma once

/**
 * @file parser.hpp
 * @brief Recursive descent parser for the Precious programming language.
 *
 * Turns the token list from the tokenizer into an AST (see ast.hpp).
 *
 * The idea behind recursive descent is simple: there is one function per thing
 * in the grammar, and each one reads the tokens it expects, then calls the
 * function for whatever comes next. There is no table or grammar file.
 *
 * Where the work lives:
 *   - parse_util.hpp  -- moving along the token list (peek, consume, ...)
 *   - parse_expr.hpp  -- values and operators: 42, x, a + b * 2
 *   - parse_stmt.hpp  -- actions: my x = 1, if/else, for, fn, ...
 *
 * Those three are included at the *bottom* of this file because they define
 * Parser's member functions, and a function body can only be written after the
 * class itself has been declared.
 *
 * Memory: every AST node is created with plain `new` and never deleted. The
 * compiler is a short-lived process, so there is no cleanup to get wrong.
 */

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "ast.hpp"
#include "tokenization.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

    // ---- Moving along the token list (see parse_util.hpp) ----

    // [[noreturn]] says error_expected() never comes back, so the compiler stops
    // warning about functions that would otherwise fall off the end after an
    // error, and we don't need a dead `return {};` after every call.
    [[noreturn]] void error_expected(const std::string& msg) const;
    std::optional<Token> peek(const int offset = 0) const;
    Token consume();
    std::optional<Token> try_consume(TokenType type);
    Token try_consume_err(TokenType type);

    // ---- Expressions (see parse_expr.hpp) ----

    std::optional<NodeTerm*> parse_term();          // one value: 42, x, f(1)
    std::optional<NodeExpr*> parse_expr(int min_prec = 0); // values + operators
    NodeTermFnCall* parse_fn_call();                 // name(a, b)

    // ---- Statements (see parse_stmt.hpp) ----

    std::optional<NodeScope*> parse_scope();   // { ... }
    std::optional<NodeIfPred*> parse_if_pred(); // the elif/else tail of an if

    // parse_stmt() is only a dispatcher. Each statement form has its own
    // function, named after the Precious syntax it parses. A parse_xxx()
    // function either parses that one form and returns it, or -- if the next
    // tokens turn out to be something else -- returns nothing and consumes
    // nothing, so parse_stmt() can simply try them in order.
    //
    // The order matters: some forms start with the same token as another
    // (`my x = 1` and `x = 1` both start with a name), so the more specific one
    // has to be tried first.
    std::optional<NodeStmt*> parse_stmt();
    std::optional<NodeStmt*> parse_gives();           // gives(expr);
    std::optional<NodeStmt*> parse_my();              // my x[: type] = expr;
    std::optional<NodeStmt*> parse_array_assign();    // arr[i] = expr;
    std::optional<NodeStmt*> parse_compound_assign(); // x += expr;
    std::optional<NodeStmt*> parse_incdec_stmt();     // ++x;  x--;
    std::optional<NodeStmt*> parse_assign();          // x = expr;
    std::optional<NodeStmt*> parse_call_stmt();       // my_fn(a, b);
    std::optional<NodeStmt*> parse_block();           // { ... }
    std::optional<NodeStmt*> parse_if();              // if / elif / else
    std::optional<NodeStmt*> parse_while();           // while (cond) { ... }
    std::optional<NodeStmt*> parse_for();             // for (init; cond; upd) { }
    std::optional<NodeStmt*> parse_for_each();        // for (x in arr) { }
    std::optional<NodeStmt*> parse_switch();          // switch (v) { case ... }
    std::optional<NodeStmt*> parse_say();             // say(expr);
    std::optional<NodeStmt*> parse_break();           // break;
    std::optional<NodeStmt*> parse_continue();        // continue;
    std::optional<NodeStmt*> parse_push();            // push arr, x;
    std::optional<NodeStmt*> parse_pop();             // pop arr;
    std::optional<NodeStmt*> parse_fn();              // fn name(p) { }

    // The pieces of the two biggest forms, split out so those functions stay
    // readable.
    void parse_for_init(NodeStmtFor* stmt_for);
    void parse_for_update(NodeStmtFor* stmt_for);
    void parse_fn_params(NodeStmtFn* fn_stmt);
    void parse_fn_return_type(NodeStmtFn* fn_stmt);

    // Two token tests that would otherwise be written out longhand in several
    // places. Adding a type means editing is_type_token() and nowhere else.
    static bool is_type_token(TokenType type);
    bool peek_is_type(int offset = 0) const;
    static bool is_compound_assign_token(TokenType type);
    bool peek_is_compound_assign(int offset = 0) const;

    // ---- The whole program ----

    // Reads every top-level definition.
    //
    // The top level is strict: only `fn` definitions are allowed there, and
    // exactly one of them must be `fn the_precious() { ... }` -- the entry
    // point, Precious's equivalent of C's main. (Imported files, once those
    // exist, will be fn libraries with no entry.)
    std::optional<NodeProg> parse_prog();

    // Checks one `fn` against the entry-point rules: it must be called
    // the_precious, must not be a second one, and must take no arguments and
    // declare no return type. Records it on the program if it passes.
    void register_entry_fn(NodeProg* prog, NodeStmtFn* fn);

private:
    const std::vector<Token> m_tokens; ///< Every token in the file.
    size_t m_index = 0;                ///< Which token we are looking at.
};

// The implementations, included after the class so the member functions above
// can see the class definition.
#include "parse_util.hpp"
#include "parse_expr.hpp"
#include "parse_stmt.hpp"
