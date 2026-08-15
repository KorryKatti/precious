/**
 * @file gen_fn.hpp
 * @brief Function and program code generation for the Precious code generator.
 */

// Included at the bottom of generation.hpp after the Generator class definition.

void Generator::gen_fn_def(const NodeStmtFn* fn, std::stringstream& out) {
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

std::string Generator::gen_prog() {
    std::stringstream decls;
    std::stringstream fns;

    for (const NodeStmt* stmt : m_prog.stmts) {
        if (std::holds_alternative<NodeStmtFn*>(stmt->var)) {
            auto fn = std::get<NodeStmtFn*>(stmt->var);
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
