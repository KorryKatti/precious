/**
 * @file gen_fn.hpp
 * @brief Function, `ask`, and whole-program code generation.
 */

// Included at the bottom of generation.hpp after the Generator class definition.

// The C++ type of one function parameter, as it appears in a signature.
//   param with type annotation -> long / std::string / double / char
//   param with no annotation    -> long  (Precious's default)
//   array param                 -> std::vector<element>&  (passed by reference)
std::string Generator::param_c_type(const NodeFnParam& param) const {
    std::string type = param.type_annotation.has_value()
        ? resolve_type(param.type_annotation.value())
        : "long";
    if (param.isArray) {
        type = "std::vector<" + type + ">&";
    }
    return type;
}

// The C++ return type of a function.
//   explicit `-> type`  -> whatever that maps to
//   body has a `gives`  -> long
//   otherwise           -> void
std::string Generator::fn_return_c_type(const NodeStmtFn* fn) const {
    if (fn->return_type.has_value()) {
        return resolve_type(fn->return_type.value());
    }
    if (has_return(fn->body)) {
        return "long";
    }
    return "void";
}

// Writes `name(params)` into `out`, with no trailing semicolon or newline, so
// the same code serves both the forward declaration and the definition.
void Generator::write_signature(std::stringstream& out, const NodeStmtFn* fn) const {
    out << fn->name.value.value() << "(";
    for (size_t i = 0; i < fn->params.size(); i++) {
        if (i > 0) {
            out << ", ";
        }
        out << param_c_type(fn->params[i]) << " " << fn->params[i].name.value.value();
    }
    out << ")";
}

void Generator::gen_fn_def(const NodeStmtFn* fn, std::stringstream& out) {
    out << fn_return_c_type(fn) << " ";
    write_signature(out, fn);
    out << "\n";

    // Remember m_output, then start it fresh. The body is generated into
    // m_output like any other block, then copied into `out` and restored, so
    // that whatever the caller had accumulated is left untouched.
    const std::string saved_output = m_output.str();
    m_output.str("");
    m_output.clear();

    // Make the parameters look like ordinary variables, so the body can refer
    // to them by name.
    for (const NodeFnParam& param : fn->params) {
        m_declared.push_back(param.name.value.value());
        m_var_types[param.name.value.value()] = param_c_type(param);
    }

    gen_scope(fn->body);

    // Take the parameters back out again, so they cannot leak into other
    // functions. (They must go in the same order they came in, so the
    // m_declared stack unwinds correctly.)
    for (size_t i = 0; i < fn->params.size(); i++) {
        m_var_types.erase(fn->params[i].name.value.value());
        m_declared.pop_back();
    }

    out << m_output.str();
    out << "\n";

    m_output.str(saved_output);
    m_output.clear();
}

// The `ask` built-in, written out as a C++ lambda that is called immediately.
// Being an expression means it works anywhere a value can, with no need to hoist
// a temporary variable into a statement.
void Generator::gen_ask(const NodeTermFnCall* fn_call) {
    if (fn_call->args.size() > 1) {
        std::cerr << "[ERROR] 'ask' takes at most one prompt, precious! (line "
                  << fn_call->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }

    // char needs a character zero, every other numeric type needs 0.
    const char* zero = m_ask_type == "char" ? "'\\0'" : "0";

    m_output << "([&]() -> " << m_ask_type << " { ";

    // The prompt is optional. Flushing matters: without it the prompt would sit
    // in the output buffer and the user's typing would appear before it.
    if (!fn_call->args.empty()) {
        m_output << "std::cout << ";
        gen_expr(fn_call->args[0]);
        m_output << " << std::flush; ";
    }

    if (m_ask_type == "std::string") {
        // Read the whole line, spaces and all.
        m_output << "std::string _t; std::getline(std::cin, _t); return _t; }())";
        return;
    }

    // Read a single value. Two details worth knowing about std::cin:
    //
    //  1. `>>` leaves the newline in the buffer, which would make a following
    //     getline() read an empty string. So drop the rest of the line.
    //  2. A failed read sets failbit, which would make every later read a
    //     silent no-op. So clear the stream state before moving on. Bad input
    //     then just becomes 0 instead of breaking the rest of the program.
    m_output << m_ask_type << " _t = " << zero << "; if (!(std::cin >> _t)) { _t = " << zero
             << "; } std::cin.clear(); "
                "std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\\n'); "
                "return _t; }())";
}

// Writes `expr` out. A non-empty ask_type tells any `ask` call inside it which
// C++ type to read into; an empty one leaves the current setting alone. The old
// setting is always restored afterwards, so one statement cannot leak its type
// into the next.
void Generator::gen_with_ask_type(const std::string& ask_type, const NodeExpr* expr) {
    const std::string saved = m_ask_type;
    if (!ask_type.empty()) {
        m_ask_type = ask_type;
    }
    gen_expr(expr);
    m_ask_type = saved;
}

// Builds the entire C++ file:
//
//     #include <bits/stdc++.h>
//     <a forward declaration for every fn>   -- so fns can call each other
//                                           -- regardless of the order they
//                                              were written in
//     int main() { ... }                     -- calls the entry function
//     <every fn definition>
std::string Generator::gen_prog() {
    std::stringstream forward_decls;
    std::stringstream definitions;

    // Pass one: figure out every function's return type, and write its forward
    // declaration. Return types have to be known before any body is generated,
    // because `my x = some_fn();` needs to know what type to give x.
    for (const NodeStmt* stmt : m_prog.stmts) {
        if (!std::holds_alternative<NodeStmtFn*>(stmt->var)) {
            continue;
        }
        const NodeStmtFn* fn = std::get<NodeStmtFn*>(stmt->var);
        const std::string name = fn->name.value.value();
        const std::string return_type = fn_return_c_type(fn);
        m_fn_return_types[name] = return_type;

        forward_decls << return_type << " ";
        write_signature(forward_decls, fn);
        forward_decls << ";\n";
    }

    // Pass two: generate each body now that all return types are known.
    for (const NodeStmt* stmt : m_prog.stmts) {
        if (!std::holds_alternative<NodeStmtFn*>(stmt->var)) {
            continue;
        }
        gen_fn_def(std::get<NodeStmtFn*>(stmt->var), definitions);
    }

    // The entry function is an ordinary function. main() just calls it, and a
    // `gives` inside it becomes the process exit code.
    const std::string entry_name = m_prog.entry_fn->name.value.value();
    const std::string entry_return = m_fn_return_types.at(entry_name);

    std::stringstream out;
    out << "#include <bits/stdc++.h>\n\n";
    out << forward_decls.str() << "\n";
    out << "int main() {\n";
    if (entry_return == "void") {
        out << "    " << entry_name << "();\n";
        out << "    return 0;\n";
    } else {
        out << "    return " << entry_name << "();\n";
    }
    out << "}\n\n";
    out << definitions.str();
    return out.str();
}