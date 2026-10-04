/**
 * @file parse_stmt.hpp
 * @brief Statement parsing for the Precious parser.
 *
 * Layout of this file:
 *
 *   1. Small shared helpers (token tests)
 *   2. Scopes and if/elif/else chains
 *   3. One parse_xxx() function per statement form
 *   4. parse_stmt() -- the dispatcher that tries each form in order
 */

// Included at the bottom of parser.hpp after the Parser class definition.

// ============================================================================
// 1. Small shared helpers
// ============================================================================

// The five type keywords. Adding a type means adding it here and nowhere else.
bool Parser::is_type_token(const TokenType type) {
    return type == TokenType::type_number_ || type == TokenType::type_word_ ||
           type == TokenType::type_question_ || type == TokenType::type_decimal_ ||
           type == TokenType::type_letter;
}

bool Parser::peek_is_type(const int offset) const {
    return peek(offset).has_value() && is_type_token(peek(offset).value().type);
}

// The five compound-assignment operators: += -= *= /= %=
bool Parser::is_compound_assign_token(const TokenType type) {
    return type == TokenType::pluseq || type == TokenType::minuseq ||
           type == TokenType::stareq || type == TokenType::fslasheq ||
           type == TokenType::moduloeq;
}

bool Parser::peek_is_compound_assign(const int offset) const {
    return peek(offset).has_value() && is_compound_assign_token(peek(offset).value().type);
}

// ============================================================================
// 2. Scopes and if/elif/else chains
// ============================================================================

// { <statements> }
std::optional<NodeScope*> Parser::parse_scope() {
    if (!try_consume(TokenType::open_curly).has_value()) {
        return {};
    }
    auto scope = new NodeScope{};
    // Keeps reading statements until parse_stmt() runs out of things it
    // recognises, which happens when the next token is the closing brace.
    while (auto stmt = parse_stmt()) {
        scope->stmts.push_back(stmt.value());
    }
    try_consume_err(TokenType::close_curly);
    return scope;
}

// Everything after the first `if`: any number of `elif`s, then an optional
// `else`. Returns nothing if the chain just ends.
std::optional<NodeIfPred*> Parser::parse_if_pred() {
    // elif (cond) { ... }
    if (try_consume(TokenType::elif)) {
        try_consume_err(TokenType::open_paren);
        auto elif = new NodeIfPredElif{};
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
        // Recurse: an elif can itself be followed by elif/else.
        elif->pred = parse_if_pred();
        return new NodeIfPred{elif};
    }

    // else { ... }
    if (try_consume(TokenType::else_)) {
        auto else_ = new NodeIfPredElse{};
        if (const auto scope = parse_scope()) {
            else_->scope = scope.value();
        } else {
            error_expected("scope");
        }
        return new NodeIfPred{else_};
    }

    return {};
}

// ============================================================================
// 3. One function per statement form
// ============================================================================

// gives(expr);
std::optional<NodeStmt*> Parser::parse_gives() {
    if (!peek().has_value() || peek().value().type != TokenType::exit ||
        !peek(1).has_value() || peek(1).value().type != TokenType::open_paren) {
        return {};
    }
    consume(); // gives
    consume(); // (

    auto stmt_exit = new NodeStmtExit{};
    if (const auto expr = parse_expr()) {
        stmt_exit->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::close_paren);
    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt_exit};
}

// my name [: type[ size]] = expr;
std::optional<NodeStmt*> Parser::parse_my() {
    const bool starts_here = peek().has_value() && peek().value().type == TokenType::let &&
                             peek(1).has_value() && peek(1).value().type == TokenType::ident &&
                             peek(2).has_value() &&
                             (peek(2).value().type == TokenType::eq ||
                              peek(2).value().type == TokenType::colon_);
    if (!starts_here) {
        return {};
    }
    consume(); // my

    auto stmt_let = new NodeStmtLet{};
    stmt_let->ident = consume();

    // Optional `: type` and, for arrays, `: type[size]` or `: type[]`.
    if (peek().has_value() && peek().value().type == TokenType::colon_) {
        consume();
        if (!peek_is_type()) {
            error_expected("type annotation (number, word, question, decimal, letter)");
        }
        stmt_let->type_annotation = consume().type;

        if (peek().has_value() && peek().value().type == TokenType::open_square) {
            consume();
            stmt_let->is_array = true;

            if (peek().has_value() && peek().value().type == TokenType::close_square) {
                consume(); // `[]` -- size comes from the initialiser
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
        stmt_let->type_annotation = std::nullopt;
    }

    consume(); // `=`. Safe: the guard above proved `=` or `:` came, and the `:`
               // branch above consumed the `:` if there was one.
    if (const auto expr = parse_expr()) {
        stmt_let->expr = expr.value();

        // An array literal needs a type to build the C++ std::vector.
        const bool is_array_literal =
            std::holds_alternative<NodeTerm*>(expr.value()->var) &&
            std::holds_alternative<NodeTermArrayLit*>(
                std::get<NodeTerm*>(expr.value()->var)->var);
        if (is_array_literal && !stmt_let->type_annotation.has_value()) {
            std::cerr << "[ERROR] Arrays need a type, precious! Use 'my "
                      << stmt_let->ident.value.value() << ": number[] = ...' (line "
                      << stmt_let->ident.line << ")" << std::endl;
            exit(EXIT_FAILURE);
        }
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt_let};
}

// arr[index] = expr;
std::optional<NodeStmt*> Parser::parse_array_assign() {
    if (!peek().has_value() || peek().value().type != TokenType::ident ||
        !peek(1).has_value() || peek(1).value().type != TokenType::open_square) {
        return {};
    }

    auto stmt = new NodeStmtArrayAssign{};
    stmt->ident = consume();
    consume(); // [

    if (auto index_expr = parse_expr()) {
        stmt->index = index_expr.value();
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
        stmt->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt};
}

// x += expr;   (also -= *= /= %=)
std::optional<NodeStmt*> Parser::parse_compound_assign() {
    if (!peek().has_value() || peek().value().type != TokenType::ident ||
        !peek_is_compound_assign(1)) {
        return {};
    }

    auto stmt = new NodeStmtCompoundAssign{};
    stmt->ident = consume();
    stmt->op = consume().type;

    if (auto expr = parse_expr()) {
        stmt->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt};
}

// ++x;  --x;  x++;  x--;
std::optional<NodeStmt*> Parser::parse_incdec_stmt() {
    const bool is_prefix = peek().has_value() &&
                           (peek().value().type == TokenType::plusplus ||
                            peek().value().type == TokenType::minusminus);
    if (!is_prefix) {
        // Not a prefix form, so it must be the postfix form: ident then ++/--.
        if (!peek().has_value() || peek().value().type != TokenType::ident ||
            !peek(1).has_value())
            return {};
        if (peek(1).value().type != TokenType::plusplus &&
            peek(1).value().type != TokenType::minusminus)
            return {};
    }

    // Both forms are exactly two tokens wide (`++ x` or `x ++`). Anything longer
    // is some other expression that happens to start the same way, such as
    // `++x + 1`, and should not be swallowed here.
    const size_t start = m_index;
    auto expr = parse_expr();
    if (!expr.has_value()) {
        error_expected("variable after '++' or '--'");
    }
    if (m_index - start != 2) {
        error_expected("';' after increment or decrement");
    }

    if (!peek().has_value() || peek().value().type != TokenType::semi) {
        error_expected("';'");
    }
    consume();

    return new NodeStmt{new NodeStmtExpr{expr.value()}};
}

// x = expr;
std::optional<NodeStmt*> Parser::parse_assign() {
    if (!peek().has_value() || peek().value().type != TokenType::ident ||
        !peek(1).has_value() || peek(1).value().type != TokenType::eq) {
        return {};
    }

    auto stmt = new NodeStmtAssign{};
    stmt->ident = consume();
    consume(); // =

    if (auto expr = parse_expr()) {
        stmt->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt};
}

// my_fn(a, b);   -- a function call used as a statement, value discarded
std::optional<NodeStmt*> Parser::parse_call_stmt() {
    if (!peek().has_value() || peek().value().type != TokenType::ident ||
        !peek(1).has_value() || peek(1).value().type != TokenType::open_paren) {
        return {};
    }

    auto fn_call = parse_fn_call();
    try_consume_err(TokenType::semi);

    // Wrap it up as an expression statement so codegen has one thing to handle.
    auto term = new NodeTerm{fn_call};
    auto expr = new NodeExpr{term};
    return new NodeStmt{new NodeStmtExpr{expr}};
}

// { ... }   -- a bare scope used as a statement
std::optional<NodeStmt*> Parser::parse_block() {
    if (!peek().has_value() || peek().value().type != TokenType::open_curly) {
        return {};
    }
    if (auto scope = parse_scope()) {
        return new NodeStmt{scope.value()};
    }
    error_expected("scope");
}

// if (cond) { ... } [elif (cond) { ... }] [else { ... }]
std::optional<NodeStmt*> Parser::parse_if() {
    if (!try_consume(TokenType::if_).has_value()) {
        return {};
    }
    try_consume_err(TokenType::open_paren);

    auto stmt_if = new NodeStmtIf{};
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
    return new NodeStmt{stmt_if};
}

// while (cond) { ... }
std::optional<NodeStmt*> Parser::parse_while() {
    if (!try_consume(TokenType::while_).has_value()) {
        return {};
    }
    try_consume_err(TokenType::open_paren);

    auto stmt_while = new NodeStmtWhile{};
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

    return new NodeStmt{stmt_while};
}

// for (item in arr) { ... }
std::optional<NodeStmt*> Parser::parse_for_each() {
    // Only matches after `for (` has been consumed, and only when the shape is
    // ident followed by `in`.
    if (!peek().has_value() || peek().value().type != TokenType::ident ||
        !peek(1).has_value() || peek(1).value().type != TokenType::in_) {
        return {};
    }

    auto stmt = new NodeStmtForEach{};
    stmt->element = consume(); // element name
    consume();                // `in`

    if (auto arr_expr = parse_expr()) {
        stmt->array = arr_expr.value();
    } else {
        error_expected("array expression");
    }
    try_consume_err(TokenType::close_paren);

    if (const auto scope = parse_scope()) {
        stmt->body = scope.value();
    } else {
        error_expected("scope");
    }

    return new NodeStmt{stmt};
}

// The `init` slot of a C-style for loop: `my x = e;`, `x = e;`, or empty `;`.
void Parser::parse_for_init(NodeStmtFor* stmt_for) {
    if (peek().has_value() && peek().value().type == TokenType::let) {
        consume(); // my
        auto stmt_let = new NodeStmtLet{};
        stmt_let->ident = consume();

        if (peek().has_value() && peek().value().type == TokenType::colon_) {
            consume();
            if (!peek_is_type()) {
                error_expected("type annotation (number, word, question, decimal, letter)");
            }
            stmt_let->type_annotation = consume().type;
        }

        try_consume_err(TokenType::eq);
        if (const auto expr = parse_expr()) {
            stmt_let->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::semi);
        stmt_for->init = new NodeStmt{stmt_let};
        return;
    }

    if (peek().has_value() && peek().value().type == TokenType::ident) {
        auto stmt_assign = new NodeStmtAssign{};
        stmt_assign->ident = consume();
        try_consume_err(TokenType::eq);
        if (const auto expr = parse_expr()) {
            stmt_assign->expr = expr.value();
        } else {
            error_expected("expression");
        }
        try_consume_err(TokenType::semi);
        stmt_for->init = new NodeStmt{stmt_assign};
        return;
    }

    // Nothing before the first `;` -- e.g. `for (; i < 5; i++)`.
    if (peek().has_value() && peek().value().type == TokenType::semi) {
        consume();
        stmt_for->init = nullptr;
        return;
    }

    error_expected("for loop init (variable declaration or assignment)");
}

// The `update` slot of a C-style for loop. Note this is not a full statement
// parser: it only accepts the forms that make sense to repeat every iteration.
void Parser::parse_for_update(NodeStmtFor* stmt_for) {
    // Nothing before `)` -- e.g. `for (my i = 0; i < 5; )`.
    if (peek().has_value() && peek().value().type == TokenType::close_paren) {
        stmt_for->update = nullptr;
        return;
    }

    if (!peek().has_value() || peek().value().type != TokenType::ident || !peek(1).has_value()) {
        error_expected("for loop update (assignment)");
    }

    // i++ / i--
    if (peek(1).value().type == TokenType::plusplus ||
        peek(1).value().type == TokenType::minusminus) {
        auto expr = parse_expr();
        if (!expr.has_value()) {
            error_expected("variable after '++' or '--'");
        }
        stmt_for->update = new NodeStmt{new NodeStmtExpr{expr.value()}};
        return;
    }

    // i += 1
    if (is_compound_assign_token(peek(1).value().type)) {
        auto stmt = new NodeStmtCompoundAssign{};
        stmt->ident = consume();
        stmt->op = consume().type;
        if (const auto expr = parse_expr()) {
            stmt->expr = expr.value();
        } else {
            error_expected("expression");
        }
        stmt_for->update = new NodeStmt{stmt};
        return;
    }

    // i = i + 1
    auto stmt_assign = new NodeStmtAssign{};
    stmt_assign->ident = consume();
    try_consume_err(TokenType::eq);
    if (const auto expr = parse_expr()) {
        stmt_assign->expr = expr.value();
    } else {
        error_expected("expression");
    }
    stmt_for->update = new NodeStmt{stmt_assign};
}

// for (init; cond; update) { ... }
std::optional<NodeStmt*> Parser::parse_for() {
    if (!try_consume(TokenType::for_).has_value()) {
        return {};
    }
    try_consume_err(TokenType::open_paren);

    // `for (item in arr)` is a different shape, so check for it before looking
    // for the init statement.
    if (auto stmt = parse_for_each()) {
        return stmt;
    }

    auto stmt_for = new NodeStmtFor{};
    parse_for_init(stmt_for);

    // Condition. Left empty means an infinite loop.
    if (peek().has_value() && peek().value().type != TokenType::semi) {
        if (const auto expr = parse_expr()) {
            stmt_for->condition = expr.value();
        } else {
            error_expected("expression");
        }
    } else {
        stmt_for->condition = nullptr;
    }
    try_consume_err(TokenType::semi);

    parse_for_update(stmt_for);
    try_consume_err(TokenType::close_paren);

    if (const auto scope = parse_scope()) {
        stmt_for->body = scope.value();
    } else {
        error_expected("scope");
    }

    return new NodeStmt{stmt_for};
}

// switch (value) { case 1: ... default: ... }
std::optional<NodeStmt*> Parser::parse_switch() {
    if (!peek().has_value() || peek().value().type != TokenType::switch_) {
        return {};
    }
    consume();
    try_consume_err(TokenType::open_paren);

    auto stmt_switch = new NodeStmtSwitch{};
    if (auto expr = parse_expr()) {
        stmt_switch->expr = expr.value();
    } else {
        error_expected("expression");
    }
    try_consume_err(TokenType::close_paren);
    try_consume_err(TokenType::open_curly);

    bool seen_default = false;
    while (peek().has_value() && peek().value().type != TokenType::close_curly) {
        // case <int literal>: <statements>
        if (try_consume(TokenType::case_)) {
            auto value_expr = parse_expr();
            if (!value_expr.has_value()) {
                error_expected("case value");
            }
            // C++ needs case labels to be compile-time constants, so only plain
            // integer literals are accepted.
            const bool is_int_literal =
                std::holds_alternative<NodeTerm*>(value_expr.value()->var) &&
                std::holds_alternative<NodeTermIntLit*>(
                    std::get<NodeTerm*>(value_expr.value()->var)->var);
            if (!is_int_literal) {
                std::cerr << "[ERROR] Case values must be integer literals, precious! (line "
                          << peek(-1).value().line << ")" << std::endl;
                exit(EXIT_FAILURE);
            }
            try_consume_err(TokenType::colon_);

            // Statements run until the next `case`, `default`, or `}`.
            auto body = new NodeScope{};
            while (peek().has_value() && peek().value().type != TokenType::case_ &&
                   peek().value().type != TokenType::default_ &&
                   peek().value().type != TokenType::close_curly) {
                if (auto stmt = parse_stmt()) {
                    body->stmts.push_back(stmt.value());
                } else {
                    break;
                }
            }

            auto node_case = new NodeCase{};
            node_case->value = value_expr.value();
            node_case->body = body;
            stmt_switch->cases.push_back(node_case);
            continue;
        }

        // default: <statements>
        if (try_consume(TokenType::default_)) {
            if (seen_default) {
                error_expected("only one 'default' per switch");
            }
            seen_default = true;
            try_consume_err(TokenType::colon_);

            auto body = new NodeScope{};
            while (peek().has_value() && peek().value().type != TokenType::close_curly) {
                if (auto stmt = parse_stmt()) {
                    body->stmts.push_back(stmt.value());
                } else {
                    break;
                }
            }
            stmt_switch->default_body = body;
            continue;
        }

        error_expected("case or default");
    }

    try_consume_err(TokenType::close_curly);
    return new NodeStmt{stmt_switch};
}

// say(expr);
std::optional<NodeStmt*> Parser::parse_say() {
    if (!peek().has_value() || peek().value().type != TokenType::print_ ||
        !peek(1).has_value() || peek(1).value().type != TokenType::open_paren) {
        return {};
    }
    consume(); // say
    consume(); // (

    auto stmt_print = new NodeStmtPrint{};
    if (const auto expr = parse_expr()) {
        stmt_print->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::close_paren);
    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt_print};
}

// break;
std::optional<NodeStmt*> Parser::parse_break() {
    if (!try_consume(TokenType::break_).has_value()) {
        return {};
    }
    try_consume_err(TokenType::semi);
    return new NodeStmt{new NodeStmtBreak{}};
}

// continue;
std::optional<NodeStmt*> Parser::parse_continue() {
    if (!try_consume(TokenType::continue_).has_value()) {
        return {};
    }
    try_consume_err(TokenType::semi);
    return new NodeStmt{new NodeStmtContinue{}};
}

// push arr, value;
std::optional<NodeStmt*> Parser::parse_push() {
    if (!peek().has_value() || peek().value().type != TokenType::push_) {
        return {};
    }
    consume();

    if (!peek().has_value() || peek().value().type != TokenType::ident) {
        error_expected("array name");
    }
    auto stmt = new NodeStmtPush{};
    stmt->ident = consume();

    try_consume_err(TokenType::comma_);
    if (auto expr = parse_expr()) {
        stmt->expr = expr.value();
    } else {
        error_expected("expression");
    }

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt};
}

// pop arr;
std::optional<NodeStmt*> Parser::parse_pop() {
    if (!peek().has_value() || peek().value().type != TokenType::pop_) {
        return {};
    }
    consume();

    if (!peek().has_value() || peek().value().type != TokenType::ident) {
        error_expected("array name");
    }
    auto stmt = new NodeStmtPop{};
    stmt->ident = consume();

    try_consume_err(TokenType::semi);
    return new NodeStmt{stmt};
}

// The parameter list of a `fn`, e.g. `(a, b: word, nums: number[])`.
void Parser::parse_fn_params(NodeStmtFn* fn_stmt) {
    try_consume_err(TokenType::open_paren);

    while (peek().has_value() && peek().value().type != TokenType::close_paren) {
        if (peek().has_value() && peek().value().type == TokenType::ident) {
            auto param = new NodeFnParam{};
            param->name = consume();

            if (peek().has_value() && peek().value().type == TokenType::colon_) {
                consume();
                if (!peek_is_type()) {
                    error_expected("type annotation (number, word, question, decimal, letter)");
                }
                param->type_annotation = consume().type;
            } else {
                param->type_annotation = std::nullopt;
            }

            // `[]` after the type marks an array parameter.
            if (peek().has_value() && peek().value().type == TokenType::open_square) {
                consume();
                param->isArray = true;
                if (peek().has_value() && peek().value().type == TokenType::close_square) {
                    consume();
                } else {
                    error_expected("']' for array parameter");
                }
            }

            fn_stmt->params.push_back(*param);
        }

        if (peek().has_value() && peek().value().type == TokenType::comma_) {
            consume();
        }
    }

    try_consume_err(TokenType::close_paren);
}

// The optional `-> type` after a function's parameter list.
void Parser::parse_fn_return_type(NodeStmtFn* fn_stmt) {
    if (!peek().has_value() || peek().value().type != TokenType::return_arrow) {
        fn_stmt->return_type = std::nullopt;
        return;
    }
    consume(); // ->

    if (!peek_is_type()) {
        error_expected("return type (number, word, question, decimal, letter)");
    }
    fn_stmt->return_type = consume().type;
}

// fn name(params) [-> type] { body }
std::optional<NodeStmt*> Parser::parse_fn() {
    if (!peek().has_value() || peek().value().type != TokenType::fn_) {
        return {};
    }
    consume(); // fn

    if (!peek().has_value() || peek().value().type != TokenType::ident) {
        error_expected("function name");
    }
    auto fn_stmt = new NodeStmtFn{};
    fn_stmt->name = consume();

    parse_fn_params(fn_stmt);
    parse_fn_return_type(fn_stmt);

    auto body = parse_scope();
    if (!body.has_value()) {
        error_expected("function body");
    }
    fn_stmt->body = body.value();

    return new NodeStmt{fn_stmt};
}

// ============================================================================
// 4. The dispatcher
// ============================================================================

// Reads every top-level definition.
//
// The top level is strict: only `fn` definitions are allowed there, and exactly
// one of them must be `fn the_precious() { ... }` -- the entry point, Precious's
// equivalent of C's main. (Imported files, once those exist, will be fn
// libraries with no entry.)
std::optional<NodeProg> Parser::parse_prog() {
    NodeProg prog;

    while (peek().has_value()) {
        auto stmt = parse_stmt();
        if (!stmt.has_value()) {
            error_expected("statement");
        }

        // At the top level the only legal thing is a function definition.
        if (!std::holds_alternative<NodeStmtFn*>(stmt.value()->var)) {
            std::cerr << "[ERROR] Only 'fn' definitions may live at top level! Move this "
                         "inside 'fn the_precious()' (line "
                      << peek(-1).value().line << ")" << std::endl;
            exit(EXIT_FAILURE);
        }

        const NodeStmtFn* fn = std::get<NodeStmtFn*>(stmt.value()->var);
        if (fn->name.value.value() == "the_precious") {
            register_entry_fn(&prog, std::get<NodeStmtFn*>(stmt.value()->var));
        }
        prog.stmts.push_back(stmt.value());
    }

    if (prog.entry_fn == nullptr) {
        std::cerr << "[ERROR] Where is the precious?! Every program needs an entry point: "
                     "'fn the_precious() { ... }'"
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    return prog;
}

// Applies the entry-point rules to a `fn the_precious`. Records it on the
// program if it is a legal entry point.
void Parser::register_entry_fn(NodeProg* prog, NodeStmtFn* fn) {
    if (prog->entry_fn != nullptr) {
        std::cerr << "[ERROR] There can be only one precious! Duplicate 'fn the_precious' (line "
                  << fn->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (!fn->params.empty()) {
        std::cerr << "[ERROR] The precious takes no arguments! Remove the parameters from "
                     "'fn the_precious' (line "
                  << fn->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fn->return_type.has_value()) {
        std::cerr << "[ERROR] The precious needs no return type! Remove '-> "
                  << to_string(fn->return_type.value()) << "' from 'fn the_precious' (line "
                  << fn->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }
    prog->entry_fn = fn;
}

// Tries every statement form in order and returns the first match. Returns
// nothing if none of them match, which is how parse_scope() knows it has hit the
// end of a block.
//
// ADDING A STATEMENT FORM? Add a parse_xxx() function above, declare it in
// parser.hpp, and add one line here. Nothing else needs to change.
std::optional<NodeStmt*> Parser::parse_stmt() {
    if (auto stmt = parse_gives()) return stmt;
    if (auto stmt = parse_my()) return stmt;
    if (auto stmt = parse_array_assign()) return stmt;
    if (auto stmt = parse_compound_assign()) return stmt;
    if (auto stmt = parse_incdec_stmt()) return stmt;
    if (auto stmt = parse_assign()) return stmt;
    if (auto stmt = parse_call_stmt()) return stmt;
    if (auto stmt = parse_block()) return stmt;
    if (auto stmt = parse_if()) return stmt;
    if (auto stmt = parse_while()) return stmt;
    if (auto stmt = parse_for()) return stmt;
    if (auto stmt = parse_switch()) return stmt;
    if (auto stmt = parse_say()) return stmt;
    if (auto stmt = parse_break()) return stmt;
    if (auto stmt = parse_continue()) return stmt;
    if (auto stmt = parse_push()) return stmt;
    if (auto stmt = parse_pop()) return stmt;
    if (auto stmt = parse_fn()) return stmt;
    return {};
}