#ifndef TOKENIZATION_HPP
#define TOKENIZATION_HPP
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#pragma once

enum class TokenType { exit, int_lit, semi, open_paren,close_paren,ident ,let,eq,plus,star,minus,fslash,open_curly,close_curly,if_};

bool is_bin_op(TokenType type){
    switch(type){
        case TokenType::plus:
        case TokenType::star:
            return true;
        default:
            return false;
    }
}

std::optional<int> bin_prec(TokenType type){
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
    std::optional<std::string> value{};
};

class Tokenizer {
public:
    inline Tokenizer(const std::string src) : m_src(std::move(src)) {}

    inline std::vector<Token> tokenize() {
        std::string buf;
        std::vector<Token> tokens;
        while (peek().has_value()) {
            if (std::isalpha(peek().value()) || peek().value() == '_') {
                buf.push_back(consume());
                while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_')) {
                    buf.push_back(consume());
                }
                if (buf == "gives") {
                    tokens.push_back({.type = TokenType::exit});
                    buf.clear();
                    continue;
                } else if (buf == "we_haves") {
                    tokens.push_back({.type = TokenType::let});
                    buf.clear();
                    continue;
                } 
                else if(buf=="if"){
                    tokens.push_back({.type = TokenType::if_});
                    buf.clear();
                    continue;
                }
                else {
                    tokens.push_back({.type=TokenType::ident,.value=buf});
                    buf.clear();
                }
            } else if (std::isdigit(peek().value())) {
                buf.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buf.push_back(consume());
                }
                tokens.push_back({.type = TokenType::int_lit, .value = buf});
                buf.clear();
                continue;
            }
            else if (peek().value()=='('){
                consume();
                tokens.push_back({.type=TokenType::open_paren});
                continue;
            }else if (peek().value()==')'){
                consume();
                tokens.push_back({.type=TokenType::close_paren});
                continue;
            }
            else if (peek().value() == ';') {
                consume();
                tokens.push_back({.type = TokenType::semi});
                continue;
            }
            else if (peek().value() == '=') {
                consume();
                tokens.push_back({.type = TokenType::eq});
                continue;
            }else if(peek().value()=='+'){
                consume();
                tokens.push_back({.type = TokenType::plus});
                continue;
            }
            else if (peek().value()=='*'){
                consume();
                tokens.push_back({.type=TokenType::star});
                continue;
            }else if(peek().value()=='-'){
                consume();
                tokens.push_back({.type=TokenType::minus});
                continue;
            }else if (peek().value()=='/'){
                consume();
                tokens.push_back({.type=TokenType::fslash});
                continue;
            } else if (peek().value()=='{'){
                consume();
                tokens.push_back({.type=TokenType::open_curly});
                continue;
            }else if (peek().value()=='}'){
                consume();
                tokens.push_back({.type=TokenType::close_curly});
                continue;
            }
            else if (std::isspace(peek().value())) {
                consume();
                continue;
            } else {
                std::cerr << "nasty human" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        m_index = 0;
        return tokens;
    }

private:
    [[nodiscard]] inline std::optional<char> peek(int offset = 0) const {
        if (m_index + offset >= m_src.length()) {
            return {};
        } else {
            return m_src.at(m_index+offset);
        }
    }

    inline char consume() { return m_src.at(m_index++); }

    const std::string m_src;
    size_t m_index = 0;
};

#endif  // TOKENIZATION_HPP
