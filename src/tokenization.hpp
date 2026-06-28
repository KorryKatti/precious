#ifndef TOKENIZATION_HPP
#define TOKENIZATION_HPP
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#pragma once

enum class TokenType { exit, int_lit, semi, open_paren,close_paren,ident ,let,eq,plus,star,minus,fslash,open_curly,close_curly,if_,elif,else_};

inline std::string to_string(const TokenType type){
    switch(type){
        case TokenType::exit: return "`gives`";
        case TokenType::int_lit: return "int literal";
        case TokenType::semi: return "`;`";
        case TokenType::open_paren: return "`(`";
        case TokenType::close_paren: return "`)`";
        case TokenType::ident: return "identifier";
        case TokenType::let: return "`we_haves`";
        case TokenType::eq: return "`=`";
        case TokenType::plus: return "`+`";
        case TokenType::star: return "`*`";
        case TokenType::minus: return "`-`";
        case TokenType::fslash: return "`/`";
        case TokenType::open_curly: return "`{`";
        case TokenType::close_curly: return "`}`";
        case TokenType::if_: return "`if`";
        case TokenType::elif: return "`elif`";
        case TokenType::else_: return "`else`";
    }
    assert(false);
}

inline std::optional<int> bin_prec(const TokenType type){
    switch(type){
        case TokenType::plus:
        case TokenType::minus:
        return 0;
        case TokenType::star:
        case TokenType::fslash:
        return 1;
        default:
        return {};
    }
}

struct Token {
    TokenType type;
    int line;
    std::optional<std::string> value{};
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string src) : m_src(std::move(src)) {}

    std::vector<Token> tokenize() {
        std::string buf;
        std::vector<Token> tokens;
        int line_count = 1;
        while (peek().has_value()) {
            if (std::isalpha(peek().value()) || peek().value() == '_') {
                buf.push_back(consume());
                while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_')) {
                    buf.push_back(consume());
                }
                if (buf == "gives") {
                    tokens.push_back({.type = TokenType::exit, .line = line_count});
                    buf.clear();
                    continue;
                } else if (buf == "we_haves") {
                    tokens.push_back({.type = TokenType::let, .line = line_count});
                    buf.clear();
                    continue;
                } 
                else if(buf=="if"){
                    tokens.push_back({.type = TokenType::if_, .line = line_count});
                    buf.clear();
                    continue;
                }
                else if (buf=="elif"){
                    tokens.push_back({.type=TokenType::elif, .line = line_count});
                    buf.clear();
                    continue;
                }
                else if (buf == "else"){
                    tokens.push_back({.type=TokenType::else_, .line = line_count});
                    buf.clear();
                    continue;

                }

                else {
                    tokens.push_back({.type=TokenType::ident, .line = line_count, .value=buf});
                    buf.clear();
                }
            } else if (std::isdigit(peek().value())) {
                buf.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buf.push_back(consume());
                }
                tokens.push_back({.type = TokenType::int_lit, .line = line_count, .value = buf});
                buf.clear();
                continue;
            }
            else if (peek().value()=='/' && peek(1).has_value()&&peek(1).value()=='/'){
                consume();
                consume();
                while (peek().has_value()&&peek().value()!='\n'){
                    consume();
                }
                continue;
            }
            else if (peek().value()=='/' && peek(1).has_value()&&peek(1).value()=='*'){
                consume();
                consume();
                while (peek().has_value()){
                    if (peek().value()=='*' && peek(1).has_value()&&peek(1).value()=='/'){
                        break;
                    }
                    consume();
                }
                if (peek().has_value()){
                    consume();
                }
                if (peek().has_value())consume();


                continue;
            }

            else if (peek().value()=='('){
                consume();
                tokens.push_back({.type=TokenType::open_paren, .line = line_count});
                continue;
            }else if (peek().value()==')'){
                consume();
                tokens.push_back({.type=TokenType::close_paren, .line = line_count});
                continue;
            }
            else if (peek().value() == ';') {
                consume();
                tokens.push_back({.type = TokenType::semi, .line = line_count});
                continue;
            }
            else if (peek().value() == '=') {
                consume();
                tokens.push_back({.type = TokenType::eq, .line = line_count});
                continue;
            }else if(peek().value()=='+'){
                consume();
                tokens.push_back({.type = TokenType::plus, .line = line_count});
                continue;
            }
            else if (peek().value()=='*'){
                consume();
                tokens.push_back({.type=TokenType::star, .line = line_count});
                continue;
            }else if(peek().value()=='-'){
                consume();
                tokens.push_back({.type=TokenType::minus, .line = line_count});
                continue;
            }else if (peek().value()=='/'){
                consume();
                tokens.push_back({.type=TokenType::fslash, .line = line_count});
                continue;
            } else if (peek().value()=='{'){
                consume();
                tokens.push_back({.type=TokenType::open_curly, .line = line_count});
                continue;
            }else if (peek().value()=='}'){
                consume();
                tokens.push_back({.type=TokenType::close_curly, .line = line_count});
                continue;
            }
            else if (peek().value() == '\n') {
                consume();
                line_count++;
                continue;
            }
            else if (std::isspace(peek().value())) {
                consume();
                continue;
            } else {
                std::cerr << "Invalid token" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        m_index = 0;
        return tokens;
    }

private:
    [[nodiscard]] std::optional<char> peek(int offset = 0) const {
        if (m_index + offset >= m_src.length()) {
            return {};
        }
        return m_src.at(m_index+offset);
    }

    char consume() { return m_src.at(m_index++); }

    const std::string m_src;
    size_t m_index = 0;
};

#endif  // TOKENIZATION_HPP

