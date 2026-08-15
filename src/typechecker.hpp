#pragma once

/**
 * @file typechecker.hpp
 * @brief Semantic type checker for the Precious programming language.
 *
 * Sits between parsing and code generation. Walks the AST, builds a type
 * map for all variables and functions, and validates that operations are
 * type-safe.
 *
 * Two-pass approach:
 *   1. Collect all function signatures (name → param types + return type)
 *   2. Walk statements and validate types
 *
 * Usage:
 *   TypeChecker checker(prog);
 *   if (!checker.check()) {
 *       // errors were printed, exit before codegen
 *   }
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"
#include "tokenization.hpp"

class TypeChecker {
public:
    /**
     * @brief Constructs a type checker for the given AST program.
     * @param prog The parsed program to check.
     */
    explicit TypeChecker(const NodeProg& prog) : m_prog(prog) {}

    /**
     * @brief Runs the full type checking pipeline.
     * @return true if the program is type-safe, false if errors were found.
     *
     * Steps:
     *   1. Collect function signatures
     *   2. Check all statements
     */
    bool check() {
        collect_fn_signatures();
        return check_prog();
    }

private:
    // ── Pass 1: Collect function signatures ──────────────────────────

    /**
     * @brief First pass — builds m_fn_signatures from all function definitions.
     *
     * Walks m_prog.stmts, finds every NodeStmtFn, and records:
     *   - parameter types (explicit annotation or default "long")
     *   - return type (explicit annotation, inferred from gives, or "void")
     *
     * This must run before check_prog() so that function calls can be
     * validated even when the call appears before the definition.
     */
    void collect_fn_signatures();

    // ── Pass 2: Validate statements ──────────────────────────────────

    /**
     * @brief Checks all top-level statements in the program.
     * @return true if all statements are type-safe.
     */
    bool check_prog();

    /**
     * @brief Checks a single statement for type correctness.
     * @param stmt The statement node to validate.
     * @return true if the statement is valid.
     *
     * Dispatches based on statement variant:
     *   - NodeStmtLet: validate declared type matches expression type
     *   - NodeStmtAssign: validate RHS matches LHS declared type
     *   - NodeStmtIf/While/Switch: validate condition is a number
     *   - NodeStmtPrint: validate expression is printable
     *   - NodeStmtExit: validate return type matches function signature
     *   - NodeStmtFn: check the function body
     *   - NodeStmtArrayAssign: validate index is number, value matches element type
     *   - NodeScope: enter new scope, check stmts, exit scope
     */
    bool check_stmt(const NodeStmt* stmt);

    /**
     * @brief Checks a scoped block, managing scope entry/exit.
     * @param scope The scope node to validate.
     * @return true if all statements in the scope are valid.
     */
    bool check_scope(const NodeScope* scope);

    /**
     * @brief Checks an if/elif/else chain.
     * @param pred The predicate chain to validate.
     * @return true if all branches are valid.
     */
    bool check_if_pred(const NodeIfPred* pred);

    // ── Expression type queries ──────────────────────────────────────

    /**
     * @brief Returns the type of an expression as a string.
     * @param expr The expression to evaluate.
     * @return The type string ("long", "const char*", "char", "void", etc.)
     *
     * Resolves types for:
     *   - Integer literals → "long"
     *   - String literals → "const char*"
     *   - Identifiers → looked up in m_var_types
     *   - Function calls → looked up in m_fn_signatures return type
     *   - Binary expressions → "long" for arithmetic, "long" for comparison
     *   - Array indexing → element type of the array
     *   - Unary minus → same as operand
     *   - Logical NOT → "long"
     */
    std::string expr_type(const NodeExpr* expr) const;

    /**
     * @brief Returns the type of a term as a string.
     * @param term The term to evaluate.
     * @return The type string.
     */
    std::string term_type(const NodeTerm* term) const;

    /**
     * @brief Checks if a type is a numeric type.
     * @param type The type string to check.
     * @return true if the type is "long" or "double".
     */
    bool is_numeric(const std::string& type) const;

    /**
     * @brief Checks if a type is a string type.
     * @param type The type string to check.
     * @return true if the type is "const char*".
     */
    bool is_string(const std::string& type) const;

    /**
     * @brief Checks if a type is an array type.
     * @param type The type string to check.
     * @return true if the type ends with "*".
     */
    bool is_array(const std::string& type) const;

    // ── Scope management ─────────────────────────────────────────────

    /**
     * @brief Enters a new scope, pushing current size onto the scope stack.
     */
    void enter_scope();

    /**
     * @brief Exits the current scope, removing all variables declared in it.
     */
    void exit_scope();

    /**
     * @brief Declares a variable in the current scope.
     * @param name The variable name.
     * @param type The variable type string.
     * @return true if declaration succeeded, false if already declared in this scope.
     */
    bool declare_var(const std::string& name, const std::string& type);

    /**
     * @brief Looks up a variable's type, searching from innermost scope outward.
     * @param name The variable name to look up.
     * @return The type string if found, or empty string if not found.
     */
    std::string lookup_var(const std::string& name) const;

    // ── Error reporting ──────────────────────────────────────────────

    /**
     * @brief Reports a type error and marks the checker as failed.
     * @param line The line number where the error occurred.
     * @param msg The error message description.
     */
    void error(int line, const std::string& msg);

    // ── Member data ──────────────────────────────────────────────────

    const NodeProg& m_prog;                                        ///< The AST to check.
    bool m_has_errors = false;                                     ///< Set to true on first error.

    /// Variable type map: name → type string. Scopes tracked via m_scope_stack.
    std::unordered_map<std::string, std::string> m_var_types;

    /// Function signature map: name → { param_types, return_type }.
    struct FnSignature {
        std::vector<std::string> param_types;
        std::string return_type;
    };
    std::unordered_map<std::string, FnSignature> m_fn_signatures;

    /// Stack of scope boundaries — each entry is the m_var_types size at scope entry.
    std::vector<size_t> m_scope_stack;
};
