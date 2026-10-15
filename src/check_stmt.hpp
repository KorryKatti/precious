/**
 * @file check_stmt.hpp
 * @brief Checking statements, functions and the whole program.
 *
 * Layout:
 *   1. Scopes           -- which variables we can see right now
 *   2. Error messages   -- the two functions every complaint goes through
 *   3. One check_xxx() per statement kind, named after the Precious syntax
 *   4. Functions        -- what they take, and what they give back
 *   5. check_stmt()     -- the dispatcher, and check() that starts it all
 */

// ============================================================================
// 1. Scopes
// ============================================================================

// A new block opens. Nothing is visible inside it yet.
void TypeChecker::enter_scope() {
    m_scopes.push_back({});
}

// The block ends, and everything declared inside it goes away with it.
void TypeChecker::exit_scope() {
    m_scopes.pop_back();
}

void TypeChecker::declare_var(const std::string& name, const std::string& type) {
    m_scopes.back()[name] = type;
}

// The innermost block that knows this name wins. "" means no block knows it.
std::string TypeChecker::lookup_var(const std::string& name) {
    for (size_t i = m_scopes.size(); i > 0; i--) {
        const auto found = m_scopes[i - 1].find(name);
        if (found != m_scopes[i - 1].end()) {
            return found->second;
        }
    }
    return "";
}

// ============================================================================
// 2. Saying what is wrong
// ============================================================================

void TypeChecker::error(const int line, const std::string& msg) {
    std::cerr << "[ERROR] " << msg;
    if (line > 0) {
        std::cerr << " (line " << line << ")";
    }
    std::cerr << std::endl;
    m_error_count++;
}

// "Can this value go into that place?" -- the rule behind `my x: T = e`, `x = e`,
// `arr[i] = e` and `push arr, e`.
void TypeChecker::check_put(const std::string& wanted, const std::string& given, const int line,
                            const std::string& name) {
    if (fits(wanted, given)) {
        return;
    }
    if (name.empty()) {
        error(line, "Cannot put a " + given + " into a " + wanted + ", precious!");
        return;
    }
    error(line, "Cannot put a " + given + " into '" + name + "', which holds a " + wanted +
                    ", precious!");
}

// ============================================================================
// 3. One function per statement kind
// ============================================================================

// my x [: type] = expr;
void TypeChecker::check_my(const NodeStmtLet* stmt) {
    const std::string name = stmt->ident.value.value();
    const std::string value = expr_type(stmt->expr);

    std::string declared;
    if (stmt->type_annotation.has_value()) {
        declared = type_name(stmt->type_annotation.value());
        if (stmt->is_array) {
            declared = declared + "[]";
        }
        check_put(declared, value, stmt->ident.line, name);

        // When an array is written out in full, every value in it has to be the
        // array's element type.
        if (is_array(declared) && std::holds_alternative<NodeTerm*>(stmt->expr->var)) {
            const NodeTerm* term = std::get<NodeTerm*>(stmt->expr->var);
            if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
                const NodeTermArrayLit* array_lit = std::get<NodeTermArrayLit*>(term->var);
                for (const NodeExpr* element : array_lit->elements) {
                    check_put(element_type(declared), expr_type(element), stmt->ident.line, name);
                }
            }
        }
    }

    // With no annotation the variable simply takes the type of its first value.
    declare_var(name, declared.empty() ? value : declared);
}

// gives expr;
void TypeChecker::check_gives(const NodeStmtExit* stmt) {
    m_fn_gives = true;
    if (m_fn_return.empty()) {
        return;  // the fn made no promise, so there is nothing to check against
    }
    const std::string value = expr_type(stmt->expr);
    if (!fits(m_fn_return, value)) {
        error(expr_line(stmt->expr), "'" + m_fn_name + "' promises to give a " + m_fn_return +
                                         ", but gives a " + value + ", precious!");
    }
}

// x = expr;
void TypeChecker::check_assign(const NodeStmtAssign* stmt) {
    const std::string name = stmt->ident.value.value();
    check_put(lookup_var(name), expr_type(stmt->expr), stmt->ident.line, name);
}

// x += expr;   (also -= *= /= %=)
void TypeChecker::check_compound_assign(const NodeStmtCompoundAssign* stmt) {
    const std::string name = stmt->ident.value.value();
    const std::string target = lookup_var(name);
    const std::string value = expr_type(stmt->expr);

    // Adding is the one that works on words: msg += " more";
    if (stmt->op == TokenType::pluseq && (target == "word" || value == "word")) {
        if (target == "word" && !value.empty() && value != "word" && value != "letter") {
            error(stmt->ident.line, "Cannot add a " + value + " to a word, precious!");
        }
        return;
    }

    // Everything else is arithmetic, so it needs numbers.
    if (!is_number(target)) {
        error(stmt->ident.line,
              "Cannot do " + to_string(stmt->op) + " on a " + target + ", precious! Numbers only.");
    } else if (!is_number(value)) {
        error(stmt->ident.line, "Cannot do " + to_string(stmt->op) + " with a " + value +
                                    ", precious! Numbers only.");
    }
}

// arr[i] = expr;
void TypeChecker::check_array_assign(const NodeStmtArrayAssign* stmt) {
    const std::string name = stmt->ident.value.value();
    const std::string target = lookup_var(name);
    const std::string value = expr_type(stmt->expr);

    if (!target.empty() && !is_array(target)) {
        error(stmt->ident.line, "'" + name + "' holds a " + target + ", not an array, precious!");
        return;
    }

    const std::string where = expr_type(stmt->index);
    if (!is_number(where)) {
        error(expr_line(stmt->index), "Cannot use a " + where + " as an index, precious!");
    }

    if (!target.empty()) {
        check_put(element_type(target), value, stmt->ident.line, name);
    }
}

// if (cond) { ... }
void TypeChecker::check_if(const NodeStmtIf* stmt) {
    check_condition(stmt->expr);
    check_block(stmt->scope);
    if (stmt->pred.has_value()) {
        check_if_pred(stmt->pred.value());
    }
}

// The elif / else tail of an if.
void TypeChecker::check_if_pred(const NodeIfPred* pred) {
    if (std::holds_alternative<NodeIfPredElif*>(pred->var)) {
        const NodeIfPredElif* elif = std::get<NodeIfPredElif*>(pred->var);
        check_condition(elif->expr);
        check_block(elif->scope);
        if (elif->pred.has_value()) {
            check_if_pred(elif->pred.value());
        }
        return;
    }
    if (std::holds_alternative<NodeIfPredElse*>(pred->var)) {
        check_block(std::get<NodeIfPredElse*>(pred->var)->scope);
        return;
    }
    std::cerr << "[BUG] check_if_pred: unhandled if/elif/else shape." << std::endl;
    exit(EXIT_FAILURE);
}

// while (cond) { ... }
void TypeChecker::check_while(const NodeStmtWhile* stmt) {
    check_condition(stmt->expr);
    check_block(stmt->scope);
}

// for (init; cond; update) { ... }
void TypeChecker::check_for(const NodeStmtFor* stmt) {
    // `my i = 0` belongs to the loop, not to the block around it.
    enter_scope();

    if (stmt->init != nullptr) {
        check_stmt(stmt->init);
    }
    if (stmt->condition != nullptr) {
        check_condition(stmt->condition);
    }
    if (stmt->update != nullptr) {
        check_stmt(stmt->update);
    }
    check_block(stmt->body);

    exit_scope();
}

// for (item in arr) { ... }
void TypeChecker::check_for_each(const NodeStmtForEach* stmt) {
    enter_scope();

    const std::string array = expr_type(stmt->array);
    if (array.empty() || is_array(array)) {
        declare_var(stmt->element.value.value(), array.empty() ? "" : element_type(array));
    } else {
        error(expr_line(stmt->array),
              "Cannot walk through a " + array + ", precious! Only arrays can be walked.");
        declare_var(stmt->element.value.value(), "");
    }

    check_block(stmt->body);
    exit_scope();
}

// switch (value) { case 1: ... default: ... }
void TypeChecker::check_switch(const NodeStmtSwitch* stmt) {
    const std::string type = expr_type(stmt->expr);
    if (!is_number(type)) {
        error(expr_line(stmt->expr),
              "Cannot switch on a " + type + ", precious! A number is wanted.");
    }

    for (const NodeCase* node_case : stmt->cases) {
        check_block(node_case->body);
    }
    if (stmt->default_body.has_value()) {
        check_block(stmt->default_body.value());
    }
}

// push arr, value;
void TypeChecker::check_push(const NodeStmtPush* stmt) {
    const std::string name = stmt->ident.value.value();
    const std::string target = lookup_var(name);
    const std::string value = expr_type(stmt->expr);

    if (!target.empty() && !is_array(target)) {
        error(stmt->ident.line, "'" + name + "' holds a " + target + ", not an array, precious!");
        return;
    }
    if (!target.empty()) {
        check_put(element_type(target), value, stmt->ident.line, name);
    }
}

// pop arr;
void TypeChecker::check_pop(const NodeStmtPop* stmt) {
    const std::string name = stmt->ident.value.value();
    const std::string target = lookup_var(name);
    if (!target.empty() && !is_array(target)) {
        error(stmt->ident.line, "'" + name + "' holds a " + target + ", not an array, precious!");
    }
}

// say(expr);
void TypeChecker::check_say(const NodeStmtPrint* stmt) {
    const std::string type = expr_type(stmt->expr);
    if (is_array(type)) {
        error(expr_line(stmt->expr),
              "Cannot say a whole array, precious! Say one of its values instead.");
    }
}

// { ... } used as a statement
void TypeChecker::check_block(const NodeScope* scope) {
    enter_scope();
    for (const NodeStmt* stmt : scope->stmts) {
        check_stmt(stmt);
    }
    exit_scope();
}

// ============================================================================
// 4. Functions
// ============================================================================

// Remembers every function before any body is checked, so that calling a
// function from a line above where it is defined still works.
void TypeChecker::collect_fn_signatures() {
    for (const NodeStmt* stmt : m_prog.stmts) {
        if (!std::holds_alternative<NodeStmtFn*>(stmt->var)) {
            continue;
        }
        const NodeStmtFn* fn = std::get<NodeStmtFn*>(stmt->var);

        FnSignature signature;
        for (const NodeFnParam& param : fn->params) {
            signature.params.push_back(param_type(param));
        }
        if (fn->return_type.has_value()) {
            signature.returns = type_name(fn->return_type.value());
        }
        m_fns[fn->name.value.value()] = signature;
    }
}

// fn name(params) [-> type] { body }
void TypeChecker::check_fn(const NodeStmtFn* fn) {
    enter_scope();

    m_fn_name = fn->name.value.value();
    m_fn_return = fn->return_type.has_value() ? type_name(fn->return_type.value()) : "";
    m_fn_gives = false;

    // The parameters are ordinary variables as far as the body is concerned.
    for (const NodeFnParam& param : fn->params) {
        declare_var(param.name.value.value(), param_type(param));
    }

    check_block(fn->body);

    if (!m_fn_return.empty() && !m_fn_gives) {
        error(fn->name.line, "'" + m_fn_name + "' promises to give a " + m_fn_return +
                                 ", but never gives anything!");
    }

    exit_scope();
}

// ============================================================================
// 5. The dispatcher
// ============================================================================

// Picks the right check_xxx() for this statement. The chain below is the full
// list of statement kinds -- read it as a table of contents, in the same order as
// gen_stmt() in gen_stmt.hpp.
//
// ADDING A STATEMENT KIND? Add a check_xxx() function above and add one branch
// here. If you forget, the compiler says so at the bottom.
void TypeChecker::check_stmt(const NodeStmt* stmt) {
    if (std::holds_alternative<NodeStmtFn*>(stmt->var)) {
        check_fn(std::get<NodeStmtFn*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeScope*>(stmt->var)) {
        check_block(std::get<NodeScope*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtExit*>(stmt->var)) {
        check_gives(std::get<NodeStmtExit*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtLet*>(stmt->var)) {
        check_my(std::get<NodeStmtLet*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtAssign*>(stmt->var)) {
        check_assign(std::get<NodeStmtAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtCompoundAssign*>(stmt->var)) {
        check_compound_assign(std::get<NodeStmtCompoundAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {
        check_if(std::get<NodeStmtIf*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtWhile*>(stmt->var)) {
        check_while(std::get<NodeStmtWhile*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtFor*>(stmt->var)) {
        check_for(std::get<NodeStmtFor*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtForEach*>(stmt->var)) {
        check_for_each(std::get<NodeStmtForEach*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPrint*>(stmt->var)) {
        check_say(std::get<NodeStmtPrint*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtExpr*>(stmt->var)) {
        // A call whose result is thrown away. The call still has to be right, so
        // working out the type is the whole check.
        expr_type(std::get<NodeStmtExpr*>(stmt->var)->expr);
        return;
    }
    if (std::holds_alternative<NodeStmtArrayAssign*>(stmt->var)) {
        check_array_assign(std::get<NodeStmtArrayAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtBreak*>(stmt->var)) {
        return;  // break; -- nothing to know about it
    }
    if (std::holds_alternative<NodeStmtContinue*>(stmt->var)) {
        return;  // continue; -- nothing to know about it
    }
    if (std::holds_alternative<NodeStmtSwitch*>(stmt->var)) {
        check_switch(std::get<NodeStmtSwitch*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPush*>(stmt->var)) {
        check_push(std::get<NodeStmtPush*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPop*>(stmt->var)) {
        check_pop(std::get<NodeStmtPop*>(stmt->var));
        return;
    }

    std::cerr << "[BUG] check_stmt: unhandled statement kind. Add it in check_stmt.hpp."
              << std::endl;
    exit(EXIT_FAILURE);
}

bool TypeChecker::check() {
    collect_fn_signatures();

    for (const NodeStmt* stmt : m_prog.stmts) {
        check_stmt(stmt);
    }

    if (m_error_count == 0) {
        return true;
    }
    std::cerr << "[ERROR] " << m_error_count << " type mistake(s), precious!" << std::endl;
    return false;
}
