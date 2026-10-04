/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * Generates C++ source code from the AST. The output is compiled with g++.
 *
 * This file holds the Generator class and its data. The work is in the files it
 * includes at the bottom:
 *   - gen_type.hpp  -- Precious type -> C++ type, and type guessing
 *   - gen_expr.hpp  -- expressions: literals, calls, operators
 *   - gen_stmt.hpp  -- statements: one gen_xxx() per statement kind
 *   - gen_fn.hpp    -- functions, the entry point, and the whole program
 *
 * Why are those included at the *bottom*? Because they define Generator's member
 * functions, and a function body can only be written after the class itself is
 * declared. This is a normal C++ pattern, not something Precious-specific.
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
    Generator(NodeProg prog) : m_prog(std::move(prog)) {}

    // ---- Statements. One per statement kind, plus the dispatcher. ----
    void gen_stmt(const NodeStmt* stmt);
    void gen_gives(const NodeStmtExit* stmt);
    void gen_my(const NodeStmtLet* stmt);
    void gen_assign(const NodeStmtAssign* stmt);
    void gen_compound_assign(const NodeStmtCompoundAssign* stmt);
    void gen_block(const NodeScope* scope);
    void gen_if(const NodeStmtIf* stmt);
    void gen_while(const NodeStmtWhile* stmt);
    void gen_for(const NodeStmtFor* stmt);
    void gen_for_each(const NodeStmtForEach* stmt);
    void gen_say(const NodeStmtPrint* stmt);
    void gen_expr_stmt(const NodeStmtExpr* stmt);
    void gen_array_assign(const NodeStmtArrayAssign* stmt);
    void gen_break();
    void gen_continue();
    void gen_switch(const NodeStmtSwitch* stmt);
    void gen_push(const NodeStmtPush* stmt);
    void gen_pop(const NodeStmtPop* stmt);

    // ---- Blocks and control-flow chains ----
    void gen_scope(const NodeScope* scope, bool inline_brace = false);
    void gen_if_pred(const NodeIfPred* pred);

    // ---- Expressions ----
    void gen_term(const NodeTerm* term);
    void gen_bin_expr(const NodeBinExpr* bin_expr);
    void gen_expr(const NodeExpr* expr);

    // ---- Functions and the whole program ----
    void gen_fn_def(const NodeStmtFn* fn, std::stringstream& out);
    void gen_ask(const NodeTermFnCall* fn_call);
    std::string gen_prog();

private:
    // ---- Function signature helpers ----

    // The C++ type of one parameter, including the std::vector<>& wrapper for
    // array parameters.
    std::string param_c_type(const NodeFnParam& param) const;

    // The C++ return type of a whole function.
    std::string fn_return_c_type(const NodeStmtFn* fn) const;

    // Writes "name(param, param)" with no semicolon or newline, so the same
    // code can produce both a forward declaration and a definition.
    void write_signature(std::stringstream& out, const NodeStmtFn* fn) const;
    // ---- Type helpers ----

    // "number" -> "long", "word" -> "std::string", and so on.
    std::string resolve_type(TokenType type) const;

    // "std::vector<long>&" -> "long". Non-arrays come back unchanged.
    std::string vector_element_type(const std::string& type) const;

    // Best guess at the C++ type an expression produces.
    std::string infer_type(const NodeExpr* expr) const;

    // True if this function body contains a `gives`.
    bool has_return(const NodeScope* body) const;
    bool has_return_pred(const NodeIfPred* pred) const;

    // True if this expression is exactly a call to the `ask` built-in.
    bool is_ask_call(const NodeExpr* expr) const;

    // True if `name` should be treated as the `ask` built-in. A program that
    // defines its own `fn ask(...)` wins.
    bool is_builtin_ask(const std::string& name) const;

    // "+" for +=, "*" for *=, and so on. Used to print compound assignments.
    std::string compound_op_symbol(TokenType op) const;

    // Writes `expr` out. If `ask_type` is not empty it tells a nested `ask`
    // call which C++ type to read into, then puts the previous setting back.
    void gen_with_ask_type(const std::string& ask_type, const NodeExpr* expr);

    // ---- The parsed program ----
    const NodeProg m_prog;

    // Where the generated C++ is accumulated.
    std::stringstream m_output;

    // Names of variables currently in scope, in declaration order. Used to
    // catch a variable being redeclared in the same block.
    std::vector<std::string> m_declared;

    // One entry per open block, holding how many variables existed when that
    // block opened. gen_scope() uses it to forget the block's variables on exit.
    std::vector<size_t> m_declared_scopes;

    // Variable name -> C++ type, for everything currently in scope.
    std::unordered_map<std::string, std::string> m_var_types;

    // Function name -> C++ return type, for every function in the program.
    std::unordered_map<std::string, std::string> m_fn_return_types;

    // What C++ type the next `ask` call should read into. "std::string" reads a
    // whole line; any other type reads a single value. Call sites that know the
    // destination use gen_with_ask_type() to set it.
    std::string m_ask_type = "std::string";
};

// The implementations, included after the class so that the member functions
// above can see the class definition.
#include "gen_type.hpp"
#include "gen_expr.hpp"
#include "gen_stmt.hpp"
#include "gen_fn.hpp"