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

// The `ask` built-in.
//
// Being an expression means it works anywhere a value can, with no need to hoist
// a temporary into a statement. The work itself lives in the standard library,
// in out/precious_runtime.h -- all that is left here is picking the reader that
// matches the destination type and writing the call.
void Generator::gen_ask(const NodeTermFnCall* fn_call) {
    if (fn_call->args.size() > 1) {
        std::cerr << "[ERROR] 'ask' takes at most one prompt, precious! (line "
                  << fn_call->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }

    const char* reader = "ask";
    if (m_ask_type == "long") {
        reader = "ask_number";
    } else if (m_ask_type == "double") {
        reader = "ask_decimal";
    } else if (m_ask_type == "char") {
        reader = "ask_letter";
    }

    m_output << "precious::" << reader << "(";
    if (!fn_call->args.empty()) {
        gen_expr(fn_call->args[0]);
    } else {
        m_output << "\"\""; // the prompt is optional
    }
    m_output << ")";
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

// The small built-ins: len(x), str(x), num(x), decimal(x).
//
// Like `ask`, they are not keywords. They are ordinary calls that the generator
// answers for, and a program that defines its own `fn len(...)` gets the
// ordinary function instead (see is_builtin()).
void Generator::gen_builtin_call(const NodeTermFnCall* fn_call) {
    const std::string name = fn_call->name.value.value();

    // http_get and http_code take a url and an optional timeout, so they are
    // checked before the one-value rule the rest of them follow.
    if (is_builtin_networking(name)) {
        if (fn_call->args.empty() || fn_call->args.size() > 2) {
            std::cerr << "[ERROR] '" << name << "' takes a url, and optionally a timeout in "
                      << "seconds, precious! (line " << fn_call->name.line << ")" << std::endl;
            exit(EXIT_FAILURE);
        }
        m_output << "precious::" << name << "(";
        gen_expr(fn_call->args[0]);
        if (fn_call->args.size() == 2) {
            m_output << ", ";
            gen_expr(fn_call->args[1]);
        }
        m_output << ")";
        return;
    }

    if (fn_call->args.size() != 1) {
        std::cerr << "[ERROR] '" << name << "' takes exactly one value, precious! (line "
                  << fn_call->name.line << ")" << std::endl;
        exit(EXIT_FAILURE);
    }

    const NodeExpr* arg = fn_call->args[0];
    const std::string given = infer_type(arg);

    if (name == "len") {
        // An array is a std::vector and a word is a std::string, and both answer
        // to .size() -- but a word that is a literal needs to be a string first.
        if (given.rfind("std::vector", 0) == 0) {
            m_output << "((";
            gen_expr(arg);
            m_output << ").size())";
            return;
        }
        gen_as_string(arg);
        m_output << ".size()";
        return;
    }

    if (name == "str") {
        // A word is already a word, so nothing to do.
        if (given == "std::string") {
            gen_expr(arg);
            return;
        }
        // A stream prints a number the way it is written -- 2.5, not 2.500000.
        m_output << "([&]() -> std::string { std::ostringstream _o; _o << (";
        gen_expr(arg);
        m_output << "); return _o.str(); }())";
        return;
    }

    if (name == "num") {
        if (given == "std::string") {
            // strtol rather than stol: text that is not a number comes back as 0
            // instead of throwing and taking the program down with it.
            m_output << "std::strtol(";
            gen_as_string(arg);
            m_output << ".c_str(), nullptr, 10)";
            return;
        }
        m_output << "((long)";
        gen_expr(arg);
        m_output << ")";
        return;
    }


    // decimal(x)
    if (given == "std::string") {
        m_output << "std::strtod(";
        gen_as_string(arg);
        m_output << ".c_str(), nullptr)";
        return;
    }
    m_output << "((double)";
    gen_expr(arg);
    m_output << ")";
}

// ---------------------------------------------------------------------------
// Native packages
// ---------------------------------------------------------------------------
//
// An `import "networking"` names one of these. This table is the only place in
// the compiler that knows what any package needs: the header the generated C++
// has to include, and the flags g++ has to be given to link it. The type itself
// is declared in generation.hpp, above the class that looks one up.

const std::vector<NativePackage>& native_packages() {
    static const std::vector<NativePackage> table = {
        {"networking", "#include <curl/curl.h>", "PRECIOUS_NETWORKING", "-lcurl"},
    };
    return table;
}

const NativePackage* Generator::find_package(const std::string& name) const {
    for (const NativePackage& package : native_packages()) {
        if (package.name == name) {
            return &package;
        }
    }
    return nullptr;
}

// The g++ flags for everything the program imported, ready to paste onto the
// compiler command. An import links its library whether or not the program ends
// up calling anything: the import line is a promise, not a hint.
std::string Generator::package_flags() const {
    std::string flags;
    for (const Token& imported : m_prog.imports) {
        const NativePackage* package = find_package(imported.value.value());
        if (package == nullptr) {
            continue; // gen_prog() has already said so and stopped
        }
        if (!flags.empty()) {
            flags += " ";
        }
        flags += package->flags;
    }
    return flags;
}

// Builds the entire C++ file:
//
//     #include <bits/stdc++.h>
//     <a forward declaration for every fn>   -- so fns can call each other
//                                           -- regardless of the order they
//                                              were written in
//     long the_ring = 0;                     -- the one hidden piece of state
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

    // A header for every package the program imported. Linking is the other half
    // of the job, and that happens in main.cpp, which asks for package_flags().
    for (const Token& imported : m_prog.imports) {
        const NativePackage* package = find_package(imported.value.value());
        if (package == nullptr) {
            std::cerr << "[ERROR] There is no such package, precious! We have: ";
            bool first = true;
            for (const NativePackage& known : native_packages()) {
                std::cerr << (first ? "" : ", ") << "'" << known.name << "'";
                first = false;
            }
            std::cerr << " (line " << imported.line << ")" << std::endl;
            exit(EXIT_FAILURE);
        }
        out << package->include << "\n";
        out << "#define " << package->define << "\n";
    }
    // The standard library comes last, because an imported package's #define
    // has to be in place before the runtime header decides what to offer.
    out << "#include \"precious_runtime.h\"  // the Precious standard library\n\n";

    out << forward_decls.str() << "\n";
    // The ring: one number that outlives every block and every function. Precious
    // has no globals of its own, so the compiler writes this one and the
    // programmer simply writes `the_ring` from wherever they like.
    out << "long the_ring = 0;\n\n";
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