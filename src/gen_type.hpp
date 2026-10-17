/**
 * @file gen_type.hpp
 * @brief Type helpers for the Precious code generator.
 *
 * Three jobs live here:
 *   1. resolve_type()     -- a Precious type keyword becomes a C++ type
 *   2. infer_type()       -- guess the C++ type of an expression
 *   3. has_return()       -- does this function body contain a `gives`?
 *
 * Types are stored as C++ type strings ("long", "std::string", "double", "char")
 * because that is what eventually gets printed into the generated code.
 */

#include <string>

// ---------------------------------------------------------------------------
// Type resolution
// ---------------------------------------------------------------------------

// Turns one of the five Precious type keywords into the matching C++ type.
std::string Generator::resolve_type(const TokenType type) const {
    if (type == TokenType::type_number_) {
        return "long";
    }
    if (type == TokenType::type_word_) {
        return "std::string";
    }
    if (type == TokenType::type_question_) {
        return "long"; // booleans are 0 or 1
    }
    if (type == TokenType::type_decimal_) {
        return "double";
    }
    if (type == TokenType::type_letter) {
        return "char";
    }
    return "long"; // unknown: fall back to the default type
}

// Pulls the element type out of an array type string.
//   "std::vector<long>"      -> "long"
//   "std::vector<long>&"     -> "long"   (how an array parameter is stored)
// Anything that is not a vector comes back unchanged.
std::string Generator::vector_element_type(const std::string& type) const {
    const std::string prefix = "std::vector<";
    if (type.rfind(prefix, 0) != 0) {
        return type;
    }
    // Drop the "std::vector<" prefix, then the trailing ">" or ">&".
    std::string element = type.substr(prefix.size());
    if (!element.empty() && element.back() == '&') {
        element.pop_back();
    }
    if (!element.empty() && element.back() == '>') {
        element.pop_back();
    }
    return element;
}

// ---------------------------------------------------------------------------
// Type inference
// ---------------------------------------------------------------------------

// Works out the C++ type an expression produces. When there is no way to tell,
// it assumes "long", which is Precious's default number type.
std::string Generator::infer_type(const NodeExpr* expr) const {
    // --- a + b ---
    if (std::holds_alternative<NodeBinExpr*>(expr->var)) {
        auto bin = std::get<NodeBinExpr*>(expr->var);
        const std::string left = infer_type(bin->lhs);
        const std::string right = infer_type(bin->rhs);
        // Adding anything to a string gives a string. Everything else is
        // treated as arithmetic, which gives a number.
        if (left == "std::string" || right == "std::string") {
            return "std::string";
        }
        return "long";
    }

    // --- a single term ---
    if (!std::holds_alternative<NodeTerm*>(expr->var)) {
        return "long";
    }
    const NodeTerm* term = std::get<NodeTerm*>(expr->var);

    if (std::holds_alternative<NodeTermStringLit*>(term->var)) {
        return "std::string";
    }
    if (std::holds_alternative<NodeTermIntLit*>(term->var)) {
        return "long";
    }
    if (std::holds_alternative<NodeTermCharLit*>(term->var)) {
        return "char";
    }

    // [1, 2, 3] is a std::vector, provided every value in it agrees on a type.
    if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
        const NodeTermArrayLit* array_lit = std::get<NodeTermArrayLit*>(term->var);
        std::string element;
        for (const NodeExpr* value : array_lit->elements) {
            const std::string type = infer_type(value);
            if (element.empty()) {
                element = type;
            }
            if (type != element) {
                return "long"; // mixed, so we cannot tell
            }
        }
        if (element.empty()) {
            return "long"; // nothing in it to learn from
        }
        return "std::vector<" + element + ">";
    }

    // ++x, x++, --x and x-- all produce whatever x is.
    if (std::holds_alternative<NodeTermPreInc*>(term->var)) {
        return infer_type(std::get<NodeTermPreInc*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPostInc*>(term->var)) {
        return infer_type(std::get<NodeTermPostInc*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPreDec*>(term->var)) {
        return infer_type(std::get<NodeTermPreDec*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPostDec*>(term->var)) {
        return infer_type(std::get<NodeTermPostDec*>(term->var)->expr);
    }

    // A variable: whatever type we gave it when it was declared.
    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        const std::string name = std::get<NodeTermIdent*>(term->var)->ident.value.value();
        auto it = m_var_types.find(name);
        if (it != m_var_types.end()) {
            return it->second;
        }
    }

    // A function call: whatever type that function returns.
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        const std::string name =
            std::get<NodeTermFnCall*>(term->var)->name.value.value();
        if (is_builtin_ask(name)) {
            return "std::string"; // ask() defaults to reading a line
        }
        if (is_builtin_networking(name)) {
            return name == "http_get" ? "std::string" : "long";
        }
        if (is_builtin(name)) {
            if (name == "str") {
                return "std::string";
            }
            if (name == "decimal") {
                return "double";
            }
            return "long"; // len and num
        }
        auto it = m_fn_return_types.find(name);
        if (it != m_fn_return_types.end()) {
            return it->second;
        }
    }

    // arr[i] or s[i]: for an array this is the element type, for a string it is
    // a char.
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        auto array_index = std::get<NodeTermArrayIndex*>(term->var);
        if (std::holds_alternative<NodeTerm*>(array_index->ident->var)) {
            const NodeTerm* base =
                std::get<NodeTerm*>(array_index->ident->var);
            if (std::holds_alternative<NodeTermIdent*>(base->var)) {
                const std::string name =
                    std::get<NodeTermIdent*>(base->var)->ident.value.value();
                auto it = m_var_types.find(name);
                if (it != m_var_types.end()) {
                    return vector_element_type(it->second);
                }
            }
        }
    }

    return "long";
}

// ---------------------------------------------------------------------------
// The `ask` built-in
// ---------------------------------------------------------------------------

// True if `name` should be treated as the `ask` built-in. A program that defines
// its own `fn ask(...)` wins, so check that no such function was declared.
bool Generator::is_builtin_ask(const std::string& name) const {
    if (name != "ask") {
        return false;
    }
    return m_fn_return_types.find("ask") == m_fn_return_types.end();
}

// True if this expression is exactly a call to the `ask` built-in.
bool Generator::is_ask_call(const NodeExpr* expr) const {
    if (!std::holds_alternative<NodeTerm*>(expr->var)) {
        return false;
    }
    const NodeTerm* term = std::get<NodeTerm*>(expr->var);
    if (!std::holds_alternative<NodeTermFnCall*>(term->var)) {
        return false;
    }
    return is_builtin_ask(std::get<NodeTermFnCall*>(term->var)->name.value.value());
}

// ---------------------------------------------------------------------------
// The other built-ins: len, str, num and decimal
// ---------------------------------------------------------------------------

// True if `name` is one of them. A program that defines its own function with
// that name gets the ordinary rules instead, exactly as with `ask`.
bool Generator::is_builtin(const std::string& name) const {
    if (name != "len" && name != "str" && name != "num" && name != "decimal") {
        return false;
    }
    return m_fn_return_types.find(name) == m_fn_return_types.end();
}

// True if `name` is one of the networking built-ins, and the program imported the
// package that offers them. Without the import there is no library to link, so
// the name means nothing -- the type checker says so before we get this far.
bool Generator::is_builtin_networking(const std::string& name) const {
    if (name != "http_get" && name != "http_code") {
        return false;
    }
    // A program that defines its own function of that name gets the ordinary one.
    if (m_fn_return_types.find(name) != m_fn_return_types.end()) {
        return false;
    }
    for (const Token& imported : m_prog.imports) {
        if (imported.value.value() == "networking") {
            return true;
        }
    }
    return false;
}

// A character written out so that g++ reads it back as the same character.
std::string Generator::escape_for_cpp(const char letter) {
    switch (letter) {
    case '\n':
        return "\\n";
    case '\t':
        return "\\t";
    case '\r':
        return "\\r";
    case '\0':
        return "\\0";
    case '\\':
        return "\\\\";
    case '\'':
        return "\\'";
    default:
        return std::string(1, letter);
    }
}

// ---------------------------------------------------------------------------
// Does this function body return a value?
// ---------------------------------------------------------------------------
//
// Used to decide between a `long`-returning and a `void` function when the author
// did not write `-> type`.
//
// NOTE: this checks `gives` directly inside a block, if/elif/else, and while.
// It does not look inside `for` or `for-each` loop bodies. That is a pre-existing
// gap, left as-is here so this refactor stays behaviour-preserving -- but it
// means a `gives` inside only a for loop produces a `void` function containing
// `return <value>;`, which g++ rejects. Worth fixing separately.

bool Generator::has_return(const NodeScope* body) const {
    for (const NodeStmt* stmt : body->stmts) {
        // gives <something>;
        if (std::holds_alternative<NodeStmtExit*>(stmt->var)) {
            return true;
        }
        // A plain { ... } block.
        if (std::holds_alternative<NodeScope*>(stmt->var)) {
            if (has_return(std::get<NodeScope*>(stmt->var))) {
                return true;
            }
        }
        // if / elif / else -- all branches count.
        if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {
            auto stmt_if = std::get<NodeStmtIf*>(stmt->var);
            if (has_return(stmt_if->scope)) {
                return true;
            }
            if (stmt_if->pred.has_value() && has_return_pred(stmt_if->pred.value())) {
                return true;
            }
        }
        if (std::holds_alternative<NodeStmtWhile*>(stmt->var)) {
            if (has_return(std::get<NodeStmtWhile*>(stmt->var)->scope)) {
                return true;
            }
        }
        // switch / case / default -- all branches count.
        if (std::holds_alternative<NodeStmtSwitch*>(stmt->var)) {
            auto stmt_switch = std::get<NodeStmtSwitch*>(stmt->var);
            for (const NodeCase* node_case : stmt_switch->cases) {
                if (has_return(node_case->body)) {
                    return true;
                }
            }
            if (stmt_switch->default_body.has_value() &&
                has_return(stmt_switch->default_body.value())) {
                return true;
            }
        }
    }
    return false;
}

bool Generator::has_return_pred(const NodeIfPred* pred) const {
    if (std::holds_alternative<NodeIfPredElif*>(pred->var)) {
        auto elif = std::get<NodeIfPredElif*>(pred->var);
        if (has_return(elif->scope)) {
            return true;
        }
        if (elif->pred.has_value()) {
            return has_return_pred(elif->pred.value());
        }
        return false;
    }
    if (std::holds_alternative<NodeIfPredElse*>(pred->var)) {
        return has_return(std::get<NodeIfPredElse*>(pred->var)->scope);
    }
    return false;
}