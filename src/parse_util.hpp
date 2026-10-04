/**
 * @file parse_util.hpp
 * @brief Token stream helpers for the Precious parser.
 *
 * The parser keeps a cursor pointing at "the token we are looking at", and these
 * are the four ways to move it or look at it.
 */

// Included at the bottom of parser.hpp after the Parser class definition.

// Looks at the token `offset` positions ahead of the cursor without moving.
// Returns nothing if we are at (or past) the end of the file.
//   offset 0 = the current token, 1 = the one after that, -1 = the one before.
std::optional<Token> Parser::peek(const int offset) const {
    if (m_index + offset < 0 || m_index + offset >= m_tokens.size()) {
        return std::nullopt;
    }
    return m_tokens.at(m_index + offset);
}

// Takes the current token and steps forward one. The cursor is never allowed to
// run past the end, because the parser only calls this when peek() has already
// confirmed there is a token there.
Token Parser::consume() { return m_tokens.at(m_index++); }

// Takes the current token only if it is of the given type. If it is not, the
// token is left alone and nothing is returned -- so the caller can try a
// different kind of statement without having consumed anything.
std::optional<Token> Parser::try_consume(const TokenType type) {
    if (peek().has_value() && peek().value().type == type) {
        return consume();
    }
    return std::nullopt;
}

// Same as try_consume(), except that being the wrong token is an error. Use
// this for the punctuation that must be there, like a closing `)`.
Token Parser::try_consume_err(const TokenType type) {
    auto found = try_consume(type);
    if (found.has_value()) {
        return found.value();
    }
    error_expected(to_string(type));
}

// Reports a syntax error in the Precious style and stops the compiler. Because
// this never returns, callers do not need a dead `return {};` after it -- which
// is why it is marked [[noreturn]] in parser.hpp.
void Parser::error_expected(const std::string& msg) const {
    std::cerr << "[ERROR] Trickses! Trickses! Expected " << msg
              << " but the precious found something else on line " << peek(-1).value().line
              << "!" << std::endl;
    exit(EXIT_FAILURE);
}