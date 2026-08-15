/**
 * @file gen_expr.hpp
 * @brief Expression code generation for the Precious code generator.
 */

// Included at the bottom of generation.hpp after the Generator class definition.

void Generator::gen_term(const NodeTerm* term) {
    struct TermVisitor {
        Generator& gen;

        void operator()(const NodeTermIntLit* term_int_lit) const {
            gen.m_output << term_int_lit->int_lit.value.value();
        }

        void operator()(const NodeTermParen* term_paren) const {
            gen.m_output << "(";
            gen.gen_expr(term_paren->expr);
            gen.m_output << ")";
        }

        void operator()(const NodeTermIdent* term_ident) const {
            gen.m_output << term_ident->ident.value.value();
        }

        void operator()(const NodeTermNot* term_not) const {
            gen.m_output << "!(";
            gen.gen_expr(term_not->expr);
            gen.m_output << ")";
        }

        void operator()(const NodeTermFnCall* term_fn_call) const {
            gen.m_output << term_fn_call->name.value.value() << "(";
            for (size_t i = 0; i < term_fn_call->args.size(); i++) {
                if (i > 0)
                    gen.m_output << ", ";
                gen.gen_expr(term_fn_call->args[i]);
            }
            gen.m_output << ")";
        }

        void operator()(const NodeTermStringLit* term_string_lit) const {
            gen.m_output << "\"" << term_string_lit->string_lit.value.value() << "\"";
        }

        void operator()(const NodeTermArrayLit* term_array_lit) const {
            gen.m_output << "{";
            for (size_t i = 0; i < term_array_lit->elements.size(); i++) {
                if (i > 0)
                    gen.m_output << ", ";
                gen.gen_expr(term_array_lit->elements[i]);
            }
            gen.m_output << "}";
        }

        void operator()(const NodeTermArrayIndex* term_array_index) const {
            gen.gen_expr(term_array_index->ident);
            gen.m_output << "[";
            gen.gen_expr(term_array_index->index);
            gen.m_output << "]";
        }

        void operator()(const NodeTermUnaryMinus* term_unary_minus) const {
            gen.m_output << "-(";
            gen.gen_expr(term_unary_minus->expr);
            gen.m_output << ")";
        }
    };
    TermVisitor visitor{.gen = *this};
    std::visit(visitor, term->var);
}

void Generator::gen_bin_expr(const NodeBinExpr* bin_expr) {
    gen_expr(bin_expr->lhs);
    switch (bin_expr->op) {
        case BinOp::Add:   m_output << " + "; break;
        case BinOp::Sub:   m_output << " - "; break;
        case BinOp::Mul:   m_output << " * "; break;
        case BinOp::Div:   m_output << " / "; break;
        case BinOp::Mod:   m_output << " % "; break;
        case BinOp::Eq:    m_output << " == "; break;
        case BinOp::NotEq: m_output << " != "; break;
        case BinOp::Lt:    m_output << " < "; break;
        case BinOp::Gt:    m_output << " > "; break;
        case BinOp::LtEq:  m_output << " <= "; break;
        case BinOp::GtEq:  m_output << " >= "; break;
        case BinOp::And:   m_output << " && "; break;
        case BinOp::Or:    m_output << " || "; break;
    }
    gen_expr(bin_expr->rhs);
}

void Generator::gen_expr(const NodeExpr* expr) {
    struct ExprVisitor {
        Generator& gen;

        void operator()(const NodeTerm* term) const { gen.gen_term(term); }
        void operator()(const NodeBinExpr* bin_expr) const { gen.gen_bin_expr(bin_expr); }
    };

    ExprVisitor visitor{.gen = *this};
    std::visit(visitor, expr->var);
}
