/**
 * @file check_expr.hpp
 * @brief Working out the type of an expression.
 *
 * Everything here answers one question: if this expression were written out as
 * code, what type would the value have? Statements ask that question and then
 * decide whether the answer is allowed there (see check_stmt.hpp).
 */

// ============================================================================
// 1. Types, as text
// ============================================================================

// type_word_ -> "word". A token that is not a type keyword comes back as "".
std::string TypeChecker::type_name(const TokenType type) {
    if (type == TokenType::type_number_) {
        return "number";
    }
    if (type == TokenType::type_word_) {
        return "word";
    }
    if (type == TokenType::type_question_) {
        return "question";
    }
    if (type == TokenType::type_decimal_) {
        return "decimal";
    }
    if (type == TokenType::type_letter) {
        return "letter";
    }
    return "";
}

// number, decimal, letter and question all hold numbers, so arithmetic accepts
// all four. An empty type counts as a number as well, because not knowing is
// never an error.
bool TypeChecker::is_number(const std::string& type) {
    return type.empty() || type == "number" || type == "decimal" || type == "letter" ||
           type == "question";
}

bool TypeChecker::is_array(const std::string& type) {
    // Two characters at the end: "[]".
    return type.size() > 2 && type.substr(type.size() - 2) == "[]";
}

// "word[]" -> "word". Anything that is not an array comes back unchanged.
std::string TypeChecker::element_type(const std::string& type) {
    if (!is_array(type)) {
        return type;
    }
    return type.substr(0, type.size() - 2);
}

// May a value of type `given` be put where a `wanted` is expected?
bool TypeChecker::fits(const std::string& wanted, const std::string& given) {
    // If either side is unknown there is nothing to say.
    if (wanted.empty() || given.empty()) {
        return true;
    }
    if (wanted == given) {
        return true;
    }
    // A question is a 0 or a 1, and so is a number, so the two mix freely.
    if (wanted == "question" || given == "question") {
        return wanted == "number" || given == "number";
    }
    // One letter fits inside a number, but a number does not fit in one letter.
    if (wanted == "number" && given == "letter") {
        return true;
    }
    // A whole number fits in a decimal, where it becomes 2.0. Not the reverse.
    return wanted == "decimal" && given == "number";
}

std::string TypeChecker::param_type(const NodeFnParam& param) {
    std::string type = "number";
    if (param.type_annotation.has_value()) {
        type = type_name(param.type_annotation.value());
    }
    if (param.isArray) {
        type = type + "[]";
    }
    return type;
}

std::string TypeChecker::bin_op_name(const BinOp op) {
    if (op == BinOp::Add) {
        return "+";
    }
    if (op == BinOp::Sub) {
        return "-";
    }
    if (op == BinOp::Mul) {
        return "*";
    }
    if (op == BinOp::Div) {
        return "/";
    }
    if (op == BinOp::Mod) {
        return "%";
    }
    if (op == BinOp::Pow) {
        return "**";
    }
    return "?";
}

// ============================================================================
// 2. Which line is this expression on?
// ============================================================================

int TypeChecker::expr_line(const NodeExpr* expr) {
    // Two values joined by an operator: ask about the left one.
    if (std::holds_alternative<NodeBinExpr*>(expr->var)) {
        return expr_line(std::get<NodeBinExpr*>(expr->var)->lhs);
    }

    const NodeTerm* term = std::get<NodeTerm*>(expr->var);
    if (std::holds_alternative<NodeTermIntLit*>(term->var)) {
        return std::get<NodeTermIntLit*>(term->var)->int_lit.line;
    }
    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        return std::get<NodeTermIdent*>(term->var)->ident.line;
    }
    if (std::holds_alternative<NodeTermStringLit*>(term->var)) {
        return std::get<NodeTermStringLit*>(term->var)->string_lit.line;
    }
    if (std::holds_alternative<NodeTermCharLit*>(term->var)) {
        return std::get<NodeTermCharLit*>(term->var)->char_lit.line;
    }
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        return std::get<NodeTermFnCall*>(term->var)->name.line;
    }
    // Every other term is a wrapper around another expression, so keep digging.
    if (std::holds_alternative<NodeTermParen*>(term->var)) {
        return expr_line(std::get<NodeTermParen*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermNot*>(term->var)) {
        return expr_line(std::get<NodeTermNot*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermUnaryMinus*>(term->var)) {
        return expr_line(std::get<NodeTermUnaryMinus*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPreInc*>(term->var)) {
        return expr_line(std::get<NodeTermPreInc*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPostInc*>(term->var)) {
        return expr_line(std::get<NodeTermPostInc*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPreDec*>(term->var)) {
        return expr_line(std::get<NodeTermPreDec*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermPostDec*>(term->var)) {
        return expr_line(std::get<NodeTermPostDec*>(term->var)->expr);
    }
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        return expr_line(std::get<NodeTermArrayIndex*>(term->var)->ident);
    }

    // An array literal is built out of tokens we do not keep, so it has no line.
    return 0;
}

// ============================================================================
// 3. Conditions
// ============================================================================

// if (...) / while (...) / for (...) want something that is true or false.
void TypeChecker::check_condition(const NodeExpr* expr) {
    const std::string type = expr_type(expr);
    if (is_number(type)) {
        return;  // 0 is false and anything else is true, same as C++
    }
    error(expr_line(expr), "A " + type + " cannot be a condition, precious!");
}

// ============================================================================
// 4. Expressions
// ============================================================================

// lhs <op> rhs
std::string TypeChecker::bin_type(const NodeBinExpr* bin_expr) {
    const std::string left = expr_type(bin_expr->lhs);
    const std::string right = expr_type(bin_expr->rhs);

    // `and` and `or` join two true-or-false things and give one back.
    if (bin_expr->op == BinOp::And || bin_expr->op == BinOp::Or) {
        return "question";
    }

    // Comparing gives the answer yes or no.
    if (bin_expr->op == BinOp::Eq || bin_expr->op == BinOp::NotEq || bin_expr->op == BinOp::Lt ||
        bin_expr->op == BinOp::Gt || bin_expr->op == BinOp::LtEq || bin_expr->op == BinOp::GtEq) {
        return "question";
    }

    // Two words make one longer word.
    if (bin_expr->op == BinOp::Add) {
        if (left == "word" || right == "word") {
            return "word";
        }
        if (left == "decimal" || right == "decimal") {
            return "decimal";
        }
        return "number";
    }

    // -, * / and % are arithmetic, so they need numbers on both sides.
    if (!is_number(left) || !is_number(right)) {
        const std::string wrong = is_number(left) ? right : left;
        const std::string op = bin_op_name(bin_expr->op);
        error(expr_line(bin_expr->lhs),
              "'" + op + "' does not work on a " + wrong + ", precious! Numbers only.");
    }
    if (left == "decimal" || right == "decimal") {
        return "decimal";
    }
    return "number";
}

// [1, 2, 3] -> "number[]", and ["a", "b"] -> "word[]". A list that is empty, or
// mixed, or only partly known, stays unknown.
std::string TypeChecker::array_lit_type(const NodeTermArrayLit* array_lit) {
    std::string common;
    for (const NodeExpr* element : array_lit->elements) {
        const std::string type = expr_type(element);
        if (type.empty()) {
            return "";
        }
        if (common.empty()) {
            common = type;
        }
        if (common != type) {
            return "";
        }
    }
    if (common.empty()) {
        return "";
    }
    return common + "[]";
}

// name(arg, arg) -- also how `ask(...)` and the small built-ins arrive here.
std::string TypeChecker::call_type(const NodeTermFnCall* fn_call) {
    const std::string name = fn_call->name.value.value();

    // `ask` reads from stdin, and what it hands back depends on where it is
    // going, so its type is whatever the caller needs. A program that defines
    // its own `fn ask(...)` gets the ordinary rules instead.
    if (name == "ask" && m_fns.find("ask") == m_fns.end()) {
        return "";
    }

    // len, str, num and decimal: the same idea, with a known answer instead of a
    // borrowed one. A program that defines its own function of that name gets
    // the ordinary rules.
    if (m_fns.find(name) == m_fns.end()) {
        if (name == "len" || name == "str" || name == "num" || name == "decimal") {
            return builtin_call_type(name, fn_call);
        }
    }

    // http_get and http_code exist only if the program imported the package that
    // offers them. Without the import there is no library to link, so say so here
    // rather than letting g++ complain about an unknown function.
    if (m_fns.find(name) == m_fns.end()) {
        if (name == "http_get" || name == "http_code") {
            if (!has_import("networking")) {
                error(fn_call->name.line,
                      "You must 'import \"networking\";' before you " + name +
                          ", precious!");
                return "";
            }
            return networking_call_type(name, fn_call);
        }
    }

    const auto found = m_fns.find(name);
    if (found == m_fns.end()) {
        return "";  // not a function we know about: g++ will have its own say
    }
    const FnSignature& signature = found->second;

    const size_t wanted = signature.params.size();
    const size_t given = fn_call->args.size();
    if (given != wanted) {
        error(fn_call->name.line, "'" + name + "' takes " + std::to_string(wanted) +
                                      " argument(s), but " + std::to_string(given) + " given!");
    }

    // Check as many arguments as both sides have, so one missing argument does
    // not also produce a second complaint about a missing type.
    const size_t pairs = given < wanted ? given : wanted;
    for (size_t i = 0; i < pairs; i++) {
        const std::string given = expr_type(fn_call->args[i]);
        if (!fits(signature.params[i], given)) {
            error(expr_line(fn_call->args[i]),
                  "Cannot put a " + given + " into argument " + std::to_string(i + 1) + " of '" +
                      name + "', which takes a " + signature.params[i] + ", precious!");
        }
    }

    return signature.returns;
}

// One value: 42, x, "hello", f(1), [1, 2], arr[i], -x, ++x
std::string TypeChecker::term_type(const NodeTerm* term) {
    if (std::holds_alternative<NodeTermIntLit*>(term->var)) {
        return "number";
    }
    if (std::holds_alternative<NodeTermStringLit*>(term->var)) {
        return "word";
    }
    if (std::holds_alternative<NodeTermCharLit*>(term->var)) {
        return "letter";
    }
    // A variable: whatever type it was declared with, or "" if we cannot find it.
    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        return lookup_var(std::get<NodeTermIdent*>(term->var)->ident.value.value());
    }
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        return call_type(std::get<NodeTermFnCall*>(term->var));
    }
    if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
        return array_lit_type(std::get<NodeTermArrayLit*>(term->var));
    }

    // (expr) -- the brackets change nothing about the type.
    if (std::holds_alternative<NodeTermParen*>(term->var)) {
        return expr_type(std::get<NodeTermParen*>(term->var)->expr);
    }

    // arr[i] or msg[i]
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        const NodeTermArrayIndex* index = std::get<NodeTermArrayIndex*>(term->var);

        const std::string where = expr_type(index->index);
        if (!is_number(where)) {
            error(expr_line(index->index), "Cannot use a " + where + " as an index, precious!");
        }

        const std::string container = expr_type(index->ident);
        if (is_array(container)) {
            return element_type(container);
        }
        if (container == "word") {
            return "letter";  // one character of a word
        }
        if (!container.empty()) {
            error(expr_line(index->ident),
                  "Cannot pick a single value out of a " + container + ", precious!");
        }
        return "";
    }

    // !expr
    if (std::holds_alternative<NodeTermNot*>(term->var)) {
        const NodeExpr* inner = std::get<NodeTermNot*>(term->var)->expr;
        const std::string type = expr_type(inner);
        if (type == "word" || is_array(type)) {
            error(expr_line(inner), "Cannot ask a " + type + " a question, precious!");
        }
        return "question";
    }

    // -expr
    if (std::holds_alternative<NodeTermUnaryMinus*>(term->var)) {
        const NodeExpr* inner = std::get<NodeTermUnaryMinus*>(term->var)->expr;
        const std::string type = expr_type(inner);
        if (!is_number(type)) {
            error(expr_line(inner), "'-' does not work on a " + type + ", precious! Numbers only.");
        }
        return type;
    }

    // ++x, x++, --x, x-- : all four make one more or one less of whatever x is.
    const NodeExpr* counter = nullptr;
    if (std::holds_alternative<NodeTermPreInc*>(term->var)) {
        counter = std::get<NodeTermPreInc*>(term->var)->expr;
    }
    if (std::holds_alternative<NodeTermPostInc*>(term->var)) {
        counter = std::get<NodeTermPostInc*>(term->var)->expr;
    }
    if (std::holds_alternative<NodeTermPreDec*>(term->var)) {
        counter = std::get<NodeTermPreDec*>(term->var)->expr;
    }
    if (std::holds_alternative<NodeTermPostDec*>(term->var)) {
        counter = std::get<NodeTermPostDec*>(term->var)->expr;
    }
    if (counter != nullptr) {
        const std::string type = expr_type(counter);
        if (!is_number(type)) {
            error(expr_line(counter), "Cannot count up or down a " + type + ", precious!");
        }
        return type;
    }

    // Every NodeTerm alternative is handled above. A new one added to ast.hpp and
    // missed here would be quietly treated as "no type", so say so loudly.
    std::cerr << "[BUG] term_type: unhandled expression kind. Add it in check_expr.hpp."
              << std::endl;
    exit(EXIT_FAILURE);
}

// ============================================================================
// 5. The small built-ins: len, str, num, decimal
// ============================================================================

// What one of them gives back, and whether what it was handed makes sense.
// `ask` is not here: it has no type of its own, because the call site decides.
std::string TypeChecker::builtin_call_type(const std::string& name,
                                           const NodeTermFnCall* fn_call) {
    if (fn_call->args.size() != 1) {
        error(fn_call->name.line,
              "'" + name + "' takes exactly one value, precious! " +
                  (fn_call->args.empty() ? "It was given none." : "Too many were given."));
        return "";
    }

    const std::string given = expr_type(fn_call->args[0]);

    if (name == "str") {
        return "word"; // anything at all can be written out as text
    }

    if (name == "len") {
        if (!given.empty() && given != "word" && !is_array(given)) {
            error(expr_line(fn_call->args[0]),
                  "Cannot ask a " + given + " how long it is, precious!");
        }
        return "number";
    }

    // num and decimal read text into a number. An array is the only thing that
    // can never be one, and the rest is left to g++ -- strtol quietly gives 0.
    if (is_array(given)) {
        error(expr_line(fn_call->args[0]),
              "Cannot read a number out of a " + given + ", precious!");
    }

    return name == "decimal" ? "decimal" : "number";
}

// ============================================================================
// 6. The networking built-ins
// ============================================================================

// True if the program wrote `import "name"`.
bool TypeChecker::has_import(const std::string& name) const {
    for (const Token& imported : m_prog.imports) {
        if (imported.value.value() == name) {
            return true;
        }
    }
    return false;
}

// http_get(url) and http_code(url) -- a url, plus an optional timeout in seconds.
std::string TypeChecker::networking_call_type(const std::string& name,
                                              const NodeTermFnCall* fn_call) {
    if (fn_call->args.empty() || fn_call->args.size() > 2) {
        error(fn_call->name.line,
              "'" + name + "' takes a url, and optionally a timeout in seconds, precious!");
        return "";
    }

    const std::string url = expr_type(fn_call->args[0]);
    if (is_array(url)) {
        error(expr_line(fn_call->args[0]),
              "A url must be a word, not a " + url + ", precious!");
    }

    if (fn_call->args.size() == 2) {
        const std::string seconds = expr_type(fn_call->args[1]);
        if (!is_number(seconds)) {
            error(expr_line(fn_call->args[1]),
                  "A timeout must be a number of seconds, not a " + seconds + ", precious!");
        }
    }

    return name == "http_get" ? "word" : "number";
}

// An expression is either one term, or two terms joined by an operator.
std::string TypeChecker::expr_type(const NodeExpr* expr) {
    if (std::holds_alternative<NodeTerm*>(expr->var)) {
        return term_type(std::get<NodeTerm*>(expr->var));
    }
    if (std::holds_alternative<NodeBinExpr*>(expr->var)) {
        return bin_type(std::get<NodeBinExpr*>(expr->var));
    }
    std::cerr << "[BUG] expr_type: unhandled expression kind." << std::endl;
    exit(EXIT_FAILURE);
}
