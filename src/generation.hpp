/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * Generates C source code from the AST. The output is compiled with gcc.
 */

#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "parser.hpp"

class Generator {
public:
    /**
     * @brief Constructs a code generator for the given AST program.
     * @param prog The parsed program to generate C code from.
     */
    inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {}

    /**
     * @brief Generates C code for a single term (literal, identifier, or paren expression).
     * @param term The AST term node to emit.
     *
     * Handles: integer literals, identifiers, parenthesized expressions, logical NOT,
     * and function calls (with arguments).
     */
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
        };
        TermVisitor visitor{.gen = *this};
        std::visit(visitor, term->var);
    }

    /**
     * @brief Generates C code for a binary expression.
     * @param bin_expr The AST binary expression node to emit.
     *
     * Emits the left-hand side, the operator, and the right-hand side.
     * Supports: +, -, *, /, ==, !=, <, >, <=, >=, and, or.
     */
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

    /**
     * @brief Generates C code for an expression node.
     * @param expr The AST expression node to emit.
     *
     * Dispatches to gen_term() or gen_bin_expr() depending on the variant type.
     */
    void gen_expr(const NodeExpr* expr) {
        struct ExprVisitor {
            Generator& gen;

            void operator()(const NodeTerm* term) const { gen.gen_term(term); }
            void operator()(const NodeBinExpr* bin_expr) const { gen.gen_bin_expr(bin_expr); }
        };

        ExprVisitor visitor{.gen = *this};
        std::visit(visitor, expr->var);
    }

    /**
     * @brief Generates C code for a scoped block of statements.
     * @param scope The AST scope node to emit.
     *
     * Emits opening/closing braces and tracks declared variables for
     * proper scope cleanup (variable declarations are popped on scope exit).
     */
    void gen_scope(const NodeScope* scope) {
        m_declared_scopes.push_back(m_declared.size());
        m_output << "{\n";
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

    /**
     * @brief Generates C code for an if/elif/else predicate chain.
     * @param pred The AST predicate node (elif or else branch).
     * @param end_label Unused label kept for interface consistency.
     *
     * Recursively processes elif branches and the optional else branch.
     */
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

    /**
     * @brief Generates C code for a single statement.
     * @param stmt The AST statement node to emit.
     *
     * Handles: gives (return), we_haves (let), assignment, scopes, if/elif/else,
     * while loops, say (printf), function definitions (no-op here), and
     * expression statements (function calls).
     */
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
                if (stmt_let->type_annotation.has_value()) {
                    c_type = gen.resolve_type(stmt_let->type_annotation.value());
                } else {
                    c_type = gen.infer_type(stmt_let->expr);
                }
                gen.m_var_types[name] = c_type;
                gen.m_output << "    " << c_type << " " << name << " = ";
                gen.gen_expr(stmt_let->expr);
                gen.m_output << ";\n";
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
        };

        StmtVisitor visitor{.gen = *this};
        std::visit(visitor, stmt->var);
    }

    /**
     * @brief Generates C code for a function call expression.
     * @param fn_call The AST function call node to emit.
     *
     * Emits the function name followed by comma-separated arguments in parentheses.
     * Note: This method is currently unused; gen_term() handles function calls directly.
     */
    void gen_term_call(const NodeTermFnCall* fn_call) {
        m_output << fn_call->name.value.value() << "(";
        for (size_t i = 0; i < fn_call->args.size(); i++) {
            if (i > 0) {
                m_output << ", ";
            }
            gen_expr(fn_call->args[i]);
        }
        m_output << ")";
    }

    /**
     * @brief Generates a complete C function definition.
     * @param fn The AST function node to emit.
     * @param out The output stream to write the function definition to.
     *
     * Emits a C function with return type void, long-typed parameters,
     * and the function body. Temporarily redirects m_output to capture
     * the scope's generated code, then writes it to the out stream.
     */
    void gen_fn_def(const NodeStmtFn* fn, std::stringstream& out) {
        std::string ret_type = has_return(fn->body) ? "long" : "void";
        out << ret_type << " " << fn->name.value.value() << "(";
        for (size_t i = 0; i < fn->params.size(); i++) {
            if (i > 0)
                out << ", ";
            out << "long " << fn->params[i].name.value.value();
        }

        out << ") {\n";
        std::string saved = m_output.str();
        m_output.str("");
        m_output.clear();
        gen_scope(fn->body);
        out << m_output.str();
        m_output.str(saved);
        m_output.clear();
        m_output << saved;
        out << "}\n\n";
    }

    /**
     * @brief Generates the complete C program from the AST.
     * @return A string containing the full C source code.
     *
     * Three-phase generation:
     * 1. Emit forward declarations for all functions (with param types)
     * 2. Emit everything except functions inside main()
     * 3. Append all function definitions after main()
     *
     * The generated C file includes stdio.h and stdlib.h headers.
     */
    [[nodiscard]] std::string gen_prog() {
        std::stringstream decls;
        std::stringstream fns;

        for (const NodeStmt* stmt : m_prog.stmts) {
            if (std::holds_alternative<NodeStmtFn*>(stmt->var)) {
                auto fn = std::get<NodeStmtFn*>(stmt->var);
                std::string ret_type = has_return(fn->body) ? "long" : "void";
                decls << ret_type << " " << fn->name.value.value() << "(";
                for (size_t i = 0; i < fn->params.size(); i++) {
                    if (i > 0)
                        decls << ", ";
                    decls << "long " << fn->params[i].name.value.value();
                }
                decls << ");\n";
                gen_fn_def(fn, fns);
            }
        }

        for (const NodeStmt* stmt : m_prog.stmts) {
            if (!std::holds_alternative<NodeStmtFn*>(stmt->var)) {
                gen_stmt(stmt);
            }
        }

        std::stringstream out;
        out << "#include <stdio.h>\n";
        out << "#include <stdlib.h>\n\n";
        out << decls.str() << "\n";
        out << "int main() {\n";
        out << m_output.str();
        out << "    return 0;\n";
        out << "}\n\n";
        out << fns.str();
        return out.str();
    }

private:
    /**
     * @brief Resolves a TokenType to its corresponding C type as a string.
     * @param type The TokenType to resolve.
     * @return A string representing the C type (e.g., "long", "const char*").
     *
     * Used for variable declarations and type annotations in the generated code.
     */
    std::string resolve_type(TokenType type) const {
        switch (type) {
            case TokenType::type_number_:
                return "long";
            case TokenType::type_word_:
                return "const char*";
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

    /**
     * @brief Infers the C type of an expression based on its AST structure.
     * @param expr The AST expression node to analyze.
     * @return A string representing the inferred C type (e.g., "long", "const char*").
     *
     * If the expression is a literal, returns the corresponding C type.
     * If it's an identifier, looks up its declared type in m_var_types.
     * Defaults to "long" if the type cannot be determined.
     */
    std::string infer_type(const NodeExpr* expr) const {
        if (!std::holds_alternative<NodeTerm*>(expr->var))
            return "long";
        auto term = std::get<NodeTerm*>(expr->var);
        if (std::holds_alternative<NodeTermStringLit*>(term->var))
            return "const char*";
        if (std::holds_alternative<NodeTermIntLit*>(term->var))
            return "long";
        if (std::holds_alternative<NodeTermIdent*>(term->var)) {
            auto ident = std::get<NodeTermIdent*>(term->var);
            auto it = m_var_types.find(ident->ident.value.value());
            if (it != m_var_types.end())
                return it->second;
        }
        return "long";
    }

    /**
     * @brief Checks if an expression is a string literal.
     * @param expr The expression to check.
     * @return true if the expression is a NodeTerm containing NodeTermStringLit.
     */
    bool is_string_expr(const NodeExpr* expr) const {
        if (!std::holds_alternative<NodeTerm*>(expr->var))
            return false;
        auto term = std::get<NodeTerm*>(expr->var);
        if (std::holds_alternative<NodeTermStringLit*>(term->var))
            return true;
        if (std::holds_alternative<NodeTermIdent*>(term->var)) {
            auto ident = std::get<NodeTermIdent*>(term->var);
            auto it = m_var_types.find(ident->ident.value.value());
            if (it != m_var_types.end() && it->second == "const char*")
                return true;  // returns true if the identifier is declared as a string type
        }
        return false;
    }

    /**
     * @brief Checks if a function body contains a gives (return) statement.
     * @param body The function body scope to search.
     * @return true if a gives/return statement exists anywhere in the body, false otherwise.
     *
     * Recursively searches through nested scopes, if/elif/else branches,
     * and while loop bodies to detect any gives statement.
     */
    bool has_return(const NodeScope* body) const {
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
        }
        return false;
    }

    /**
     * @brief Checks if an if/elif/else predicate chain contains a gives statement.
     * @param pred The predicate chain to search.
     * @return true if a gives/return statement exists in any branch.
     */
    bool has_return_pred(const NodeIfPred* pred) const {
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

    const NodeProg m_prog;       ///< The parsed program AST to generate code from.
    std::stringstream m_output;  ///< Current output buffer for generated C code.
    int m_if_count = 0;          ///< Counter for unique if-statement labels.
    std::vector<std::string>
        m_declared;  ///< Stack of declared variable names (for scope tracking).
    std::vector<size_t>
        m_declared_scopes;  ///< Stack of scope boundaries (indices into m_declared).
    std::unordered_map<std::string, std::string>
        m_var_types;  ///< Map of variable names to their types
};
