/**
 * @file gen_type.hpp
 * @brief Type resolution and inference for the Precious code generator.
 */

// Included at the bottom of generation.hpp after the Generator class definition.

std::string Generator::resolve_type(TokenType type) const {
    switch (type) {
        case TokenType::type_number_:
            return "long";
        case TokenType::type_word_:
            return "std::string";
        case TokenType::type_question_:
            return "long";
        case TokenType::type_decimal_:
            return "double";
        case TokenType::type_letter:
            return "char";
        default:
            return "long";
    }
}

std::string Generator::resolve_type(const std::string& type) const {
    if (type == "number") {
        return "long";
    }
    if (type == "word") {
        return "std::string";
    }
    if (type == "question") {
        return "long";
    }
    if (type == "decimal") {
        return "double";
    }
    if (type == "letter") {
        return "char";
    }
    return "long";
}

std::string Generator::infer_type(const NodeExpr* expr) const {
    // Handle binary expressions — if either operand is string, result is string
    if (std::holds_alternative<NodeBinExpr*>(expr->var)) {
        auto bin = std::get<NodeBinExpr*>(expr->var);
        std::string left = infer_type(bin->lhs);
        std::string right = infer_type(bin->rhs);
        if (left == "std::string" || right == "std::string")
            return "std::string";
        return "long";
    }
    if (!std::holds_alternative<NodeTerm*>(expr->var))
        return "long";
    auto term = std::get<NodeTerm*>(expr->var);
    if (std::holds_alternative<NodeTermStringLit*>(term->var))
        return "std::string";
    if (std::holds_alternative<NodeTermIntLit*>(term->var))
        return "long";
    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        auto ident = std::get<NodeTermIdent*>(term->var);
        auto it = m_var_types.find(ident->ident.value.value());
        if (it != m_var_types.end())
            return it->second;
    }
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        auto fn_call = std::get<NodeTermFnCall*>(term->var);
        auto it = m_fn_return_types.find(fn_call->name.value.value());
        if (it != m_fn_return_types.end())
            return it->second;
    }
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        auto arr_idx = std::get<NodeTermArrayIndex*>(term->var);
        if (std::holds_alternative<NodeTerm*>(arr_idx->ident->var)) {
            auto ident_term = std::get<NodeTerm*>(arr_idx->ident->var);
            if (std::holds_alternative<NodeTermIdent*>(ident_term->var)) {
                auto ident = std::get<NodeTermIdent*>(ident_term->var);
                auto it = m_var_types.find(ident->ident.value.value());
                if (it != m_var_types.end()) {
                    const std::string& base_type = it->second;
                    // std::vector<type>& or std::vector<type> -> extract element type
                    if (base_type.find("std::vector<") == 0) {
                        std::string vec_type = base_type;
                        // Strip trailing & if present (reference param)
                        if (!vec_type.empty() && vec_type.back() == '&')
                            vec_type.pop_back();
                        // Extract element type from std::vector<element_type>
                        size_t start = 12; // len("std::vector<")
                        size_t end = vec_type.size() - 1; // len(">")
                        return vec_type.substr(start, end - start);
                    }
                    // Fallback: strip trailing * for old pointer-style params
                    if (m_array_params.find(ident->ident.value.value()) != m_array_params.end() &&
                        base_type.size() >= 1 && base_type.back() == '*') {
                        return base_type.substr(0, base_type.size() - 1);
                    }
                    return base_type;
                }
            }
        }
    }
    return "long";
}

std::string Generator::infer_return_type(const NodeScope* body) const {
    for (const NodeStmt* stmt : body->stmts) {
        if (std::holds_alternative<NodeStmtExit*>(stmt->var)) {
            auto exit_stmt = std::get<NodeStmtExit*>(stmt->var);
            return infer_type(exit_stmt->expr);
        }
        if (std::holds_alternative<NodeScope*>(stmt->var)) {
            auto nested = infer_return_type(std::get<NodeScope*>(stmt->var));
            if (nested != "void")
                return nested;
        }
        if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {
            auto if_stmt = std::get<NodeStmtIf*>(stmt->var);
            auto then_type = infer_return_type(if_stmt->scope);
            if (then_type != "void")
                return then_type;
            if (if_stmt->pred.has_value()) {
                auto pred_type = infer_return_type_pred(if_stmt->pred.value());
                if (pred_type != "void")
                    return pred_type;
            }
        }
        if (std::holds_alternative<NodeStmtWhile*>(stmt->var)) {
            auto while_type = infer_return_type(std::get<NodeStmtWhile*>(stmt->var)->scope);
            if (while_type != "void")
                return while_type;
        }
        if (std::holds_alternative<NodeStmtSwitch*>(stmt->var)) {
            auto switch_stmt = std::get<NodeStmtSwitch*>(stmt->var);
            for (const NodeCase* node_case : switch_stmt->cases) {
                auto case_type = infer_return_type(node_case->body);
                if (case_type != "void")
                    return case_type;
            }
            if (switch_stmt->default_body.has_value()) {
                auto default_type = infer_return_type(switch_stmt->default_body.value());
                if (default_type != "void")
                    return default_type;
            }
        }
    }
    return "void";
}

std::string Generator::infer_return_type_pred(const NodeIfPred* pred) const {
    if (std::holds_alternative<NodeIfPredElif*>(pred->var)) {
        auto elif = std::get<NodeIfPredElif*>(pred->var);
        auto then_type = infer_return_type(elif->scope);
        if (then_type != "void")
            return then_type;
        if (elif->pred.has_value()) {
            return infer_return_type_pred(elif->pred.value());
        }
    } else if (std::holds_alternative<NodeIfPredElse*>(pred->var)) {
        return infer_return_type(std::get<NodeIfPredElse*>(pred->var)->scope);
    }
    return "void";
}

bool Generator::is_string_expr(const NodeExpr* expr) const {
    if (!std::holds_alternative<NodeTerm*>(expr->var))
        return false;
    auto term = std::get<NodeTerm*>(expr->var);
    if (std::holds_alternative<NodeTermStringLit*>(term->var))
        return true;
    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        auto ident = std::get<NodeTermIdent*>(term->var);
        auto it = m_var_types.find(ident->ident.value.value());
        if (it != m_var_types.end() && it->second == "std::string")
            return true;
    }
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        auto fn_call = std::get<NodeTermFnCall*>(term->var);
        auto it = m_fn_return_types.find(fn_call->name.value.value());
        if (it != m_fn_return_types.end() && it->second == "std::string")
            return true;
    }
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        auto arr_idx = std::get<NodeTermArrayIndex*>(term->var);
        if (std::holds_alternative<NodeTerm*>(arr_idx->ident->var)) {
            auto ident_term = std::get<NodeTerm*>(arr_idx->ident->var);
            if (std::holds_alternative<NodeTermIdent*>(ident_term->var)) {
                auto ident = std::get<NodeTermIdent*>(ident_term->var);
                auto it = m_var_types.find(ident->ident.value.value());
                if (it != m_var_types.end() && it->second.find("std::string") != std::string::npos)
                    return true;
            }
        }
    }
    return false;
}

bool Generator::has_return(const NodeScope* body) const {
    for (const NodeStmt* stmt : body->stmts) {
        if (std::holds_alternative<NodeStmtExit*>(stmt->var)) {
            return true;
        }
        if (std::holds_alternative<NodeScope*>(stmt->var)) {
            if (has_return(std::get<NodeScope*>(stmt->var))) {
                return true;
            }
        }
        if (std::holds_alternative<NodeStmtIf*>(stmt->var)) {
            auto if_stmt = std::get<NodeStmtIf*>(stmt->var);
            if (has_return(if_stmt->scope)) {
                return true;
            }
            if (if_stmt->pred.has_value()) {
                if (has_return_pred(if_stmt->pred.value())) {
                    return true;
                }
            }
        }
        if (std::holds_alternative<NodeStmtWhile*>(stmt->var)) {
            if (has_return(std::get<NodeStmtWhile*>(stmt->var)->scope)) {
                return true;
            }
        }
        if (std::holds_alternative<NodeStmtSwitch*>(stmt->var)) {
            auto switch_stmt = std::get<NodeStmtSwitch*>(stmt->var);
            for (const NodeCase* node_case : switch_stmt->cases) {
                if (has_return(node_case->body)) {
                    return true;
                }
            }
            if (switch_stmt->default_body.has_value() &&
                has_return(switch_stmt->default_body.value())) {
                return true;
            }
        }
    }
    return false;
}

bool Generator::has_return_pred(const NodeIfPred* pred) const {
    if (std::holds_alternative<NodeIfPredElif*>(pred->var)) {
        auto elif = std::get<NodeIfPredElif*>(pred->var);
        if (has_return(elif->scope))
            return true;
        if (elif->pred.has_value()) {
            return has_return_pred(elif->pred.value());
        }
    } else if (std::holds_alternative<NodeIfPredElse*>(pred->var)) {
        return has_return(std::get<NodeIfPredElse*>(pred->var)->scope);
    }
    return false;
}
