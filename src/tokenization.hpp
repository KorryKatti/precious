/**
 * @file tokenization.hpp
 * @brief Lexer/tokenizer for the Precious programming language.
 *
 * The tokenizer converts raw source code text into a stream of tokens that
 * the parser can consume. It handles:
 * - Keywords: gives, we_haves, if, elif, else
 * - Identifiers and integer literals
 * - Operators: +, -, *, /, =
 * - Delimiters: (, ), {, }, ;
 * - Comments: // (line) and /* (block) *\/
 * - Whitespace and newline tracking for error reporting
 */

#ifndef TOKENIZATION_HPP
#define TOKENIZATION_HPP
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#pragma once

/**
 * @enum TokenType
 * @brief Enumerates all token types recognized by the Precious language.
 *
 * Each value represents a distinct lexical element that can appear in source code.
 */
enum class TokenType {
    exit,           ///< Keyword: "gives" (exit/return statement)
    int_lit,        ///< Integer literal (sequence of digits)
    semi,           ///< Semicolon: ;
    open_paren,     ///< Opening parenthesis: (
    close_paren,    ///< Closing parenthesis: )
    ident,          ///< User-defined identifier
    let,            ///< Keyword: "we_haves" (variable declaration)
    eq,             ///< Assignment operator: =
    plus,           ///< Addition operator: +
    star,           ///< Multiplication operator: *
    minus,          ///< Subtraction operator: -
    fslash,         ///< Division operator: /
    open_curly,     ///< Opening brace: {
    close_curly,    ///< Closing brace: }
    if_,            ///< Keyword: "if"
    elif,           ///< Keyword: "elif"
    else_,           ///< Keyword: "else"
    // on my own now
    eqeq,        ///< Equality operator: ==
    noteq,       ///< Inequality operator: !=
    lt,          ///< Less than operator: <
    gt,          ///< Greater than operator: >
    lteq,        ///< Less than or equal operator: <=
    gteq,        ///< Greater than or equal operator: >=
    and_,       /// < and operator ( and )
    or_,        /// < or operator ( or )
    bang,         /// < not operator ( ! )
    while_,     /// < while operator ( while )
    print_,     /// < print operator ( print )
    // shiver me timber
    fn_,        /// < function operator ( fn )
};

/**
 * @brief Converts a TokenType to its human-readable string representation.
 * @param type The token type to convert.
 * @return A string showing the literal token or its name (e.g., "`gives`", "int literal").
 */
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
        // on my own now
        case TokenType::eqeq: return "`==`";
        case TokenType::noteq: return "`!=`";
        case TokenType::lt: return "`<`";
        case TokenType::gt: return "`>`";
        case TokenType::lteq: return "`<=`";
        case TokenType::gteq: return "`>=`";
        case TokenType::and_: return "`and`";
        case TokenType::or_: return "`or`";
        case TokenType::bang: return "`!`";
        case TokenType::while_: return "`while`";
        case TokenType::print_: return "`say`";
        case TokenType::fn_: return "`fn`";
        default: return "unknown token type";
    }
    assert(false);
}

/**
 * @brief Returns the binary operator precedence for a given token type.
 * @param type The token type to check.
 * @return The precedence level (0 for plus/minus, 1 for star/slash), or std::nullopt for non-operators.
 *
 * Used by the parser to implement correct operator precedence in expressions.
 * Higher values indicate tighter binding (evaluated first).
 */
inline std::optional<int> bin_prec(const TokenType type){
    switch(type){

        case TokenType::or_:
        return 0;
        case TokenType::and_:
        return 1;
        case TokenType::eqeq:
        case TokenType::noteq:
        case TokenType::lt:
        case TokenType::gt:
        case TokenType::lteq:
        case TokenType::gteq:
        return 2;
        case TokenType::plus:
        case TokenType::minus:
        return 3;
        case TokenType::star:
        case TokenType::fslash:
        return 4;
        default:
        return {};
    }
}

/**
 * @struct Token
 * @brief Represents a single lexical token from the source code.
 *
 * Each token records its type, source line number (for error messages),
 * and optionally a string value (for identifiers and integer literals).
 */
struct Token {
    TokenType type;                  ///< The type of this token.
    int line;                        ///< Source line number where this token appears.
    std::optional<std::string> value; ///< Optional value (identifier name or literal text).
};

/**
 * @class Tokenizer
 * @brief Converts raw source code into a vector of Token objects.
 *
 * The tokenizer performs lexical analysis by scanning the input string character
 * by character. It groups characters into tokens based on the language's lexical rules.
 *
 * Usage:
 * @code
 *   Tokenizer tokenizer(source_code);
 *   std::vector<Token> tokens = tokenizer.tokenize();
 * @endcode
 */
class Tokenizer {
public:
    /**
     * @brief Constructs a tokenizer with the given source code string.
     * @param src The source code to tokenize.
     */
    explicit Tokenizer(const std::string src) : m_src(std::move(src)) {}

    /**
     * @brief Tokenizes the source code and returns a vector of tokens.
     * @return A vector of Token objects representing the lexical elements.
     * @throws Exits with EXIT_FAILURE on invalid tokens.
     *
     * Scans the entire source string and produces tokens. Handles:
     * - Multi-character identifiers and keywords
     * - Multi-digit integer literals
     * - Single-character operators and delimiters
     * - Line comments (//) and block comments (/* ... *\/)
     * - Whitespace and newlines (skipped, but newlines increment line counter)
     */
    std::vector<Token> tokenize() {
        std::string buf;
        std::vector<Token> tokens;
        int line_count = 1;
        while (peek().has_value()) {
            // Identifiers and keywords: start with letter or underscore
            if (std::isalpha(peek().value()) || peek().value() == '_') {
                buf.push_back(consume());
                while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_')) {
                    buf.push_back(consume());
                }
                // Check for reserved keywords
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

                }else if (buf=="and"){
                    tokens.push_back({.type=TokenType::and_, .line = line_count});
                    buf.clear();
                    continue;
                }else if (buf=="or"){
                    tokens.push_back({.type=TokenType::or_, .line = line_count});
                    buf.clear();
                    continue;
                }else if (buf=="while"){
                    tokens.push_back({.type=TokenType::while_, .line = line_count});
                    buf.clear();
                    continue;
                } else if (buf=="say"){
                    tokens.push_back({.type=TokenType::print_, .line = line_count});
                    buf.clear();
                    continue;
                } else if (buf=="fn"){
                    tokens.push_back({.type=TokenType::fn_, .line = line_count});
                    buf.clear();
                    continue;
                }

                // Not a keyword — it's a user-defined identifier
                else {
                    tokens.push_back({.type=TokenType::ident, .line = line_count, .value=buf});
                    buf.clear();
                }
            }
            // Integer literals: sequence of digits
            else if (std::isdigit(peek().value())) {
                buf.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buf.push_back(consume());
                }
                tokens.push_back({.type = TokenType::int_lit, .line = line_count, .value = buf});
                buf.clear();
                continue;
            }
            // Line comment: // ... (consume until newline)
            else if (peek().value()=='/' && peek(1).has_value()&&peek(1).value()=='/'){
                consume();
                consume();
                while (peek().has_value()&&peek().value()!='\n'){
                    consume();
                }
                continue;
            }
            // Block comment: /* ... */ (consume until closing delimiter)
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

            // Single-character tokens: parentheses, operators, delimiters
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
            // on my own now
            // equality ops
            else if (peek().value()=='=' && peek(1).has_value() && peek(1).value()=='='){
                consume();
                consume();
                tokens.push_back({.type=TokenType::eqeq, .line = line_count});
                continue;
            }
            else if (peek().value()=='!' && peek(1).has_value() && peek(1).value()=='='){
                consume();
                consume();
                tokens.push_back({.type=TokenType::noteq, .line = line_count});
                continue;
            } // not equal to
            else if (peek().value()=='<' && peek(1).has_value() && peek(1).value()=='='){
                consume();
                consume();
                tokens.push_back({.type=TokenType::lteq, .line = line_count});
                continue;
            } // less than or equal to
            else if (peek().value()=='>' && peek(1).has_value() && peek(1).value()=='='){
                consume();
                consume();
                tokens.push_back({.type=TokenType::gteq, .line = line_count});
                continue;
            } // greater than or equal to
            else if (peek().value()=='<'){
                consume();
                tokens.push_back({.type=TokenType::lt, .line = line_count});
                continue;
            } // less than
            else if (peek().value()=='>'){
                consume();
                tokens.push_back({.type=TokenType::gt, .line = line_count});
                continue;
            } // greater than
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
            }else if (peek().value()=='!'){
                consume();
                tokens.push_back({.type=TokenType::bang, .line = line_count});
                continue;
            }
            // Newline: advance line counter for error reporting
            else if (peek().value() == '\n') {
                consume();
                line_count++;
                continue;
            }
            // Whitespace (space, tab, etc.): skip silently
            else if (std::isspace(peek().value())) {
                consume();
                continue;
            }
            else {
                std::cerr << "[ERROR] Nasty little token! '" << peek().value()
                          << "' is not understood, no it isn't, precious! (line " << line_count << ")" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        m_index = 0;
        return tokens;
    }

private:
    /**
     * @brief Peeks at a character at the given offset from current position.
     * @param offset Number of characters ahead to peek (default 0 = current).
     * @return The character at that position, or std::nullopt if out of bounds.
     */
    [[nodiscard]] std::optional<char> peek(int offset = 0) const {
        if (m_index + offset >= m_src.length()) {
            return {};
        }
        return m_src.at(m_index+offset);
    }

    /**
     * @brief Consumes and returns the current character, advancing the position.
     * @return The consumed character.
     */
    char consume() { return m_src.at(m_index++); }

    const std::string m_src;  ///< The source code string being tokenized.
    size_t m_index = 0;       ///< Current read position in the source string.
};

#endif  // TOKENIZATION_HPP
