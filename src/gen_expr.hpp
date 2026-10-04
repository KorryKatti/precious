/**
 * @file gen_expr.hpp
 * @brief Expression code generation for the Precious code generator.
 *
 * Layout:
 *   1. gen_term()   -- atomic values: literals, identifiers, calls, ++/--
 *   2. gen_bin_expr() -- one binary operator
 *   3. gen_expr()   -- an expression is either a single term or a bin expr
 *
 * Each AST node holds a std::variant: a pointer to one of several node structs,
 * with a hidden tag saying which. To read the tag we use:
 *   std::holds_alternative<T>(v)  -> "is it a T?"
 *   std::get<T>(v)                -> "give me the T"
 * The gen_* functions below use a plain if/else chain over those, so you can
 * read them top to bottom and see every node type the language can produce.
 */

void Generator::gen_term(const NodeTerm* term) {
    // --- Values you type directly ---

    if (std::holds_alternative<NodeTermIntLit*>(term->var)) {
        m_output << std::get<NodeTermIntLit*>(term->var)->int_lit.value.value();
        return;
    }

    if (std::holds_alternative<NodeTermStringLit*>(term->var)) {
        m_output << "\"" << std::get<NodeTermStringLit*>(term->var)->string_lit.value.value()
                 << "\"";
        return;
    }

    if (std::holds_alternative<NodeTermIdent*>(term->var)) {
        m_output << std::get<NodeTermIdent*>(term->var)->ident.value.value();
        return;
    }

    // --- Operators that wrap one expression ---

    // (expr)
    if (std::holds_alternative<NodeTermParen*>(term->var)) {
        m_output << "(";
        gen_expr(std::get<NodeTermParen*>(term->var)->expr);
        m_output << ")";
        return;
    }

    // !expr
    if (std::holds_alternative<NodeTermNot*>(term->var)) {
        m_output << "!(";
        gen_expr(std::get<NodeTermNot*>(term->var)->expr);
        m_output << ")";
        return;
    }

    // -expr
    if (std::holds_alternative<NodeTermUnaryMinus*>(term->var)) {
        m_output << "-(";
        gen_expr(std::get<NodeTermUnaryMinus*>(term->var)->expr);
        m_output << ")";
        return;
    }

    // --- ++ and -- ---
    //
    // The parenthesising differs on purpose. Postfix is already a complete
    // expression, so `x++;` comes out clean; prefix wraps its operand so that
    // something like `-++x` still groups correctly.

    // ++x
    if (std::holds_alternative<NodeTermPreInc*>(term->var)) {
        m_output << "++(";
        gen_expr(std::get<NodeTermPreInc*>(term->var)->expr);
        m_output << ")";
        return;
    }

    // --x
    if (std::holds_alternative<NodeTermPreDec*>(term->var)) {
        m_output << "--(";
        gen_expr(std::get<NodeTermPreDec*>(term->var)->expr);
        m_output << ")";
        return;
    }

    // x++
    if (std::holds_alternative<NodeTermPostInc*>(term->var)) {
        gen_expr(std::get<NodeTermPostInc*>(term->var)->expr);
        m_output << "++";
        return;
    }

    // x--
    if (std::holds_alternative<NodeTermPostDec*>(term->var)) {
        gen_expr(std::get<NodeTermPostDec*>(term->var)->expr);
        m_output << "--";
        return;
    }

    // --- Calls, indexing, literals ---

    // name(arg, arg)   -- also where the `ask` built-in is handled
    if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
        auto fn_call = std::get<NodeTermFnCall*>(term->var);
        if (is_builtin_ask(fn_call->name.value.value())) {
            gen_ask(fn_call);
            return;
        }
        m_output << fn_call->name.value.value() << "(";
        for (size_t i = 0; i < fn_call->args.size(); i++) {
            if (i > 0) {
                m_output << ", ";
            }
            gen_expr(fn_call->args[i]);
        }
        m_output << ")";
        return;
    }

    // [1, 2, 3]  -- emitted as a C++ brace-init list
    if (std::holds_alternative<NodeTermArrayLit*>(term->var)) {
        auto array_lit = std::get<NodeTermArrayLit*>(term->var);
        m_output << "{";
        for (size_t i = 0; i < array_lit->elements.size(); i++) {
            if (i > 0) {
                m_output << ", ";
            }
            gen_expr(array_lit->elements[i]);
        }
        m_output << "}";
        return;
    }

    // arr[i]  -- also used for string indexing, since C++ std::string supports []
    if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
        auto array_index = std::get<NodeTermArrayIndex*>(term->var);
        gen_expr(array_index->ident);
        m_output << "[";
        gen_expr(array_index->index);
        m_output << "]";
        return;
    }

    // Every NodeTerm alternative is handled above. If a new one is added to
    // NodeTerm in ast.hpp and not handled here, generated C++ would silently
    // lose it -- so say so loudly instead.
    std::cerr << "[BUG] gen_term: unhandled expression kind. Add it in gen_expr.hpp."
              << std::endl;
    exit(EXIT_FAILURE);
}

// lhs <op> rhs
void Generator::gen_bin_expr(const NodeBinExpr* bin_expr) {
    gen_expr(bin_expr->lhs);

    if (bin_expr->op == BinOp::Add) {
        m_output << " + ";
    } else if (bin_expr->op == BinOp::Sub) {
        m_output << " - ";
    } else if (bin_expr->op == BinOp::Mul) {
        m_output << " * ";
    } else if (bin_expr->op == BinOp::Div) {
        m_output << " / ";
    } else if (bin_expr->op == BinOp::Mod) {
        m_output << " % ";
    } else if (bin_expr->op == BinOp::Eq) {
        m_output << " == ";
    } else if (bin_expr->op == BinOp::NotEq) {
        m_output << " != ";
    } else if (bin_expr->op == BinOp::Lt) {
        m_output << " < ";
    } else if (bin_expr->op == BinOp::Gt) {
        m_output << " > ";
    } else if (bin_expr->op == BinOp::LtEq) {
        m_output << " <= ";
    } else if (bin_expr->op == BinOp::GtEq) {
        m_output << " >= ";
    } else if (bin_expr->op == BinOp::And) {
        m_output << " && ";
    } else if (bin_expr->op == BinOp::Or) {
        m_output << " || ";
    } else {
        std::cerr << "[BUG] gen_bin_expr: unknown binary operator." << std::endl;
        exit(EXIT_FAILURE);
    }

    gen_expr(bin_expr->rhs);
}

// An expression is either one term, or two terms joined by an operator.
void Generator::gen_expr(const NodeExpr* expr) {
    if (std::holds_alternative<NodeTerm*>(expr->var)) {
        gen_term(std::get<NodeTerm*>(expr->var));
        return;
    }
    if (std::holds_alternative<NodeBinExpr*>(expr->var)) {
        gen_bin_expr(std::get<NodeBinExpr*>(expr->var));
        return;
    }
    std::cerr << "[BUG] gen_expr: unhandled expression kind." << std::endl;
    exit(EXIT_FAILURE);
}