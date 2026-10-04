/**
 * @file gen_stmt.hpp
 * @brief Statement code generation for the Precious code generator.
 *
 * Layout:
 *   1. gen_scope()   -- a { ... } block, and the variable bookkeeping around it
 *   2. gen_if_pred() -- the elif/else chain after an if
 *   3. One gen_xxx() function per statement kind, named after the Precious
 *      syntax it emits
 *   4. gen_stmt()    -- the dispatcher that picks one based on the node kind
 *
 * Every statement is indented 4 spaces to match what gen_scope() expects.
 */

void Generator::gen_scope(const NodeScope* scope, bool inline_brace) {
    // Remember where the variable list is now, so everything declared inside
    // this block can be forgotten again on the way out.
    m_declared_scopes.push_back(m_declared.size());

    if (inline_brace) {
        // Used after `if (...)` / `while (...)` / `for (...)`, where the space
        // before the brace has already been written.
        m_output << " {\n";
    } else {
        m_output << "{\n";
    }

    for (const NodeStmt* stmt : scope->stmts) {
        gen_stmt(stmt);
    }

    m_output << "}\n";

    // Drop every variable declared inside this block.
    const size_t count_inside = m_declared.size() - m_declared_scopes.back();
    for (size_t i = 0; i < count_inside; i++) {
        m_var_types.erase(m_declared.back());
        m_declared.pop_back();
    }
    m_declared_scopes.pop_back();
}

// elif (cond) { } else if ... / else { }
void Generator::gen_if_pred(const NodeIfPred* pred) {
    if (std::holds_alternative<NodeIfPredElif*>(pred->var)) {
        auto elif = std::get<NodeIfPredElif*>(pred->var);
        m_output << " else if (";
        gen_expr(elif->expr);
        m_output << ")";
        gen_scope(elif->scope, true);
        if (elif->pred.has_value()) {
            gen_if_pred(elif->pred.value());
        }
        return;
    }

    if (std::holds_alternative<NodeIfPredElse*>(pred->var)) {
        m_output << " else";
        gen_scope(std::get<NodeIfPredElse*>(pred->var)->scope, true);
        return;
    }

    std::cerr << "[BUG] gen_if_pred: unhandled if/elif/else shape." << std::endl;
    exit(EXIT_FAILURE);
}

// Turns a compound-assignment token into the symbol printed before the `=`, so
// `+=` becomes "+" and the caller writes `x " +" "= 3`.
std::string Generator::compound_op_symbol(const TokenType op) const {
    if (op == TokenType::minuseq) {
        return "-";
    }
    if (op == TokenType::stareq) {
        return "*";
    }
    if (op == TokenType::fslasheq) {
        return "/";
    }
    if (op == TokenType::moduloeq) {
        return "%";
    }
    return "+"; // pluseq, and the fallback
}

// gives expr;   ->  return expr;
void Generator::gen_gives(const NodeStmtExit* stmt) {
    m_output << "    return ";
    // `gives` feeds the process exit code, so a number is wanted here even when
    // the value came from `ask`, which otherwise reads a line.
    const std::string old_ask_type = m_ask_type;
    m_ask_type = "long";
    gen_expr(stmt->expr);
    m_ask_type = old_ask_type;
    m_output << ";\n";
}

// my x = expr;
void Generator::gen_my(const NodeStmtLet* stmt) {
    const std::string& name = stmt->ident.value.value();

    // Redeclaring in the same block is an error. Walk only the variables
    // declared since the innermost block started.
    const size_t block_start = m_declared_scopes.empty() ? 0 : m_declared_scopes.back();
    for (size_t i = block_start; i < m_declared.size(); i++) {
        if (m_declared[i] == name) {
            std::cerr << "[ERROR] We already has it! '" << name
                      << "' is already declared, precious! (line " << stmt->ident.line << ")"
                      << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    m_declared.push_back(name);

    // Work out the C++ type: the annotation if there is one, otherwise guess
    // from the expression.
    std::string c_type;
    if (stmt->type_annotation.has_value()) {
        c_type = resolve_type(stmt->type_annotation.value());
        if (stmt->is_array) {
            c_type = "std::vector<" + c_type + ">";
        }
    } else {
        c_type = infer_type(stmt->expr);
    }
    m_var_types[name] = c_type;

    m_output << "    " << c_type << " " << name << " = ";
    gen_with_ask_type(is_ask_call(stmt->expr) ? c_type : "", stmt->expr);
    m_output << ";\n";
}

// x = expr;
void Generator::gen_assign(const NodeStmtAssign* stmt) {
    const std::string& name = stmt->ident.value.value();

    // If the target already has a type, let `ask` read in that type.
    std::string ask_type;
    if (is_ask_call(stmt->expr)) {
        auto it = m_var_types.find(name);
        if (it != m_var_types.end()) {
            ask_type = it->second;
        }
    }

    m_output << "    " << name << " = ";
    gen_with_ask_type(ask_type, stmt->expr);
    m_output << ";\n";
}

// x += expr;   (also -= *= /= %=)
void Generator::gen_compound_assign(const NodeStmtCompoundAssign* stmt) {
    m_output << "    " << stmt->ident.value.value() << " " << compound_op_symbol(stmt->op)
             << "= ";
    gen_expr(stmt->expr);
    m_output << ";\n";
}

// { ... } used as a statement
void Generator::gen_block(const NodeScope* scope) { gen_scope(scope, false); }

// if (cond) { ... }
void Generator::gen_if(const NodeStmtIf* stmt) {
    m_output << "    if (";
    gen_expr(stmt->expr);
    m_output << ")";
    gen_scope(stmt->scope, true);
    if (stmt->pred.has_value()) {
        gen_if_pred(stmt->pred.value());
    }
}

// while (cond) { ... }
void Generator::gen_while(const NodeStmtWhile* stmt) {
    m_output << "    while (";
    gen_expr(stmt->expr);
    m_output << ")";
    gen_scope(stmt->scope, true);
}

// for (init; cond; update) { ... }
void Generator::gen_for(const NodeStmtFor* stmt) {
    m_output << "    for (";

    // --- init ---
    if (stmt->init != nullptr) {
        if (std::holds_alternative<NodeStmtLet*>(stmt->init->var)) {
            // my i = 0  -- declared inline, so no need for a type; default long
            auto let = std::get<NodeStmtLet*>(stmt->init->var);
            m_output << resolve_type(let->type_annotation.value_or(TokenType::type_number_))
                     << " " << let->ident.value.value() << " = ";
            gen_expr(let->expr);
        } else if (std::holds_alternative<NodeStmtAssign*>(stmt->init->var)) {
            auto assign = std::get<NodeStmtAssign*>(stmt->init->var);
            m_output << assign->ident.value.value() << " = ";
            gen_expr(assign->expr);
        }
    }
    m_output << "; ";

    // --- condition ---
    if (stmt->condition != nullptr) {
        gen_expr(stmt->condition);
    }
    m_output << "; ";

    // --- update ---
    if (stmt->update != nullptr) {
        if (std::holds_alternative<NodeStmtAssign*>(stmt->update->var)) {
            auto assign = std::get<NodeStmtAssign*>(stmt->update->var);
            m_output << assign->ident.value.value() << " = ";
            gen_expr(assign->expr);
        } else if (std::holds_alternative<NodeStmtCompoundAssign*>(stmt->update->var)) {
            auto compound = std::get<NodeStmtCompoundAssign*>(stmt->update->var);
            m_output << compound->ident.value.value() << " "
                     << compound_op_symbol(compound->op) << "= ";
            gen_expr(compound->expr);
        } else if (std::holds_alternative<NodeStmtExpr*>(stmt->update->var)) {
            // i++ / i--
            gen_expr(std::get<NodeStmtExpr*>(stmt->update->var)->expr);
        }
    }
    m_output << ")";

    gen_scope(stmt->body, true);
}

// for (item in arr) { ... }
// C++ has no for-each over a range, so this becomes a counted loop that copies
// each element into `item`.
void Generator::gen_for_each(const NodeStmtForEach* stmt) {
    // The array has to be a plain variable so its name and type can be looked up.
    std::string array_name;
    if (std::holds_alternative<NodeTerm*>(stmt->array->var)) {
        auto term = std::get<NodeTerm*>(stmt->array->var);
        if (std::holds_alternative<NodeTermIdent*>(term->var)) {
            array_name = std::get<NodeTermIdent*>(term->var)->ident.value.value();
        }
    }

    // Default to long if the array's type is unknown.
    std::string element_type = "long";
    auto type_it = m_var_types.find(array_name);
    if (type_it != m_var_types.end()) {
        element_type = type_it->second;
        // "std::vector<long>" -> "long"
        if (element_type.find("std::vector<") == 0) {
            element_type = element_type.substr(12, element_type.size() - 13);
        }
    }

    m_output << "    for (long _i = 0; _i < " << array_name << ".size(); _i++) {\n";
    m_output << "        " << element_type << " " << stmt->element.value.value() << " = "
             << array_name << "[_i];\n";
    for (const NodeStmt* body_stmt : stmt->body->stmts) {
        gen_stmt(body_stmt);
    }
    m_output << "    }\n";
}

// say(expr);
void Generator::gen_say(const NodeStmtPrint* stmt) {
    m_output << "    std::cout << (";
    gen_expr(stmt->expr);
    m_output << ") << \"\\n\";\n";
}

// my_fn(a, b);   -- call it and throw the result away
void Generator::gen_expr_stmt(const NodeStmtExpr* stmt) {
    m_output << "    ";
    gen_expr(stmt->expr);
    m_output << ";\n";
}

// arr[i] = expr;
void Generator::gen_array_assign(const NodeStmtArrayAssign* stmt) {
    m_output << "    " << stmt->ident.value.value() << "[";
    gen_expr(stmt->index);
    m_output << "] = ";

    // For an array target, `ask` should read the array's element type.
    std::string ask_type;
    if (is_ask_call(stmt->expr)) {
        auto it = m_var_types.find(stmt->ident.value.value());
        if (it != m_var_types.end() && it->second.find("std::vector<") == 0) {
            // "std::vector<long>" -> "long"
            ask_type = it->second.substr(12, it->second.size() - 13);
        }
    }
    gen_with_ask_type(ask_type, stmt->expr);

    m_output << ";\n";
}

void Generator::gen_break() { m_output << "    break;\n"; }

void Generator::gen_continue() { m_output << "    continue;\n"; }

// switch (value) { case 1: ... default: ... }
// Each case body is a block followed by an explicit break, so there is no
// fallthrough.
void Generator::gen_switch(const NodeStmtSwitch* stmt) {
    m_output << "    switch (";
    gen_expr(stmt->expr);
    m_output << ") {\n";

    for (const NodeCase* node_case : stmt->cases) {
        m_output << "        case ";
        gen_expr(node_case->value);
        m_output << ": ";
        gen_scope(node_case->body, true);
        m_output << "        break;\n";
    }

    if (stmt->default_body.has_value()) {
        m_output << "        default: ";
        gen_scope(stmt->default_body.value(), true);
        m_output << "        break;\n";
    }

    m_output << "    }\n";
}

// push arr, value;
void Generator::gen_push(const NodeStmtPush* stmt) {
    m_output << "    " << stmt->ident.value.value() << ".push_back(";
    gen_expr(stmt->expr);
    m_output << ");\n";
}

// pop arr;
void Generator::gen_pop(const NodeStmtPop* stmt) {
    m_output << "    " << stmt->ident.value.value() << ".pop_back();\n";
}

// Picks the right gen_xxx() for this statement. The if/else chain below is the
// full list of statement kinds -- read it as a table of contents.
//
// ADDING A STATEMENT KIND? Add a gen_xxx() function above, declare it in
// generation.hpp, and add one branch here. If you forget this step the compiler
// will tell you at the bottom with "[BUG] gen_stmt: unhandled statement kind".
void Generator::gen_stmt(const NodeStmt* stmt) {
    if (std::holds_alternative<NodeStmtFn*>(stmt->var)) {
        // Function definitions are emitted by gen_prog(), not here.
        return;
    }
    if (std::holds_alternative<NodeScope*>(stmt->var)) {
        gen_block(std::get<NodeScope*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtExit*>(stmt->var)) {
        gen_gives(std::get<NodeStmtExit*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtLet*>(stmt->var)) {
        gen_my(std::get<NodeStmtLet*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtAssign*>(stmt->var)) {
        gen_assign(std::get<NodeStmtAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtCompoundAssign*>(stmt->var)) {
        gen_compound_assign(std::get<NodeStmtCompoundAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {
        gen_if(std::get<NodeStmtIf*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtWhile*>(stmt->var)) {
        gen_while(std::get<NodeStmtWhile*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtFor*>(stmt->var)) {
        gen_for(std::get<NodeStmtFor*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtForEach*>(stmt->var)) {
        gen_for_each(std::get<NodeStmtForEach*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPrint*>(stmt->var)) {
        gen_say(std::get<NodeStmtPrint*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtExpr*>(stmt->var)) {
        gen_expr_stmt(std::get<NodeStmtExpr*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtArrayAssign*>(stmt->var)) {
        gen_array_assign(std::get<NodeStmtArrayAssign*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtBreak*>(stmt->var)) {
        gen_break();
        return;
    }
    if (std::holds_alternative<NodeStmtContinue*>(stmt->var)) {
        gen_continue();
        return;
    }
    if (std::holds_alternative<NodeStmtSwitch*>(stmt->var)) {
        gen_switch(std::get<NodeStmtSwitch*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPush*>(stmt->var)) {
        gen_push(std::get<NodeStmtPush*>(stmt->var));
        return;
    }
    if (std::holds_alternative<NodeStmtPop*>(stmt->var)) {
        gen_pop(std::get<NodeStmtPop*>(stmt->var));
        return;
    }

    std::cerr << "[BUG] gen_stmt: unhandled statement kind. Add it in gen_stmt.hpp."
              << std::endl;
    exit(EXIT_FAILURE);
}