/**
 * @file parse_stmt.hpp
 * @brief Statement parsing for the Precious parser.
 */

// Included at the bottom of parser.hpp after the Parser class definition.

std::optional<NodeScope*> Parser::parse_scope() {
    if (!try_consume(TokenType::open_curly).has_value()) {
        return {};
    }
    auto scope = m_allocator.emplace<NodeScope>();
    while (auto stmt = parse_stmt()) {
        scope->stmts.push_back(stmt.value());
    }
    try_consume_err(TokenType::close_curly);
    return scope;
}

std::optional<NodeIfPred*> Parser::parse_if_pred() {
    if (try_consume(TokenType::elif)) {
        try_consume_err(TokenType::open_paren);
        const auto elif = m_allocator.alloc<NodeIfPredElif>();
        if (auto expr = parse_expr()) {
            elif->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        if (auto scope = parse_scope()) {
            elif->scope = scope.value();
        } else {
            error_expected("scope");
        }
        elif->pred = parse_if_pred();
        auto pred = m_allocator.emplace<NodeIfPred>(elif);
        return pred;
    }
    if (try_consume(TokenType::else_)) {
        auto else_ = m_allocator.alloc<NodeIfPredElse>();
        if (const auto scope = parse_scope()) {
            else_->scope = scope.value();
        } else {
            error_expected("scope");
        }
        auto pred = m_allocator.emplace<NodeIfPred>(else_);
        return pred;
    }
    return {};
}

std::optional<NodeStmt*> Parser::parse_stmt() {
    // gives <expr>;
    if (peek().has_value() && peek().value().type == TokenType::exit && peek(1).has_value() &&
        peek(1).value().type == TokenType::open_paren) {
        consume();
        consume();
        auto stmt_exit = m_allocator.alloc<NodeStmtExit>();
        if (const auto node_expr = parse_expr()) {
            stmt_exit->expr = node_expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_exit);
        return stmt;
    }

    // my <ident> [: type] = <expr>;
    if (peek().has_value() && peek().value().type == TokenType::let && peek(1).has_value() &&
        peek(1).value().type == TokenType::ident && peek(2).has_value() &&
        (peek(2).value().type == TokenType::eq || peek(2).value().type == TokenType::colon_)) {
        consume();
        auto stmt_let = m_allocator.alloc<NodeStmtLet>();
        stmt_let->ident = consume();
        if (peek().has_value() && peek().value().type == TokenType::colon_) {
            consume();
            if (peek().has_value() && (peek().value().type == TokenType::type_number_ ||
                                       peek().value().type == TokenType::type_word_ ||
                                       peek().value().type == TokenType::type_question_ ||
                                       peek().value().type == TokenType::type_decimal_ ||
                                       peek().value().type == TokenType::type_letter)) {
                stmt_let->type_annotation = consume().type;

                if (peek().has_value() && peek().value().type == TokenType::open_square) {
                    consume();
                    stmt_let->is_array = true;

                    if (peek().has_value() && peek().value().type == TokenType::close_square) {
                        consume();
                    } else if (peek().has_value() && peek().value().type == TokenType::int_lit) {
                        stmt_let->array_size = consume();
                        if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                            error_expected("']'");
                        }
                        consume();
                    } else {
                        error_expected("']' or array size");
                    }
                }
            } else {
                error_expected("type annotation (number, word, question, decimal, letter)");
            }
        } else {
            stmt_let->type_annotation = std::nullopt;
        }

        consume();
        if (const auto expr = parse_expr()) {
            stmt_let->expr = expr.value();
            if (std::holds_alternative<NodeTerm*>(expr.value()->var)) {
                auto term = std::get<NodeTerm*>(expr.value()->var);
                if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
                    if (!stmt_let->type_annotation.has_value()) {
                        std::cerr << "[ERROR] Arrays need a type, precious! Use 'my "
                                  << stmt_let->ident.value.value() << ": number[] = ...' (line "
                                  << stmt_let->ident.line << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
            }
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_let);
        return stmt;
    }

    // Array assignment: ident[expr] = expr;
    if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
        peek(1).value().type == TokenType::open_square) {
        auto arr_assign = m_allocator.emplace<NodeStmtArrayAssign>();
        arr_assign->ident = consume();

        consume();
        if (auto index_expr = parse_expr()) {
            arr_assign->index = index_expr.value();
        } else {
            error_expected("index expression");
        }

        if (!peek().has_value() || peek().value().type != TokenType::close_square) {
            error_expected("']'");
        }
        consume();

        if (!peek().has_value() || peek().value().type != TokenType::eq) {
            error_expected("'='");
        }
        consume();

        if (auto expr = parse_expr()) {
            arr_assign->expr = expr.value();
        } else {
            error_expected("expression");
        }

        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(arr_assign);
        return stmt;
    }

    // <ident> (+=|-=|*=|/=|%=) <expr>; — compound assignment
    if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value()) {
        bool is_compound = false;
        switch (peek(1).value().type) {
            case TokenType::pluseq:
            case TokenType::minuseq:
            case TokenType::stareq:
            case TokenType::fslasheq:
            case TokenType::moduloeq:
                is_compound = true;
                break;
            default:
                break;
        }
        if (is_compound) {
            auto compound = m_allocator.alloc<NodeStmtCompoundAssign>();
            compound->ident = consume();
            compound->op = consume().type;
            if (auto expr = parse_expr()) {
                compound->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            auto stmt = m_allocator.emplace<NodeStmt>(compound);
            return stmt;
        }
    }

    // <ident> = <expr>;
    if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
        peek(1).value().type == TokenType::eq) {
        const auto assign = m_allocator.alloc<NodeStmtAssign>();
        assign->ident = consume();
        consume();
        if (auto expr = parse_expr()) {
            assign->expr = expr.value();

        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(assign);
        return stmt;
    }

    // <ident>(...); — function call as statement
    if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() &&
        peek(1).value().type == TokenType::open_paren) {
        auto fn_call = parse_fn_call();
        try_consume_err(TokenType::semi);
        auto term = m_allocator.emplace<NodeTerm>(fn_call);
        auto expr = m_allocator.emplace<NodeExpr>(term);
        auto stmt_expr = m_allocator.emplace<NodeStmtExpr>(expr);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_expr);
        return stmt;
    }

    // { ... } (scoped block)
    if (peek().has_value() && peek().value().type == TokenType::open_curly) {
        if (auto scope = parse_scope()) {
            auto stmt = m_allocator.emplace<NodeStmt>(scope.value());
            return stmt;
        }
        error_expected("scope");
    }
    // if (<expr>) { ... } [elif ...] [else ...]
    if (auto if_ = try_consume(TokenType::if_)) {
        try_consume_err(TokenType::open_paren);
        auto stmt_if = m_allocator.alloc<NodeStmtIf>();
        if (const auto expr = parse_expr()) {
            stmt_if->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        if (const auto scope = parse_scope()) {
            stmt_if->scope = scope.value();
        } else {
            error_expected("scope");
        }
        stmt_if->pred = parse_if_pred();
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_if);
        return stmt;
    }
    if (auto while_ = try_consume(TokenType::while_)) {
        try_consume_err(TokenType::open_paren);
        auto stmt_while = m_allocator.alloc<NodeStmtWhile>();
        if (const auto expr = parse_expr()) {
            stmt_while->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        if (const auto scope = parse_scope()) {
            stmt_while->scope = scope.value();
        } else {
            error_expected("scope");
        }
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_while);
        return stmt;
    }

    // for (<item> in <array>) { <body> }  OR  for (<init>; <cond>; <update>) { <body> }
    if (auto for_ = try_consume(TokenType::for_)) {
        try_consume_err(TokenType::open_paren);

        // Peek ahead to check for "in" keyword (for-each pattern)
        if (peek().has_value() && peek().value().type == TokenType::ident &&
            peek(1).has_value() && peek(1).value().type == TokenType::in_) {
            // for-each: for (item in arr) { ... }
            auto stmt_foreach = m_allocator.alloc<NodeStmtForEach>();
            stmt_foreach->element = consume(); // element name
            consume(); // consume 'in'
            if (auto arr_expr = parse_expr()) {
                stmt_foreach->array = arr_expr.value();
            } else {
                error_expected("array expression");
            }
            try_consume_err(TokenType::close_paren);
            if (const auto scope = parse_scope()) {
                stmt_foreach->body = scope.value();
            } else {
                error_expected("scope");
            }
            auto stmt = m_allocator.emplace<NodeStmt>(stmt_foreach);
            return stmt;
        }

        // Regular for loop: for (init; cond; update) { ... }
        auto stmt_for = m_allocator.alloc<NodeStmtFor>();

        // Parse init statement: either "my x = expr" or "x = expr"
        if (peek().has_value() && peek().value().type == TokenType::let) {
            // my <ident> = <expr>;
            consume();
            auto stmt_let = m_allocator.alloc<NodeStmtLet>();
            stmt_let->ident = consume();
            if (peek().has_value() && peek().value().type == TokenType::colon_) {
                consume();
                if (peek().has_value() && (peek().value().type == TokenType::type_number_ ||
                                           peek().value().type == TokenType::type_word_ ||
                                           peek().value().type == TokenType::type_question_ ||
                                           peek().value().type == TokenType::type_decimal_ ||
                                           peek().value().type == TokenType::type_letter)) {
                    stmt_let->type_annotation = consume().type;
                } else {
                    error_expected("type annotation (number, word, question, decimal, letter)");
                }
            }
            try_consume_err(TokenType::eq);
            if (const auto expr = parse_expr()) {
                stmt_let->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            stmt_for->init = m_allocator.emplace<NodeStmt>(stmt_let);
        } else if (peek().has_value() && peek().value().type == TokenType::ident) {
            // <ident> = <expr>;
            auto assign = m_allocator.alloc<NodeStmtAssign>();
            assign->ident = consume();
            try_consume_err(TokenType::eq);
            if (const auto expr = parse_expr()) {
                assign->expr = expr.value();
            } else {
                error_expected("expression");
            }
            try_consume_err(TokenType::semi);
            stmt_for->init = m_allocator.emplace<NodeStmt>(assign);
        } else if (peek().has_value() && peek().value().type == TokenType::semi) {
            // Empty init: for (; cond; update)
            consume();
            stmt_for->init = nullptr;
        } else {
            error_expected("for loop init (variable declaration or assignment)");
        }

        // Parse condition
        if (peek().has_value() && peek().value().type != TokenType::semi) {
            if (const auto expr = parse_expr()) {
                stmt_for->condition = expr.value();
            } else {
                error_expected("expression");
            }
        } else {
            stmt_for->condition = nullptr; // empty condition = infinite loop
        }
        try_consume_err(TokenType::semi);

        // Parse update: <ident> (=|+=|-=|*=|/=|%=) <expr>
        if (peek().has_value() && peek().value().type != TokenType::close_paren) {
            if (peek().has_value() && peek().value().type == TokenType::ident &&
                peek(1).has_value() &&
                (peek(1).value().type == TokenType::pluseq ||
                 peek(1).value().type == TokenType::minuseq ||
                 peek(1).value().type == TokenType::stareq ||
                 peek(1).value().type == TokenType::fslasheq ||
                 peek(1).value().type == TokenType::moduloeq)) {
                auto compound = m_allocator.alloc<NodeStmtCompoundAssign>();
                compound->ident = consume();
                compound->op = consume().type;
                if (const auto expr = parse_expr()) {
                    compound->expr = expr.value();
                } else {
                    error_expected("expression");
                }
                stmt_for->update = m_allocator.emplace<NodeStmt>(compound);
            } else if (peek().has_value() && peek().value().type == TokenType::ident) {
                auto assign = m_allocator.alloc<NodeStmtAssign>();
                assign->ident = consume();
                try_consume_err(TokenType::eq);
                if (const auto expr = parse_expr()) {
                    assign->expr = expr.value();
                } else {
                    error_expected("expression");
                }
                stmt_for->update = m_allocator.emplace<NodeStmt>(assign);
            } else {
                error_expected("for loop update (assignment)");
            }
        } else {
            stmt_for->update = nullptr; // empty update
        }
        try_consume_err(TokenType::close_paren);

        // Parse body
        if (const auto scope = parse_scope()) {
            stmt_for->body = scope.value();
        } else {
            error_expected("scope");
        }

        auto stmt = m_allocator.emplace<NodeStmt>(stmt_for);
        return stmt;
    }

    // switch (expr) { case <int>: <stmts> ... default: <stmts> }
    if (peek().has_value() && peek().value().type == TokenType::switch_) {
        consume();
        try_consume_err(TokenType::open_paren);
        auto stmt_switch = m_allocator.alloc<NodeStmtSwitch>();
        if (auto expr = parse_expr()) {
            stmt_switch->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        try_consume_err(TokenType::open_curly);

        bool seen_default = false;
        while (peek().has_value() && peek().value().type != TokenType::close_curly) {
            if (try_consume(TokenType::case_)) {
                auto value_expr = parse_expr();
                if (!value_expr.has_value()) {
                    error_expected("case value");
                }
                if (!std::holds_alternative<NodeTerm*>(value_expr.value()->var) ||
                    !std::holds_alternative<NodeTermIntLit*>(
                        std::get<NodeTerm*>(value_expr.value()->var)->var)) {
                    std::cerr << "[ERROR] Case values must be integer literals, precious! (line "
                              << peek(-1).value().line << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
                try_consume_err(TokenType::colon_);
                auto body = m_allocator.alloc<NodeScope>();
                while (peek().has_value() && peek().value().type != TokenType::case_ &&
                       peek().value().type != TokenType::default_ &&
                       peek().value().type != TokenType::close_curly) {
                    if (auto stmt = parse_stmt()) {
                        body->stmts.push_back(stmt.value());
                    } else {
                        break;
                    }
                }
                auto node_case = m_allocator.emplace<NodeCase>();
                node_case->value = value_expr.value();
                node_case->body = body;
                stmt_switch->cases.push_back(node_case);
            } else if (try_consume(TokenType::default_)) {
                if (seen_default) {
                    error_expected("only one 'default' per switch");
                }
                seen_default = true;
                try_consume_err(TokenType::colon_);
                auto body = m_allocator.alloc<NodeScope>();
                while (peek().has_value() && peek().value().type != TokenType::close_curly) {
                    if (auto stmt = parse_stmt()) {
                        body->stmts.push_back(stmt.value());
                    } else {
                        break;
                    }
                }
                stmt_switch->default_body = body;
            } else {
                error_expected("case or default");
            }
        }
        try_consume_err(TokenType::close_curly);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_switch);
        return stmt;
    }

    // say(<expr>);
    if (peek().has_value() && peek().value().type == TokenType::print_ && peek(1).has_value() &&
        peek(1).value().type == TokenType::open_paren) {
        consume();
        consume();
        auto stmt_print = m_allocator.alloc<NodeStmtPrint>();
        if (const auto expr = parse_expr()) {
            stmt_print->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_print);
        return stmt;
    }

    if (try_consume(TokenType::break_)) {
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(m_allocator.alloc<NodeStmtBreak>());
        return stmt;
    }

    if (try_consume(TokenType::continue_)) {
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(m_allocator.alloc<NodeStmtContinue>());
        return stmt;
    }

    // push <ident>, <expr>;
    if (peek().has_value() && peek().value().type == TokenType::push_) {
        consume();
        auto stmt_push = m_allocator.alloc<NodeStmtPush>();
        if (!peek().has_value() || peek().value().type != TokenType::ident) {
            error_expected("array name");
        }
        stmt_push->ident = consume();
        try_consume_err(TokenType::comma_);
        if (auto expr = parse_expr()) {
            stmt_push->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_push);
        return stmt;
    }

    // pop <ident>;
    if (peek().has_value() && peek().value().type == TokenType::pop_) {
        consume();
        auto stmt_pop = m_allocator.alloc<NodeStmtPop>();
        if (!peek().has_value() || peek().value().type != TokenType::ident) {
            error_expected("array name");
        }
        stmt_pop->ident = consume();
        try_consume_err(TokenType::semi);
        auto stmt = m_allocator.emplace<NodeStmt>(stmt_pop);
        return stmt;
    }

    // fn name(params) [-> type] { body }
    if (peek().has_value() && peek().value().type == TokenType::fn_) {
        auto fn_stmt = m_allocator.emplace<NodeStmtFn>();
        consume();
        if (!peek().has_value() || peek().value().type != TokenType::ident) {
            error_expected("function name");
        }
        fn_stmt->name = consume();
        if (!peek().has_value() || peek().value().type != TokenType::open_paren) {
            error_expected("open paren");
        }
        consume();

        while (peek().has_value() && peek().value().type != TokenType::close_paren) {
            if (peek().has_value() && peek().value().type == TokenType::ident) {
                auto param = m_allocator.alloc<NodeFnParam>();
                param->name = consume();
                if (peek().has_value() && peek().value().type == TokenType::colon_) {
                    consume();
                    if (peek().has_value() &&
                        (peek().value().type == TokenType::type_number_ ||
                         peek().value().type == TokenType::type_word_ ||
                         peek().value().type == TokenType::type_question_ ||
                         peek().value().type == TokenType::type_decimal_ ||
                         peek().value().type == TokenType::type_letter)) {
                        param->type_annotation = consume().type;
                    } else {
                        error_expected(
                            "type annotation (number, word, question, decimal, letter)");
                    }
                } else {
                    param->type_annotation = std::nullopt;
                }
                if (peek().has_value() && peek().value().type == TokenType::open_square){
                    consume();
                    param->isArray = true;
                    if (peek().has_value() && peek().value().type == TokenType::close_square){
                        consume();
                    }
                    else {
                        error_expected("']' for array parameter");
                    }
                }

                fn_stmt->params.push_back(*param);
            }
            if (peek().has_value() && peek().value().type == TokenType::comma_) {
                consume();
            }
        }
        consume();
        if (peek().has_value() && peek().value().type == TokenType::return_arrow) {
            consume();
            if (peek().has_value() && (peek().value().type == TokenType::type_number_ ||
                                       peek().value().type == TokenType::type_word_ ||
                                       peek().value().type == TokenType::type_question_ ||
                                       peek().value().type == TokenType::type_decimal_ ||
                                       peek().value().type == TokenType::type_letter)) {
                switch (peek().value().type) {
                    case TokenType::type_number_:
                        fn_stmt->return_type = "number";
                        break;
                    case TokenType::type_word_:
                        fn_stmt->return_type = "word";
                        break;
                    case TokenType::type_question_:
                        fn_stmt->return_type = "question";
                        break;
                    case TokenType::type_decimal_:
                        fn_stmt->return_type = "decimal";
                        break;
                    case TokenType::type_letter:
                        fn_stmt->return_type = "letter";
                        break;
                    default:
                        break;
                }
                consume();
            } else {
                error_expected("return type (number, word, question, decimal, letter)");
            }
        } else {
            fn_stmt->return_type = std::nullopt;
        }

        auto body = parse_scope();
        if (!body.has_value()) {
            error_expected("function body");
        }
        fn_stmt->body = body.value();
        auto stmt = m_allocator.emplace<NodeStmt>(fn_stmt);
        return stmt;
    }


    return {};
}
