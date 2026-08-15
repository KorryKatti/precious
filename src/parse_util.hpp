/**
 * @file parse_util.hpp
 * @brief Token stream utilities for the Precious parser.
 */

// Included at the bottom of parser.hpp after the Parser class definition.

std::optional<Token> Parser::peek(const int offset) const {
    if (m_index + offset < 0 || m_index + offset >= m_tokens.size()) {
        return {};
    }
    return m_tokens.at(m_index + offset);
}

Token Parser::consume() { return m_tokens.at(m_index++); }

Token Parser::try_consume_err(TokenType type) {
    if (peek().has_value() && peek().value().type == type) {
        return consume();
    }
    error_expected(to_string(type));
    return {};
}

std::optional<Token> Parser::try_consume(TokenType type) {
    if (peek().has_value() && peek().value().type == type) {
        return consume();
    }
    return {};
}

void Parser::error_expected(const std::string& msg) const {
    std::cerr << "[ERROR] Trickses! Trickses! Expected " << msg
              << " but the precious found something else on line " << peek(-1).value().line
              << "!" << std::endl;
    exit(EXIT_FAILURE);
}
