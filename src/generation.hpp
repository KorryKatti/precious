/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * Generates C source code from the AST. The output is compiled with gcc.
 */

#include <sstream>
#include <vector>
#include <variant>

#include "parser.hpp"

class Generator {
public:
    inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {}

    void gen_term(const NodeTerm* term) {
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
        };
        TermVisitor visitor{.gen = *this};
        std::visit(visitor, term->var);
    }

    void gen_bin_expr(const NodeBinExpr* bin_expr) {
        struct BinExprVisitor {
            Generator& gen;

            void operator()(const NodeBinExprAdd* add) const {
                gen.gen_expr(add->lhs);
                gen.m_output << " + ";
                gen.gen_expr(add->rhs);
            }
            void operator()(const NodeBinExprSub* sub) const {
                gen.gen_expr(sub->lhs);
                gen.m_output << " - ";
                gen.gen_expr(sub->rhs);
            }
            void operator()(const NodeBinExprMulti* multi) const {
                gen.gen_expr(multi->lhs);
                gen.m_output << " * ";
                gen.gen_expr(multi->rhs);
            }
            void operator()(const NodeBinExprDiv* div) const {
                gen.gen_expr(div->lhs);
                gen.m_output << " / ";
                gen.gen_expr(div->rhs);
            }
            void operator()(const NodeBinExprEq* eq) const {
                gen.gen_expr(eq->lhs);
                gen.m_output << " == ";
                gen.gen_expr(eq->rhs);
            }
            void operator()(const NodeBinExprNotEq* noteq) const {
                gen.gen_expr(noteq->lhs);
                gen.m_output << " != ";
                gen.gen_expr(noteq->rhs);
            }
            void operator()(const NodeBinExprLt* lt) const {
                gen.gen_expr(lt->lhs);
                gen.m_output << " < ";
                gen.gen_expr(lt->rhs);
            }
            void operator()(const NodeBinExprGt* gt) const {
                gen.gen_expr(gt->lhs);
                gen.m_output << " > ";
                gen.gen_expr(gt->rhs);
            }
            void operator()(const NodeBinExprLtEq* lteq) const {
                gen.gen_expr(lteq->lhs);
                gen.m_output << " <= ";
                gen.gen_expr(lteq->rhs);
            }
            void operator()(const NodeBinExprGtEq* gteq) const {
                gen.gen_expr(gteq->lhs);
                gen.m_output << " >= ";
                gen.gen_expr(gteq->rhs);
            }
            void operator()(const NodeBinExprAnd* and_) const {
                gen.gen_expr(and_->lhs);
                gen.m_output << " && ";
                gen.gen_expr(and_->rhs);
            }
            void operator()(const NodeBinExprOr* or_) const {
                gen.gen_expr(or_->lhs);
                gen.m_output << " || ";
                gen.gen_expr(or_->rhs);
            }
            void operator()(const NodeTermParen* term_paren) const {
                gen.gen_expr(term_paren->expr);
            }
        };
        BinExprVisitor visitor{.gen = *this};
        std::visit(visitor, bin_expr->var);
    }

    void gen_expr(const NodeExpr* expr) {
        struct ExprVisitor {
            Generator& gen;

            void operator()(const NodeTerm* term) const { gen.gen_term(term); }
            void operator()(const NodeBinExpr* bin_expr) const { gen.gen_bin_expr(bin_expr); }
        };

        ExprVisitor visitor{.gen = *this};
        std::visit(visitor, expr->var);
    }

    void gen_scope(const NodeScope* scope) {
        m_declared_scopes.push_back(m_declared.size());
        m_output << "{\n";
        for (const NodeStmt* stmt : scope->stmts) {
            gen_stmt(stmt);
        }
        m_output << "}\n";
        const size_t pop_count = m_declared.size() - m_declared_scopes.back();
        for (size_t i = 0; i < pop_count; i++) {
            m_declared.pop_back();
        }
        m_declared_scopes.pop_back();
    }

    void gen_if_pred(const NodeIfPred* pred, const std::string& end_label) {
        struct PredVisitor {
            Generator& gen;
            const std::string& end_label;
            void operator()(const NodeIfPredElif* elif) const {
                gen.m_output << " else if (";
                gen.gen_expr(elif->expr);
                gen.m_output << ")\n";
                gen.gen_scope(elif->scope);
                if (elif->pred.has_value()) {
                    gen.gen_if_pred(elif->pred.value(), end_label);
                }
            }
            void operator()(const NodeIfPredElse* else_) const {
                gen.m_output << " else\n";
                gen.gen_scope(else_->scope);
            }
        };

        PredVisitor visitor{.gen = *this, .end_label = end_label};
        std::visit(visitor, pred->var);
    }

    void gen_stmt(const NodeStmt* stmt) {
        struct StmtVisitor {
            Generator& gen;

            void operator()(const NodeStmtExit* stmt_exit) const {
                gen.m_output << "    return ";
                gen.gen_expr(stmt_exit->expr);
                gen.m_output << ";\n";
            }

            void operator()(const NodeStmtLet* stmt_let) const {
                const std::string& name = stmt_let->ident.value.value();
                const size_t scope_start = gen.m_declared_scopes.empty()
                    ? 0 : gen.m_declared_scopes.back();
                for (size_t i = scope_start; i < gen.m_declared.size(); i++) {
                    if (gen.m_declared[i] == name) {
                        std::cerr << "[ERROR] We already has it! '"
                                  << name
                                  << "' is already declared, precious! (line "
                                  << stmt_let->ident.line << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
                gen.m_declared.push_back(name);
                gen.m_output << "    long " << name << " = ";
                gen.gen_expr(stmt_let->expr);
                gen.m_output << ";\n";
            }

            void operator()(const NodeStmtAssign* stmt_assign) const {
                gen.m_output << "    " << stmt_assign->ident.value.value() << " = ";
                gen.gen_expr(stmt_assign->expr);
                gen.m_output << ";\n";
            }

            void operator()(const NodeScope* scope) const {
                gen.gen_scope(scope);
            }

            void operator()(const NodeStmtIf* stmt_if) const {
                gen.m_output << "    if (";
                gen.gen_expr(stmt_if->expr);
                gen.m_output << ")\n";
                gen.gen_scope(stmt_if->scope);
                if (stmt_if->pred.has_value()) {
                    std::string end_label = "end_if_" + std::to_string(gen.m_if_count++);
                    gen.gen_if_pred(stmt_if->pred.value(), end_label);
                }
            }

            void operator()(const NodeStmtWhile* stmt_while) const {
                gen.m_output << "    while (";
                gen.gen_expr(stmt_while->expr);
                gen.m_output << ")\n";
                gen.gen_scope(stmt_while->scope);
            }

            void operator()(const NodeStmtPrint* stmt_print) const {
                gen.m_output << "    printf(\"%ld\\n\", ";
                gen.gen_expr(stmt_print->expr);
                gen.m_output << ");\n";
            }
        };

        StmtVisitor visitor{.gen = *this};
        std::visit(visitor, stmt->var);
    }

    [[nodiscard]] std::string gen_prog() {
        m_output << "#include <stdio.h>\n";
        m_output << "#include <stdlib.h>\n\n";
        m_output << "int main() {\n";

        for (const NodeStmt* stmt : m_prog.stmts) {
            gen_stmt(stmt);
        }

        m_output << "    return 0;\n";
        m_output << "}\n";
        return m_output.str();
    }

private:
    const NodeProg m_prog;
    std::stringstream m_output;
    int m_if_count = 0;
    std::vector<std::string> m_declared;
    std::vector<size_t> m_declared_scopes;
};
