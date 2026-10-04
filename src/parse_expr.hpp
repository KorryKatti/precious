/**
 * @file parse_expr.hpp
 * @brief Expression parsing for the Precious parser.
 *
 * An expression is one of two things:
 *
 *   a single value   42, "hi", x, f(1, 2), arr[i], -x, ++x, x++, [1, 2, 3]
 *                     ^ parse_term() reads one of these
 *
 *   two values joined by an operator   a + b, x * 2, n > 0, p and q
 *                     ^ parse_expr() joins them up
 *
 * The order they bind in comes from bin_prec() in tokenization.hpp, so
 * `a + b * c` correctly groups as `a + (b * c)`.
 */

// Included at the bottom of parser.hpp after the Parser class definition.

/**
 * Passed to parse_expr() to mean "just read one operand, do not let any operator
 * pull in more".
 *
 * It sits above every value in bin_prec() (the highest there is 4, for `* / %`),
 * so the operator loop in parse_expr() stops straight away and only the operand
 * itself gets read. That is what you want after something that must grab just
 * one thing, such as the number after a unary minus.
 */
constexpr int PREC_UNARY_OPERAND = 5;

// ---------------------------------------------------------------------------
// parse_term: one single value
// ---------------------------------------------------------------------------

// Reads one atomic value. Returns nothing if the next tokens are not the start
// of a value.
std::optional<NodeTerm*> Parser::parse_term() {
    // 42
    if (auto int_lit = try_consume(TokenType::int_lit)) {
        return new NodeTerm{new NodeTermIntLit{int_lit.value()}};
    }

    // "hello"
    if (auto string_lit = try_consume(TokenType::string_lit)) {
        return new NodeTerm{new NodeTermStringLit{string_lit.value()}};
    }

    // ++x  or  --x   (prefix form -- the operator comes first)
    if (peek().has_value() && (peek().value().type == TokenType::plusplus ||
                               peek().value().type == TokenType::minusminus)) {
        const bool is_increment = peek().value().type == TokenType::plusplus;
        consume(); // ++ or --
        if (!peek().has_value() || peek().value().type != TokenType::ident) {
            error_expected("variable name after '++' or '--'");
        }
        // The operand is always a plain variable, so build it directly.
        auto operand = new NodeExpr{new NodeTerm{new NodeTermIdent{consume()}}};

        auto term = new NodeTerm{};
        if (is_increment) {
            term->var = new NodeTermPreInc{operand};
        } else {
            term->var = new NodeTermPreDec{operand};
        }
        return term;
    }

    // -x   (unary minus; distinct from the `-` in `a - b`)
    if (peek().has_value() && peek().value().type == TokenType::minus) {
        consume(); // -
        auto operand = parse_expr(PREC_UNARY_OPERAND);
        if (!operand.has_value()) {
            error_expected("expression after unary minus");
        }
        return new NodeTerm{new NodeTermUnaryMinus{operand.value()}};
    }

    // !x   (logical not)
    if (try_consume(TokenType::bang).has_value()) {
        auto operand = parse_expr(PREC_UNARY_OPERAND);
        if (!operand.has_value()) {
            error_expected("expression");
        }
        return new NodeTerm{new NodeTermNot{operand.value()}};
    }

    // Everything else starts with either `(`, `[`, or a name.
    if (try_consume(TokenType::open_paren).has_value()) {
        // ( expr )   -- the parentheses are kept in the tree so codegen knows to
        // emit them back, which keeps grouping intact.
        auto inner = parse_expr();
        if (!inner.has_value()) {
            error_expected("expression");
        }
        try_consume_err(TokenType::close_paren);
        return new NodeTerm{new NodeTermParen{inner.value()}};
    }

    // [ expr, expr, ... ]
    if (peek().has_value() && peek().value().type == TokenType::open_square) {
        consume(); // [
        auto array_lit = new NodeTermArrayLit{};
        while (peek().has_value() && peek().value().type != TokenType::close_square) {
            auto element = parse_expr();
            if (!element.has_value()) {
                error_expected("expression in array literal");
            }
            array_lit->elements.push_back(element.value());

            // A comma is optional here, so `[1 2 3]` works too.
            if (peek().has_value() && peek().value().type == TokenType::comma_) {
                consume();
            }
        }
        if (!peek().has_value() || peek().value().type != TokenType::close_square) {
            error_expected("closing square bracket for array literal");
        }
        consume(); // ]
        return new NodeTerm{array_lit};
    }

    if (peek().has_value() && peek().value().type == TokenType::ident) {
        // name( ... )   -- a function call
        if (peek(1).has_value() && peek(1).value().type == TokenType::open_paren) {
            return new NodeTerm{parse_fn_call()};
        }

        // name[ ... ]   -- array indexing, and string indexing
        if (peek(1).has_value() && peek(1).value().type == TokenType::open_square) {
            auto array_index = new NodeTermArrayIndex{};
            array_index->ident = new NodeExpr{new NodeTerm{new NodeTermIdent{consume()}}};
            consume(); // [

            auto index = parse_expr();
            if (!index.has_value()) {
                error_expected("index expression");
            }
            array_index->index = index.value();

            if (!peek().has_value() || peek().value().type != TokenType::close_square) {
                error_expected("']'");
            }
            consume(); // ]
            return new NodeTerm{array_index};
        }

        // A bare name.
        return new NodeTerm{new NodeTermIdent{consume()}};
    }

    return {};
}

// ---------------------------------------------------------------------------
// parse_expr: values joined by operators
// ---------------------------------------------------------------------------

// Reads one operand, then keeps joining operators to it for as long as they
// bind tightly enough.
//
// `min_prec` is the loosest operator allowed to be pulled in. Passing a higher
// number means "leave the looser operators for whoever called me", which is how
// `a + b * c` ends up as `a + (b * c)` and not `(a + b) * c`.
//
// This is called "operator precedence climbing". The loop below is the whole
// algorithm -- there is no clever trick to it.
std::optional<NodeExpr*> Parser::parse_expr(const int min_prec) {
    auto first_term = parse_term();
    if (!first_term.has_value()) {
        return {};
    }

    // x++  or  x--   (postfix form -- the operator comes after)
    //
    // Handled right here, before the operator loop, because postfix binds
    // tighter than every operator. If it were handled in the loop, `x++ + 1`
    // could be grouped the wrong way.
    if (peek().has_value() && (peek().value().type == TokenType::plusplus ||
                               peek().value().type == TokenType::minusminus)) {
        const bool is_increment = peek().value().type == TokenType::plusplus;
        consume(); // ++ or --

        auto term = new NodeTerm{};
        if (is_increment) {
            term->var = new NodeTermPostInc{new NodeExpr{first_term.value()}};
        } else {
            term->var = new NodeTermPostDec{new NodeExpr{first_term.value()}};
        }
        first_term = term;
    }

    auto lhs = new NodeExpr{first_term.value()};

    while (true) {
        if (!peek().has_value()) {
            break; // ran out of input
        }

        const std::optional<int> prec = bin_prec(peek().value().type);
        if (!prec.has_value()) {
            break; // not an operator, so the expression ends here
        }
        if (prec.value() < min_prec) {
            break; // too loose to belong to this expression
        }

        const TokenType op_token = consume().type;
        const int rhs_min_prec = prec.value() + 1;

        auto rhs = parse_expr(rhs_min_prec);
        if (!rhs.has_value()) {
            error_expected("expression");
        }

        // Which operator was it? Turn the token into the BinOp the AST uses.
        BinOp op;
        if (op_token == TokenType::plus) {
            op = BinOp::Add;
        } else if (op_token == TokenType::minus) {
            op = BinOp::Sub;
        } else if (op_token == TokenType::star) {
            op = BinOp::Mul;
        } else if (op_token == TokenType::fslash) {
            op = BinOp::Div;
        } else if (op_token == TokenType::modulo) {
            op = BinOp::Mod;
        } else if (op_token == TokenType::eqeq) {
            op = BinOp::Eq;
        } else if (op_token == TokenType::noteq) {
            op = BinOp::NotEq;
        } else if (op_token == TokenType::lt) {
            op = BinOp::Lt;
        } else if (op_token == TokenType::gt) {
            op = BinOp::Gt;
        } else if (op_token == TokenType::lteq) {
            op = BinOp::LtEq;
        } else if (op_token == TokenType::gteq) {
            op = BinOp::GtEq;
        } else if (op_token == TokenType::and_) {
            op = BinOp::And;
        } else {
            op = BinOp::Or; // `or` is the only one left
        }

        // Fold what we have so far into the left side, so the next operator
        // attaches to this whole thing: that is what makes `a - b - c` group
        // left to right instead of right to left.
        auto combined = new NodeExpr{};
        combined->var = lhs->var;
        lhs->var = new NodeBinExpr{op, combined, rhs.value()};
    }

    return lhs;
}

// ---------------------------------------------------------------------------
// parse_fn_call: name(arg, arg, ...)
// ---------------------------------------------------------------------------

// Called with the name and its `(` already known to be there. Builds the call
// and leaves the cursor just after the closing `)`.
NodeTermFnCall* Parser::parse_fn_call() {
    auto fn_call = new NodeTermFnCall{};
    fn_call->name = consume(); // the function's name
    consume();                // (

    while (peek().has_value() && peek().value().type != TokenType::close_paren) {
        auto arg = parse_expr();
        if (!arg.has_value()) {
            error_expected("expression");
        }
        fn_call->args.push_back(arg.value());

        // A comma separates arguments; without one the list has ended.
        if (peek().has_value() && peek().value().type == TokenType::comma_) {
            consume();
        } else {
            break;
        }
    }

    try_consume_err(TokenType::close_paren);
    return fn_call;
}