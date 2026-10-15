#pragma once

/**
 * @file typechecker.hpp
 * @brief Checks the AST for type mistakes, before any C++ is generated.
 *
 * Runs between the parser and the code generator (see main.cpp). It answers one
 * question about every expression -- "what type is this?" -- and then asks
 * whether that type is allowed in the place it is being used.
 *
 * A type is one of the five Precious type names: "number", "decimal", "letter",
 * "word", "question". An array adds "[]" on the end, so an array of words is
 * "word[]". An empty string means "we cannot tell", and that is always allowed:
 * the checker stays quiet when it has nothing to say.
 *
 * Where the work lives:
 *   - check_expr.hpp  -- what type does an expression produce?
 *   - check_stmt.hpp  -- one check_xxx() per statement kind, and the signatures
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

class TypeChecker {
public:
    explicit TypeChecker(const NodeProg& prog) : m_prog(prog) {}

    // Checks the whole program. Every mistake is printed as it is found, and this
    // returns false if there was at least one.
    bool check();

private:
    // ---- Types, as text ----

    // type_word_ -> "word". Returns "" for a token that is not a type.
    static std::string type_name(const TokenType type);

    // number, decimal, letter and question all hold numbers, so arithmetic and
    // conditions accept all four. An empty type counts as a number too, because
    // not knowing is never an error.
    static bool is_number(const std::string& type);

    // True for "word[]" and every other array type.
    static bool is_array(const std::string& type);

    // "word[]" -> "word". A type that is not an array comes back unchanged.
    static std::string element_type(const std::string& type);

    // May a value of type `given` be put where a `wanted` is expected?
    static bool fits(const std::string& wanted, const std::string& given);

    // The type of one function parameter. One written without a `: type` is a
    // number, which is what the code generator makes of it as well.
    static std::string param_type(const NodeFnParam& param);

    // The symbol for an arithmetic operator, for error messages.
    static std::string bin_op_name(const BinOp op);

    // ---- What type does this expression produce? ----

    std::string expr_type(const NodeExpr* expr);
    std::string term_type(const NodeTerm* term);
    std::string bin_type(const NodeBinExpr* bin_expr);
    std::string array_lit_type(const NodeTermArrayLit* array_lit);
    std::string call_type(const NodeTermFnCall* fn_call);
    std::string builtin_call_type(const std::string& name, const NodeTermFnCall* fn_call);

    // if (...) / while (...) / for (...) want something that is true or false.
    void check_condition(const NodeExpr* expr);

    // Which source line an expression starts on, for error messages. It only has
    // to be close: it digs through the wrappers until it finds a real token.
    int expr_line(const NodeExpr* expr);

    // ---- One function per statement kind, plus the dispatcher ----

    void check_stmt(const NodeStmt* stmt);
    void check_block(const NodeScope* scope);
    void check_fn(const NodeStmtFn* fn);
    void check_gives(const NodeStmtExit* stmt);
    void check_my(const NodeStmtLet* stmt);
    void check_assign(const NodeStmtAssign* stmt);
    void check_compound_assign(const NodeStmtCompoundAssign* stmt);
    void check_array_assign(const NodeStmtArrayAssign* stmt);
    void check_if(const NodeStmtIf* stmt);
    void check_if_pred(const NodeIfPred* pred);
    void check_while(const NodeStmtWhile* stmt);
    void check_for(const NodeStmtFor* stmt);
    void check_for_each(const NodeStmtForEach* stmt);
    void check_switch(const NodeStmtSwitch* stmt);
    void check_push(const NodeStmtPush* stmt);
    void check_pop(const NodeStmtPop* stmt);
    void check_say(const NodeStmtPrint* stmt);

    // ---- What every function looks like ----

    struct FnSignature {
        std::vector<std::string> params;  ///< the type of each parameter
        std::string returns;              ///< "" when the fn wrote no `-> type`
    };

    // Fills m_fns before anything is checked, so a function can be called from
    // a line above the one where it is defined.
    void collect_fn_signatures();

    // ---- Which blocks are we inside? ----
    //
    // m_scopes is a stack. The entry at the back is the block we are in right
    // now; the entries before it are the blocks around it.

    void enter_scope();
    void exit_scope();
    void declare_var(const std::string& name, const std::string& type);
    std::string lookup_var(const std::string& name);

    // ---- Saying what is wrong ----

    void error(const int line, const std::string& msg);

    // The one rule behind `my x: T = e`, `x = e`, `arr[i] = e` and `push arr, e`:
    // does this value belong in that place? Pass an empty name for a place that
    // has no name of its own.
    void check_put(const std::string& wanted, const std::string& given, const int line,
                   const std::string& name);

    // ---- What we know ----

    const NodeProg& m_prog;  ///< The program being checked.
    int m_error_count = 0;   ///< How many mistakes we have printed so far.

    /// One entry per open block: variable name -> type, for that block only.
    std::vector<std::unordered_map<std::string, std::string>> m_scopes;

    /// Function name -> what it takes and what it returns.
    std::unordered_map<std::string, FnSignature> m_fns;

    /// The function being checked right now.
    std::string m_fn_name;
    std::string m_fn_return;  ///< what it promises to give, "" if it promises nothing
    bool m_fn_gives = false;  ///< whether a `gives` has been seen in it
};

// The implementations, included after the class so the member functions above can
// see the class definition.
#include "check_expr.hpp"
#include "check_stmt.hpp"
