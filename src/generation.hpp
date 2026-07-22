/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * Generates C source code from the AST. The output is compiled with gcc.
 */

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ast.hpp"

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

            // Array literal: [1, 2, 3] -> {1, 2, 3}
            void operator()(const NodeTermArrayLit* term_array_lit) const {
                gen.m_output << "{";
                for (size_t i = 0; i < term_array_lit->elements.size(); i++) {
                    if (i > 0)
                        gen.m_output << ", ";
                    gen.gen_expr(term_array_lit->elements[i]);
                }
                gen.m_output << "}";
            }

            // Array index access: arr[i] -> arr[i]
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

    /**
     * @brief Generates C code for a binary expression.
     * @param bin_expr The AST binary expression node to emit.
     *
     * Emits the left-hand side, the operator, and the right-hand side.
     * Supports: +, -, *, /, ==, !=, <, >, <=, >=, and, or.
     */
    void gen_bin_expr(const NodeBinExpr* bin_expr) {
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
    void gen_scope(const NodeScope* scope, bool inline_brace = false) {
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

    /**
     * @brief Generates C code for a single statement.
     * @param stmt The AST statement node to emit.
     *
     * Handles: gives (return), my (let), assignment, scopes, if/elif/else,
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
                std::string arr_suffix;
                if (stmt_let->type_annotation.has_value()) {
                    c_type = gen.resolve_type(stmt_let->type_annotation.value());
                    // Handle array type suffix: number[] or number[5]
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

            // Array assignment: arr[i] = val; -> arr[i] = val;
            void operator()(const NodeStmtArrayAssign* stmt_arr_assign) const {
                gen.m_output << "    " << stmt_arr_assign->ident.value.value() << "[";
                gen.gen_expr(stmt_arr_assign->index);
                gen.m_output << "] = ";
                gen.gen_expr(stmt_arr_assign->expr);
                gen.m_output << ";\n";
            }
        };

        StmtVisitor visitor{.gen = *this};
        std::visit(visitor, stmt->var);
    }

    /**
     * @brief Generates a complete C function definition.
     * @param fn The AST function node to emit.
     * @param out The output stream to write the function definition to.
     *
     * Emits a C function with resolved return type, typed parameters,
     * and the function body. Params are registered in gen_prog() for
     * return-type inference; here they are pushed again for scope tracking
     * within the body and popped on exit.
     */
    void gen_fn_def(const NodeStmtFn* fn, std::stringstream& out) {
        // Params are already registered in gen_prog() for return-type inference.
        std::string ret_type = fn->return_type.has_value()
            ? resolve_type(fn->return_type.value())
            : "long";
        out << ret_type << " " << fn->name.value.value() << "(";
        for (size_t i = 0; i < fn->params.size(); i++) {
            if (i > 0)
                out << ", ";
            std::string param_type = fn->params[i].type_annotation.has_value()
                ? resolve_type(fn->params[i].type_annotation.value())
                : "long";
            if (fn->params[i].isArray) {
                out << param_type << "* " << fn->params[i].name.value.value();
            } else {
                out << param_type << " " << fn->params[i].name.value.value();
            }
        }

        out << ")\n";
        std::string saved = m_output.str();
        m_output.str("");
        m_output.clear();
        // register params in scope tracking so they're available in the body
        for (const auto& param : fn->params) {
            std::string pname = param.name.value.value();
            std::string ptype = param.type_annotation.has_value()
                ? resolve_type(param.type_annotation.value())
                : "long";
            if (param.isArray) {
                ptype += "*";
                m_array_params.insert(pname);
            }
            m_declared.push_back(pname);
            m_var_types[pname] = ptype;
        }
        gen_scope(fn->body);
        // clean up params from type tracking
        for (size_t i = 0; i < fn->params.size(); i++) {
            m_var_types.erase(fn->params[i].name.value.value());
            m_declared.pop_back();
        }
        out << m_output.str();
        m_output.str(saved);
        m_output.clear();
        m_output << saved;
        out << "\n";
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
                // Register params in temp context so return-type inference is accurate.
                for (const auto& param : fn->params) {
                    std::string pname = param.name.value.value();
                    std::string ptype = param.type_annotation.has_value()
                        ? resolve_type(param.type_annotation.value())
                        : "long";
                    if (param.isArray) {
                        ptype += "*";
                        m_array_params.insert(pname);
                    }
                    m_declared.push_back(pname);
                    m_var_types[pname] = ptype;
                }
                std::string ret_type = fn->return_type.has_value()
                    ? resolve_type(fn->return_type.value())
                    : "long";
                m_fn_return_types[fn->name.value.value()] = ret_type;
                for (const auto& param : fn->params) {
                    m_var_types.erase(param.name.value.value());
                    if (param.isArray) {
                        m_array_params.erase(param.name.value.value());
                    }
                    m_declared.pop_back();
                }
                decls << ret_type << " " << fn->name.value.value() << "(";
                for (size_t i = 0; i < fn->params.size(); i++) {
                    if (i > 0)
                        decls << ", ";
                    std::string param_type = fn->params[i].type_annotation.has_value()
                        ? resolve_type(fn->params[i].type_annotation.value())
                        : "long";
                    if (fn->params[i].isArray) {
                        decls << param_type << "* " << fn->params[i].name.value.value();
                    } else {
                        decls << param_type << " " << fn->params[i].name.value.value();
                    }
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

    std::string resolve_type(const std::string& type) const {
        if (type == "number") {
            return "long";
        }
        if (type == "word") {
            return "const char*";
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

    /**
     * @brief Infers the C type of an expression based on its AST structure.
     * @param expr The AST expression node to analyze.
     * @return A string representing the inferred C type (e.g., "long", "const char*").
     *
     * If the expression is a literal, returns the corresponding C type.
     * If it's an identifier, looks up its declared type in m_var_types.
     * If it's a function call, looks up the return type in m_fn_return_types.
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

    std::string infer_return_type(const NodeScope* body) const {
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
        }
        return "void";
    }

    std::string infer_return_type_pred(const NodeIfPred* pred) const {
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
                return true;
        }
        if (std::holds_alternative<NodeTermFnCall*>(term->var)) {
            auto fn_call = std::get<NodeTermFnCall*>(term->var);
            auto it = m_fn_return_types.find(fn_call->name.value.value());
            if (it != m_fn_return_types.end() && it->second == "const char*")
                return true;
        }
        // Array index: arr[i] where arr is const char*[] or const char**
        if (std::holds_alternative<NodeTermArrayIndex*>(term->var)) {
            auto arr_idx = std::get<NodeTermArrayIndex*>(term->var);
            if (std::holds_alternative<NodeTerm*>(arr_idx->ident->var)) {
                auto ident_term = std::get<NodeTerm*>(arr_idx->ident->var);
                if (std::holds_alternative<NodeTermIdent*>(ident_term->var)) {
                    auto ident = std::get<NodeTermIdent*>(ident_term->var);
                    auto it = m_var_types.find(ident->ident.value.value());
                    if (it != m_var_types.end() && it->second.find("const char*") != std::string::npos)
                        return true;
                }
            }
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
    std::unordered_set<std::string>
        m_array_params;  ///< Names of function parameters passed as arrays
    std::unordered_map<std::string, std::string>
        m_fn_return_types;  ///< Map of function names to their return types
};
