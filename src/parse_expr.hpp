/**
 * @file parse_expr.hpp
 * @brief Expression parsing for the Precious parser.
 */

// Included at the bottom of parser.hpp after the Parser class definition.

std::optional<NodeTerm*> Parser::parse_term() {
    if (auto int_lit = try_consume(TokenType::int_lit)) {
        auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(int_lit.value());
        auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
        return term;
    }
    if (peek().has_value() && peek().value().type == TokenType::minus) {
        consume();
        auto expr = parse_expr(5);
        if (!expr.has_value()) {
            error_expected("expression after unary minus");
        }
        auto term_unary_minus = m_allocator.emplace<NodeTermUnaryMinus>(expr.value());
        auto term = m_allocator.emplace<NodeTerm>(term_unary_minus);
        return term;
    }

    if (peek().has_value() && peek().value().type == TokenType::ident) {
        if (peek(1).has_value() && peek(1).value().type == TokenType::open_paren) {
            auto fn_call = parse_fn_call();
            auto term = m_allocator.emplace<NodeTerm>(fn_call);
            return term;
        }
        if (peek(1).has_value() && peek(1).value().type == TokenType::open_square) {
            auto arr_index = m_allocator.emplace<NodeTermArrayIndex>();
            auto ident_token = consume();
            auto ident_ident = m_allocator.emplace<NodeTermIdent>(ident_token);
            auto ident_term = m_allocator.emplace<NodeTerm>(ident_ident);
            arr_index->ident = m_allocator.emplace<NodeExpr>(ident_term);
            consume();

            if (auto index_expr = parse_expr()) {
                arr_index->index = index_expr.value();
            } else {
                error_expected("index expression");
            }

            if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                error_expected("']'");
            }
            consume();
            auto term = m_allocator.emplace<NodeTerm>(arr_index);
            return term;
        }
        auto expr_ident = m_allocator.emplace<NodeTermIdent>(consume());
        auto term = m_allocator.emplace<NodeTerm>(expr_ident);
        return term;
    }
    if (auto open_parent = try_consume(TokenType::open_paren)) {
        auto expr = parse_expr();
        if (!expr.has_value()) {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        auto term_paren = m_allocator.emplace<NodeTermParen>(expr.value());
        auto term = m_allocator.emplace<NodeTerm>(term_paren);
        return term;
    }
    if (auto bang = try_consume(TokenType::bang)) {
        auto expr = parse_expr(5);
        if (!expr.has_value()) {
            error_expected("expression");
        }
        auto term_not = m_allocator.emplace<NodeTermNot>(expr.value());
        auto term = m_allocator.emplace<NodeTerm>(term_not);
        return term;
    }
    if (auto string_lit = try_consume(TokenType::string_lit)) {
        auto term_string_lit = m_allocator.emplace<NodeTermStringLit>(string_lit.value());
        auto term = m_allocator.emplace<NodeTerm>(term_string_lit);
        return term;
    }
    if (peek().has_value() && peek().value().type == TokenType::open_square) {
        auto arr_lit = m_allocator.emplace<NodeTermArrayLit>();
        consume();
        while (peek().has_value() && peek().value().type != TokenType::close_square) {
            if (auto expr = parse_expr()) {
                arr_lit->elements.push_back(expr.value());
            } else {
                error_expected("expression in array literal");
            }
            if (peek().has_value() && peek().value().type == TokenType::comma_) {
                consume();
            }
        }
        if (!peek().has_value() || peek().value().type != TokenType::close_square) {
            error_expected("closing square bracket for array literal");
        }
        consume();
        auto term = m_allocator.emplace<NodeTerm>(arr_lit);
        return term;
    }

    return {};
}

std::optional<NodeExpr*> Parser::parse_expr(const int min_prec) {
    std::optional<NodeTerm*> term_lhs = parse_term();
    if (!term_lhs.has_value()) {
        return {};
    }
    auto expr_lhs = m_allocator.emplace<NodeExpr>(term_lhs.value());

    while (true) {
        std::optional<Token> curr_token = peek();
        if (!curr_token.has_value()) {
            break;
        }
        std::optional<int> prec = bin_prec(curr_token.value().type);
        if (!prec.has_value() || prec.value() < min_prec) {
            break;
        }

        const auto [type, line, value] = consume();
        const int next_min_prec = prec.value() + 1;
        auto expr_rhs = parse_expr(next_min_prec);

        if (!expr_rhs.has_value()) {
            error_expected("expression");
        }

        BinOp op;
        switch (type) {
            case TokenType::plus:   op = BinOp::Add; break;
            case TokenType::minus:  op = BinOp::Sub; break;
            case TokenType::star:   op = BinOp::Mul; break;
            case TokenType::fslash: op = BinOp::Div; break;
            case TokenType::modulo: op = BinOp::Mod; break;
            case TokenType::eqeq:   op = BinOp::Eq; break;
            case TokenType::noteq:  op = BinOp::NotEq; break;
            case TokenType::lt:     op = BinOp::Lt; break;
            case TokenType::gt:     op = BinOp::Gt; break;
            case TokenType::lteq:   op = BinOp::LtEq; break;
            case TokenType::gteq:   op = BinOp::GtEq; break;
            case TokenType::and_:   op = BinOp::And; break;
            case TokenType::or_:    op = BinOp::Or; break;
            default: assert(false);
        }
        auto expr_lhs2 = m_allocator.emplace<NodeExpr>();
        expr_lhs2->var = expr_lhs->var;
        auto bin = m_allocator.emplace<NodeBinExpr>(op, expr_lhs2, expr_rhs.value());
        expr_lhs->var = bin;
    }
    return expr_lhs;
}

NodeTermFnCall* Parser::parse_fn_call() {
    auto fn_call = m_allocator.emplace<NodeTermFnCall>();
    fn_call->name = consume();
    consume();

    while (peek().has_value() && peek().value().type != TokenType::close_paren) {
        auto arg_expr = parse_expr();
        if (!arg_expr.has_value()) {
            error_expected("expression");
        }
        fn_call->args.push_back(arg_expr.value());

        if (peek().has_value() && peek().value().type == TokenType::comma_) {
            consume();
        } else {
            break;
        }
    }

    try_consume_err(TokenType::close_paren);
    return fn_call;
}
