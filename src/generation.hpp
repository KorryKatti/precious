/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * Generates C source code from the AST. The output is compiled with gcc.
 *
 * This file defines the Generator class and includes implementation files:
 *   - gen_type.hpp  — type resolution and inference
 *   - gen_expr.hpp  — expression code generation
 *   - gen_stmt.hpp  — statement code generation
 *   - gen_fn.hpp    — function and program code generation
 */

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ast.hpp"

class Generator {
public:
    inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {}

    // Method declarations — implementations in gen_*.hpp files below
    void gen_term(const NodeTerm* term);
    void gen_bin_expr(const NodeBinExpr* bin_expr);
    void gen_expr(const NodeExpr* expr);
    void gen_scope(const NodeScope* scope, bool inline_brace = false);
    void gen_if_pred(const NodeIfPred* pred, const std::string& end_label);
    void gen_stmt(const NodeStmt* stmt);
    void gen_fn_def(const NodeStmtFn* fn, std::stringstream& out);
    [[nodiscard]] std::string gen_prog();

private:
    // Type helpers
    std::string resolve_type(TokenType type) const;
    std::string resolve_type(const std::string& type) const;
    std::string infer_type(const NodeExpr* expr) const;
    std::string infer_return_type(const NodeScope* body) const;
    std::string infer_return_type_pred(const NodeIfPred* pred) const;
    bool is_string_expr(const NodeExpr* expr) const;
    bool has_return(const NodeScope* body) const;
    bool has_return_pred(const NodeIfPred* pred) const;

    const NodeProg m_prog;
    std::stringstream m_output;
    int m_if_count = 0;
    std::vector<std::string> m_declared;
    std::vector<size_t> m_declared_scopes;
    std::unordered_map<std::string, std::string> m_var_types;
    std::unordered_set<std::string> m_array_params;
    std::unordered_map<std::string, std::string> m_fn_return_types;
    std::unordered_map<std::string, int> m_array_sizes; ///< Track array sizes for for-each
};

// Include implementations in dependency order
#include "gen_type.hpp"
#include "gen_expr.hpp"
#include "gen_stmt.hpp"
#include "gen_fn.hpp"
