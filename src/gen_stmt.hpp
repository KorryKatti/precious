/**
 * @file gen_stmt.hpp
 * @brief Statement code generation for the Precious code generator.
 */

// Included at the bottom of generation.hpp after the Generator class definition.

void Generator::gen_scope(const NodeScope* scope, bool inline_brace) {
    m_declared_scopes.push_back(m_declared.size());
    if (inline_brace) {
        m_output << " {\n";
    } else {
        m_output << "{\n";
    }
    for (const NodeStmt* stmt : scope->stmts) {
        gen_stmt(stmt);
    }
    m_output << "}\n";
    const size_t pop_count = m_declared.size() - m_declared_scopes.back();
    for (size_t i = 0; i < pop_count; i++) {
        m_var_types.erase(m_declared.back());
        m_declared.pop_back();
    }
    m_declared_scopes.pop_back();
}

void Generator::gen_if_pred(const NodeIfPred* pred, const std::string& end_label) {
    struct PredVisitor {
        Generator& gen;
        const std::string& end_label;
        void operator()(const NodeIfPredElif* elif) const {
            gen.m_output << " else if (";
            gen.gen_expr(elif->expr);
            gen.m_output << ")";
            gen.gen_scope(elif->scope, true);
            if (elif->pred.has_value()) {
                gen.gen_if_pred(elif->pred.value(), end_label);
            }
        }
        void operator()(const NodeIfPredElse* else_) const {
            gen.m_output << " else";
            gen.gen_scope(else_->scope, true);
        }
    };

    PredVisitor visitor{.gen = *this, .end_label = end_label};
    std::visit(visitor, pred->var);
}

void Generator::gen_stmt(const NodeStmt* stmt) {
    struct StmtVisitor {
        Generator& gen;

        void operator()(const NodeStmtExit* stmt_exit) const {
            gen.m_output << "    return ";
            gen.gen_expr(stmt_exit->expr);
            gen.m_output << ";\n";
        }

        void operator()(const NodeStmtLet* stmt_let) const {
            const std::string& name = stmt_let->ident.value.value();
            const size_t scope_start =
                gen.m_declared_scopes.empty() ? 0 : gen.m_declared_scopes.back();
            for (size_t i = scope_start; i < gen.m_declared.size(); i++) {
                if (gen.m_declared[i] == name) {
                    std::cerr << "[ERROR] We already has it! '" << name
                              << "' is already declared, precious! (line "
                              << stmt_let->ident.line << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            gen.m_declared.push_back(name);
            std::string c_type;
            std::string arr_suffix;
            if (stmt_let->type_annotation.has_value()) {
                c_type = gen.resolve_type(stmt_let->type_annotation.value());
                if (stmt_let->is_array) {
                    if (stmt_let->array_size.has_value()) {
                        arr_suffix = "[" + stmt_let->array_size.value().value.value() + "]";
                    } else {
                        arr_suffix = "[]";
                    }
                }
            } else {
                c_type = gen.infer_type(stmt_let->expr);
            }
            gen.m_var_types[name] = c_type;
            gen.m_output << "    " << c_type << " " << name << arr_suffix << " = ";
            gen.gen_expr(stmt_let->expr);
            gen.m_output << ";\n";
            // Track array size for for-each
            if (stmt_let->is_array && stmt_let->array_size.has_value()) {
                gen.m_array_sizes[name] = std::stoi(stmt_let->array_size.value().value.value());
            }
        }

        void operator()(const NodeStmtAssign* stmt_assign) const {
            gen.m_output << "    " << stmt_assign->ident.value.value() << " = ";
            gen.gen_expr(stmt_assign->expr);
            gen.m_output << ";\n";
        }

        void operator()(const NodeScope* scope) const { gen.gen_scope(scope); }

        void operator()(const NodeStmtIf* stmt_if) const {
            gen.m_output << "    if (";
            gen.gen_expr(stmt_if->expr);
            gen.m_output << ")";
            gen.gen_scope(stmt_if->scope, true);
            if (stmt_if->pred.has_value()) {
                std::string end_label = "end_if_" + std::to_string(gen.m_if_count++);
                gen.gen_if_pred(stmt_if->pred.value(), end_label);
            }
        }

        void operator()(const NodeStmtWhile* stmt_while) const {
            gen.m_output << "    while (";
            gen.gen_expr(stmt_while->expr);
            gen.m_output << ")";
            gen.gen_scope(stmt_while->scope, true);
        }

        void operator()(const NodeStmtFor* stmt_for) const {
            gen.m_output << "    for (";
            // Init
            if (stmt_for->init != nullptr) {
                if (std::holds_alternative<NodeStmtLet*>(stmt_for->init->var)) {
                    auto let = std::get<NodeStmtLet*>(stmt_for->init->var);
                    gen.m_output << gen.resolve_type(let->type_annotation.value_or(TokenType::type_number_))
                                 << " " << let->ident.value.value() << " = ";
                    gen.gen_expr(let->expr);
                } else if (std::holds_alternative<NodeStmtAssign*>(stmt_for->init->var)) {
                    auto assign = std::get<NodeStmtAssign*>(stmt_for->init->var);
                    gen.m_output << assign->ident.value.value() << " = ";
                    gen.gen_expr(assign->expr);
                }
            }
            gen.m_output << "; ";
            // Condition
            if (stmt_for->condition != nullptr) {
                gen.gen_expr(stmt_for->condition);
            }
            gen.m_output << "; ";
            // Update
            if (stmt_for->update != nullptr) {
                if (std::holds_alternative<NodeStmtAssign*>(stmt_for->update->var)) {
                    auto assign = std::get<NodeStmtAssign*>(stmt_for->update->var);
                    gen.m_output << assign->ident.value.value() << " = ";
                    gen.gen_expr(assign->expr);
                }
            }
            gen.m_output << ")";
            gen.gen_scope(stmt_for->body, true);
        }

        void operator()(const NodeStmtForEach* stmt_foreach) const {
            // Get array name and look up its size
            std::string arr_name;
            if (std::holds_alternative<NodeTerm*>(stmt_foreach->array->var)) {
                auto term = std::get<NodeTerm*>(stmt_foreach->array->var);
                if (std::holds_alternative<NodeTermIdent*>(term->var)) {
                    arr_name = std::get<NodeTermIdent*>(term->var)->ident.value.value();
                }
            }
            int arr_size = 0;
            auto size_it = gen.m_array_sizes.find(arr_name);
            if (size_it != gen.m_array_sizes.end()) {
                arr_size = size_it->second;
            } else {
                std::cerr << "[ERROR] Cannot determine array size for '" << arr_name
                          << "' in for-each, precious! Use 'my arr: type[N] = ...' (line "
                          << stmt_foreach->element.line << ")" << std::endl;
                exit(EXIT_FAILURE);
            }

            // Determine element type from array type
            std::string elem_type = "long";
            auto type_it = gen.m_var_types.find(arr_name);
            if (type_it != gen.m_var_types.end()) {
                elem_type = type_it->second;
                // Remove array suffix if present (e.g., "long[3]" -> "long")
                auto bracket_pos = elem_type.find('[');
                if (bracket_pos != std::string::npos) {
                    elem_type = elem_type.substr(0, bracket_pos);
                }
            }

            gen.m_output << "    for (long _i = 0; _i < " << arr_size << "; _i++) {\n";
            gen.m_output << "        " << elem_type << " " << stmt_foreach->element.value.value()
                         << " = " << arr_name << "[_i];\n";
            for (const NodeStmt* stmt : stmt_foreach->body->stmts) {
                gen.gen_stmt(stmt);
            }
            gen.m_output << "    }\n";
        }

        void operator()(const NodeStmtPrint* stmt_print) const {
            if (gen.is_string_expr(stmt_print->expr)) {
                gen.m_output << "    printf(\"%s\\n\", ";
            } else {
                gen.m_output << "    printf(\"%ld\\n\", ";
            }
            gen.gen_expr(stmt_print->expr);
            gen.m_output << ");\n";
        }

        void operator()(const NodeStmtFn*) const {
            // Function definitions are handled in gen_prog(), not gen_stmt()
        }

        void operator()(const NodeStmtExpr* stmt_expr) const {
            gen.m_output << "    ";
            gen.gen_expr(stmt_expr->expr);
            gen.m_output << ";\n";
        }

        void operator()(const NodeStmtArrayAssign* stmt_arr_assign) const {
            gen.m_output << "    " << stmt_arr_assign->ident.value.value() << "[";
            gen.gen_expr(stmt_arr_assign->index);
            gen.m_output << "] = ";
            gen.gen_expr(stmt_arr_assign->expr);
            gen.m_output << ";\n";
        }

        void operator()(const NodeStmtBreak*) const {
            gen.m_output << "    break;\n";
        }
        void operator()(const NodeStmtContinue*) const {
            gen.m_output << "    continue;\n";
        }

        void operator()(const NodeStmtSwitch* stmt_switch) const {
            gen.m_output << "    switch (";
            gen.gen_expr(stmt_switch->expr);
            gen.m_output << ") {\n";
            for (const NodeCase* node_case : stmt_switch->cases) {
                gen.m_output << "        case ";
                gen.gen_expr(node_case->value);
                gen.m_output << ": ";
                gen.gen_scope(node_case->body, true);
                gen.m_output << "        break;\n";
            }
            if (stmt_switch->default_body.has_value()) {
                gen.m_output << "        default: ";
                gen.gen_scope(stmt_switch->default_body.value(), true);
                gen.m_output << "        break;\n";
            }
            gen.m_output << "    }\n";
        }
    };

    StmtVisitor visitor{.gen = *this};
    std::visit(visitor, stmt->var);
}
