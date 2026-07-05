/**
 * @file generation.hpp
 * @brief Code generator for the Precious programming language.
 *
 * This file implements the code generation phase of the compiler. It traverses
 * the AST (Abstract Syntax Tree) produced by the parser and emits x86-64 assembly
 * code targeting Linux (NASM syntax, ELF64 format).
 *
 * The generated assembly uses a stack-based execution model:
 * - Expressions push their results onto the stack
 * - Binary operations pop two operands, compute, and push the result
 * - Statements manage their own stack frame via begin/end scope
 *
 * The output is a complete .asm file that can be assembled with NASM and linked with ld.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <variant>

#include "parser.hpp"

/**
 * @class Generator
 * @brief Generates x86-64 assembly from an AST.
 *
 * The generator maintains:
 * - A stack of variables with their stack offsets
 * - A stack size counter for tracking stack pointer position
 * - A scope stack for variable lifetime management
 * - A label counter for generating unique branch labels
 *
 * Usage:
 * @code
 *   Generator generator(prog);
 *   std::string assembly = generator.gen_prog();
 * @endcode
 */
class Generator {
public:
    /**
     * @brief Constructs a generator for the given program AST.
     * @param prog The parsed program to generate code for.
     */
    inline explicit Generator(NodeProg prog) : m_prog(std::move(prog)) {}

    /**
     * @brief Generates assembly code for a single term.
     * @param term The AST term node to generate code for.
     *
     * Handles three term types:
     * - Integer literals: loads immediate value into RAX and pushes
     * - Parenthesized expressions: delegates to expression generation
     * - Identifiers: looks up variable on stack and pushes its value
     */
    void gen_term(const NodeTerm* term) {
        struct TermVisitor {
            Generator& gen;

            void operator()(const NodeTermIntLit* term_int_lit) const {
                gen.m_output << "    ;; int lit: " << term_int_lit->int_lit.value.value() << "\n";
                gen.m_output << "    mov rax, " << term_int_lit->int_lit.value.value() << "\n";
                gen.push("rax");
            }

            void operator()(const NodeTermParen* term_paren) const {
                gen.gen_expr(term_paren->expr);
            }

            void operator()(const NodeTermIdent* term_ident) const {
                const auto it = std::ranges::find_if(
                    std::as_const(gen.m_vars),
                    [&](const Var& var) { return var.name == term_ident->ident.value.value(); });
                if (it == gen.m_vars.cend()) {
                    std::cerr << "[ERROR] Nasty little identifier! '"
                              << term_ident->ident.value.value()
                              << "' is not declared, no it isn't, precious! (line "
                              << term_ident->ident.line << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen.m_output << "    ;; variable: " << term_ident->ident.value.value() << "\n";
                std::stringstream offset;
                offset << "QWORD [rsp + " << (gen.m_stack_size - it->stack_loc - 1) * 8 << "]";
                gen.push(offset.str());
            }
            void operator()(const NodeTermNot* term_not) const {
                gen.m_output << "    ;; not expr\n";
                gen.gen_expr(
                    term_not->expr);  // evaluate the inner expression, result pushed onto stack
                gen.pop("rax");       // pop the result into rax
                gen.m_output
                    << "    test rax, rax\n";  // test if rax is zero or non-zero (sets zero flag)
                gen.m_output << "    setz al\n";  // set al to 1 if zero flag is set (rax was 0), 0
                                                  // otherwise — this flips the boolean
                gen.m_output << "    movzx rax, al\n";  // zero extend al to rax so we can push a
                                                        // full 64-bit value
                gen.push("rax");
            }
        };
        TermVisitor visitor{.gen = *this};
        std::visit(visitor, term->var);
    }

    /**
     * @brief Generates assembly code for a binary expression.
     * @param bin_expr The AST binary expression node.
     *
     * Binary operations follow the pattern:
     * 1. Generate code for RHS (pushes result)
     * 2. Generate code for LHS (pushes result)
     * 3. Pop both operands into RAX and RBX
     * 4. Perform the operation
     * 5. Push the result
     *
     * This order ensures correct evaluation for non-commutative operations.
     */
    void gen_bin_expr(const NodeBinExpr* bin_expr) {
        struct BinExprVisitor {
            Generator& gen;

            void operator()(const NodeBinExprSub* sub) const {
                gen.m_output << "    ;; sub expr\n";
                gen.gen_expr(sub->rhs);
                gen.gen_expr(sub->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    sub rax, rbx\n";
                gen.push("rax");
            }

            void operator()(const NodeBinExprAdd* add) const {
                gen.m_output << "    ;; add expr\n";
                gen.gen_expr(add->rhs);
                gen.gen_expr(add->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    add rax, rbx\n";
                gen.push("rax");
            }

            void operator()(const NodeBinExprMulti* multi) const {
                gen.m_output << "    ;; mul expr\n";
                gen.gen_expr(multi->rhs);
                gen.gen_expr(multi->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    mul rbx\n";
                gen.push("rax");
            }

            void operator()(const NodeBinExprDiv* div) const {
                gen.m_output << "    ;; div expr\n";
                gen.gen_expr(div->rhs);
                gen.gen_expr(div->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    div rbx\n";
                gen.push("rax");
            }
            // on my own now
            void operator()(const NodeBinExprEq* eq) const {
                gen.m_output << "    ;; eq expr\n";
                gen.gen_expr(eq->rhs);
                gen.gen_expr(eq->lhs);
                gen.pop("rax");  // rax is used to store the result of the comparison
                gen.pop("rbx");  // rbx is used to store the left-hand side value
                gen.m_output << "    cmp rax, rbx\n";  // compare the two
                gen.m_output << "    sete al\n";  // set al to 1 if equal , al is the lower 8 bits
                                                  // of rax, in simple terms a boolean value
                gen.m_output << "    movzx rax, al\n";  // zero-extend al to rax , in simple terms
                                                        // make it a full 64 bit value
                gen.push("rax");
                // we made it full 64 bit cuz that's what the rest of the code expects
            }
            void operator()(const NodeBinExprNotEq* noteq) const {
                gen.m_output << "    ;; not eq expr\n";
                gen.gen_expr(noteq->rhs);
                gen.gen_expr(noteq->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    cmp rax, rbx\n";
                gen.m_output
                    << "    setne al\n";  // set al to 1 if not equal , 1 means true , 0 means false
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprLtEq* lteq) const {
                gen.m_output << "    ;; less than or equal expr\n";
                gen.gen_expr(lteq->rhs);
                gen.gen_expr(lteq->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    cmp rax, rbx\n";
                gen.m_output << "    setle al\n";  // set al to 1 if less than or equal , 1 means
                                                   // true , 0 means false
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprGtEq* gteq) const {
                gen.m_output << "    ;; greater than or equal expr\n";
                gen.gen_expr(gteq->rhs);
                gen.gen_expr(gteq->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    cmp rax, rbx\n";
                gen.m_output << "    setge al\n";  // set al to 1 if greater than or equal , 1 means
                                                   // true , 0 means false
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprLt* lt) const {
                gen.m_output << "    ;; less than expr\n";
                gen.gen_expr(lt->rhs);
                gen.gen_expr(lt->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    cmp rax, rbx\n";
                gen.m_output
                    << "    setl al\n";  // set al to 1 if less than , 1 means true , 0 means false
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            void operator()(const NodeBinExprGt* gt) const {
                gen.m_output << "    ;; greater than expr\n";
                gen.gen_expr(gt->rhs);
                gen.gen_expr(gt->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    cmp rax, rbx\n";
                gen.m_output << "    setg al\n";  // set al to 1 if greater than , 1 means true , 0
                                                  // means false
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            // visit error gone , this means we are done with the binary expression generation
            // i think that shoudl be enough to add comparsion , will test and do compiler error
            // based development now
            void operator()(const NodeBinExprAnd* and_) const {
                gen.m_output << "  ;; and expr\n";
                gen.gen_expr(and_->rhs);  // this is used to evaluate the right hand side of the
                                          // expression first , this is because we want to short
                                          // circuit the evaluation if the left hand side is false.
                                          // the function gen_expr will push the result of the
                                          // expression onto the stack , so we can pop it later
                gen.gen_expr(and_->lhs);
                gen.pop(
                    "rax");  // rax is used to store the result of the right hand side expression
                gen.pop("rbx");  // rbx is used to store the result of the left hand side expression
                gen.m_output
                    << "    test rax, rax\n";  // test if the right hand side is true or false
                gen.m_output
                    << "    setnz al\n";  // set al to 1 if the right hand side is true , 0 if
                                          // false. al is the lower 8 bits of rax , so we can use it
                                          // to store the result of the and operation
                gen.m_output
                    << "    test rbx, rbx\n";  // test if the left hand side is true or false
                gen.m_output
                    << "    setnz bl\n";  // set bl to 1 if the left hand side is true , 0 if false.
                                          // bl is the lower 8 bits of rbx , so we can use it to
                                          // store the result of the and operation
                gen.m_output
                    << "    and al, bl\n";  // perform the and operation on the two results , if
                                            // both are true , al will be 1 , otherwise 0
                gen.m_output << "    movzx rax, al\n";  // zero extend al to rax , so we can push it
                                                        // onto the stack
                gen.push("rax");
            }
            void operator()(const NodeBinExprOr* or_) const {
                gen.m_output << "  ;; or expr\n";
                gen.gen_expr(or_->rhs);
                gen.gen_expr(or_->lhs);
                gen.pop("rax");
                gen.pop("rbx");
                gen.m_output << "    test rax, rax\n";
                gen.m_output << "    setnz al\n";
                gen.m_output << "    test rbx, rbx\n";
                gen.m_output << "    setnz bl\n";
                gen.m_output
                    << "    or al, bl\n";  // perform the or operation on the two results , if
                                           // either is true , al will be 1 , otherwise 0
                gen.m_output << "    movzx rax, al\n";
                gen.push("rax");
            }
            void operator()(const NodeTermParen* term_paren) const {
                gen.gen_expr(term_paren->expr);
            }
        };
        BinExprVisitor visitor{.gen = *this};
        std::visit(visitor, bin_expr->var);
    }

    /**
     * @brief Generates assembly code for an expression.
     * @param expr The AST expression node.
     *
     * Dispatches to gen_term() for single terms or gen_bin_expr() for binary operations.
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
     * @brief Generates assembly code for a scoped block of statements.
     * @param scope The AST scope node.
     *
     * Creates a new scope (recording current variable count), generates all
     * statements, then ends the scope (removing variables declared within).
     */
    void gen_scope(const NodeScope* scope) {
        begin_scope();
        for (const NodeStmt* stmt : scope->stmts) {
            gen_stmt(stmt);
        }
        end_scope();
    }

    /**
     * @brief Generates assembly code for an elif/else predicate.
     * @param pred The if predicate node (elif or else).
     * @param end_label The label to jump to when skipping this branch.
     *
     * For elif: evaluates condition, jumps to next branch if false, otherwise
     * executes scope and jumps to end. Recursively handles chained elif/else.
     * For else: unconditionally executes its scope.
     */
    void gen_if_pred(const NodeIfPred* pred, const std::string& end_label) {
        struct PredVisitor {
            Generator& gen;
            const std::string& end_label;
            void operator()(const NodeIfPredElif* elif) const {
                gen.m_output << "    ;; elif\n";
                gen.gen_expr(elif->expr);
                gen.pop("rax");
                const std::string label = gen.create_label();
                gen.m_output << "    test rax, rax\n";
                gen.m_output << "    jz " << label << "\n";
                gen.gen_scope(elif->scope);

                gen.m_output << "    jmp " << end_label << "\n";
                gen.m_output << label << ":\n";
                if (elif->pred.has_value()) {
                    gen.gen_if_pred(elif->pred.value(), end_label);
                }
            }
            void operator()(const NodeIfPredElse* else_) const {
                gen.m_output << "    ;; else\n";
                gen.gen_scope(else_->scope);
            }
        };

        PredVisitor visitor{.gen = *this, .end_label = end_label};
        std::visit(visitor, pred->var);
    }

    /**
     * @brief Generates assembly code for a statement.
     * @param stmt The AST statement node.
     *
     * Dispatches to the appropriate code generation method based on statement type:
     * - Exit: evaluates expression, sets up syscall, exits
     * - Let: declares variable, evaluates initializer
     * - Assign: evaluates expression, stores result in existing variable
     * - Scope: delegates to gen_scope()
     * - If: generates condition check and branching logic
     */
    void gen_stmt(const NodeStmt* stmt) {
        struct StmtVisitor {
            Generator& gen;

            void operator()(const NodeStmtExit* stmt_exit) const {
                gen.m_output << "    ;; gives\n";
                gen.gen_expr(stmt_exit->expr);
                gen.m_output << "    mov rax, 60\n";
                gen.pop("rdi");
                gen.m_output << "    syscall\n";
                gen.m_output << "    ;; /gives\n";
            }

            void operator()(const NodeStmtLet* stmt_let) const {
                if (std::ranges::find_if(std::as_const(gen.m_vars), [&](const Var& var) {
                        return var.name == stmt_let->ident.value.value();
                    }) != gen.m_vars.cend()) {
                    std::cerr << "[ERROR] We already has it! '" << stmt_let->ident.value.value()
                              << "' is already declared, precious! (line " << stmt_let->ident.line
                              << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen.m_output << "    ;; we_haves " << stmt_let->ident.value.value() << "\n";
                gen.m_vars.push_back(
                    Var{.name = stmt_let->ident.value.value(), .stack_loc = gen.m_stack_size});
                gen.gen_expr(stmt_let->expr);
                gen.m_output << "    ;; /we_haves\n";
            }

            void operator()(const NodeStmtAssign* stmt_assign) const {
                const auto it = std::ranges::find_if(gen.m_vars, [&](const Var& var) {
                    return (var.name == stmt_assign->ident.value.value());
                });
                if (it == gen.m_vars.end()) {
                    std::cerr << "[ERROR] Trickses! '" << stmt_assign->ident.value.value()
                              << "' is not declared, we cannot assigns to it, no no! (line "
                              << stmt_assign->ident.line << ")" << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen.m_output << "    ;; " << stmt_assign->ident.value.value() << " =\n";
                gen.gen_expr(stmt_assign->expr);
                gen.pop("rax");
                gen.m_output << "    mov QWORD [rsp + "
                             << (gen.m_stack_size - it->stack_loc - 1) * 8 << "], rax\n";
            }

            void operator()(const NodeScope* scope) const {
                gen.m_output << "    ;; scope\n";
                gen.gen_scope(scope);
                gen.m_output << "    ;; /scope\n";
            }

            void operator()(const NodeStmtIf* stmt_if) const {
                gen.m_output << "    ;; if\n";
                gen.gen_expr(stmt_if->expr);
                gen.pop("rax");
                const std::string label = gen.create_label();
                gen.m_output << "    test rax, rax\n";
                gen.m_output << "    jz " << label << "\n";
                gen.gen_scope(stmt_if->scope);
                if (stmt_if->pred.has_value()) {
                    const std::string end_label = gen.create_label();
                    gen.m_output << "    jmp " << end_label << "\n";
                    gen.m_output << label << ":\n";
                    gen.gen_if_pred(stmt_if->pred.value(), end_label);
                    gen.m_output << end_label << ":\n";
                } else {
                    gen.m_output << label << ":\n";
                }
                gen.m_output << "    ;; /if\n";
            }

            void operator()(const NodeStmtWhile* stmt_while) const {
                gen.m_output << "    ;; while\n";

                // Create labels for loop start and end
                const std::string loop_start = gen.create_label();
                const std::string loop_end = gen.create_label();

                // Loop start label
                gen.m_output << loop_start << ":\n";

                // Evaluate condition
                gen.gen_expr(stmt_while->expr);
                gen.pop("rax");
                gen.m_output << "    test rax, rax\n";
                gen.m_output << "    jz " << loop_end << "\n";  // exit loop if condition is 0

                // Loop body
                gen.gen_scope(stmt_while->scope);

                // Jump back to start
                gen.m_output << "    jmp " << loop_start << "\n";

                // Loop end label
                gen.m_output << loop_end << ":\n";
                gen.m_output << "    ;; /while\n";
            }

            void operator()(const NodeStmtPrint* stmt_print)
            const {
                gen.m_output << "    ;; say(expr)\n";
                gen.gen_expr(stmt_print->expr);
                gen.pop("rax"); // rax = integer to print
                gen.m_output << "    ;; convert rax to string at print_buf\n";
                gen.m_output << "    mov rdi, print_buf ;; buffer pointer\n "; // destination buffer
                gen.m_output << "    mov rcx, 0  ;; buffer index\n";
                gen.m_output << "    mov rbx, 10 ;; divisor for decimal conversion\n";

                gen.m_output << ";; handle zero\n";
                gen.m_output << "    test rax, rax\n";// check if rax is zero , will indicate if we need to print '0' or not
                gen.m_output << " jnz .not_zero\n";
            }
        };

        StmtVisitor visitor{.gen = *this};
        std::visit(visitor, stmt->var);
    }

    /**
     * @brief Generates the complete assembly program.
     * @return A string containing the full x86-64 assembly source.
     *
     * Outputs:
     * - global _start entry point
     * - All statement code
     * - Exit syscall (exit code 0) at program end
     */
    [[nodiscard]] std::string gen_prog() {
        m_output << "section .bss\n";
        m_output << "    print_buf: resb 32\n\n";
        m_output << "    ;; 32 byte buffer for integer to string conversion";
        // i know this is not the best way to do this and there is a big code for some mov little endian and all that but for now this should do the work
        // TODO
        // make it the better version
        m_output << "global _start\n_start:\n\n";
        m_output << "    ;; program start\n\n";


        for (const NodeStmt* stmt : m_prog.stmts) {
            gen_stmt(stmt);
        }

        m_output << "\n    ;; program end\n";
        m_output << "    mov rax, 60\n";
        m_output << "    mov rdi, 0\n";
        m_output << "    syscall\n\n";
        return m_output.str();
    }

private:
    /**
     * @brief Pushes a register or value onto the stack.
     * @param reg The register name or memory operand to push.
     */
    void push(const std::string& reg) {
        m_output << "    push " << reg << "\n";
        m_stack_size++;
    }

    /**
     * @brief Pops the top of the stack into a register.
     * @param reg The destination register.
     */
    void pop(const std::string& reg) {
        m_output << "    pop " << reg << "\n";
        m_stack_size--;
    }

    /**
     * @brief Begins a new scope by recording the current variable count.
     *
     * Used for variable lifetime management. Variables declared after this
     * call will be removed when end_scope() is called.
     */
    void begin_scope() { m_scopes.push_back(m_vars.size()); }

    /**
     * @brief Ends the current scope, removing variables declared within it.
     *
     * Calculates how many variables were added since begin_scope(),
     * adjusts the stack pointer to deallocate them, and removes them from
     * the variable table.
     */
    void end_scope() {
        const size_t pop_count = m_vars.size() - m_scopes.back();
        if (pop_count != 0) {
            m_output << "    add rsp, " << pop_count * 8 << "\n";
        }
        m_stack_size -= pop_count;
        for (size_t i = 0; i < pop_count; i++) {
            m_vars.pop_back();
        }
        m_scopes.pop_back();
    }

    /**
     * @brief Creates a unique label for branch targets.
     * @return A unique label string (e.g., "label0", "label1", ...).
     *
     * Labels are used for conditional jumps in if/elif/else statements.
     */
    std::string create_label() {
        std::stringstream ss;
        ss << "label" << m_label_count++;
        return ss.str();
    }

    /**
     * @struct Var
     * @brief Represents a variable in the current scope.
     */
    struct Var {
        std::string name;  ///< The variable's identifier name.
        size_t stack_loc;  ///< The variable's position on the stack (offset from stack base).
    };

    const NodeProg m_prog;           ///< The AST program to generate code for.
    std::stringstream m_output;      ///< String stream for accumulating generated assembly.
    size_t m_stack_size = 0;         ///< Current number of items on the stack.
    std::vector<Var> m_vars{};       ///< Active variables in current scope.
    std::vector<size_t> m_scopes{};  ///< Stack of scope boundaries (variable counts).
    int m_label_count = 0;           ///< Counter for generating unique labels.
};