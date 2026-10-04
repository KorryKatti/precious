/**
 * @file tokenization.hpp
 * @brief Lexer/tokenizer for the Precious programming language.
 *
 * The tokenizer turns source text into a list of Token structs. The parser then
 * reads that list. Nothing else in the compiler looks at raw source text.
 *
 * What it recognises:
 * - Keywords and identifiers   my, fn, while, hello_world, ...
 * - Integer literals           42
 * - String literals            "hello"
 * - Operators                  + - * / % == != < > <= >= = += -= *= /= %= ++ --
 *                              and -> ->
 * - Delimiters                 ( ) { } [ ] ; , :
 * - Comments                   // to end of line,  /* ... *\/
 *
 * ADDING A KEYWORD? Add one line to keywords(). Nothing else needs to change.
 */

#ifndef TOKENIZATION_HPP
#define TOKENIZATION_HPP

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @enum TokenType
 * @brief Every kind of token the language has.
 *
 * One value per lexical element. Names ending in `_` are keywords that would
 * clash with a C++ keyword (if_, else_, default_, continue_, break_).
 */
enum class TokenType {
    exit,            ///< `gives`
    int_lit,         ///< 42
    semi,            ///< ;
    open_paren,      ///< (
    close_paren,     ///< )
    ident,           ///< a name made up by the programmer
    let,             ///< `my`
    eq,              ///< =
    plus,            ///< +
    star,            ///< *
    minus,           ///< -
    fslash,          ///< /
    open_curly,      ///< {
    close_curly,     ///< }
    if_,             ///< `if`
    elif,            ///< `elif`
    else_,           ///< `else`
    eqeq,            ///< ==
    noteq,           ///< !=
    lt,              ///< <
    gt,              ///< >
    lteq,            ///< <=
    gteq,            ///< >=
    and_,            ///< `and`
    or_,             ///< `or`
    bang,            ///< !
    while_,          ///< `while`
    print_,          ///< `say`
    fn_,             ///< `fn`
    comma_,          ///< ,
    string_lit,      ///< "hello"
    colon_,          ///< :   (used in `my x: number = 5`)
    type_number_,    ///< `number`
    type_word_,      ///< `word`
    type_question_,  ///< `question`
    type_decimal_,   ///< `decimal`
    type_letter,     ///< `letter`
    open_square,     ///< [
    close_square,    ///< ]
    modulo,          ///< %
    return_arrow,    ///< ->
    break_,          ///< `break`
    continue_,       ///< `continue`
    switch_,         ///< `switch`
    case_,           ///< `case`
    default_,        ///< `default`
    for_,            ///< `for`
    in_,             ///< `in`   (for-each loops)
    push_,           ///< `push`
    pop_,            ///< `pop`
    pluseq,          ///< +=
    minuseq,         ///< -=
    stareq,          ///< *=
    fslasheq,        ///< /=
    moduloeq,        ///< %=
    plusplus,        ///< ++
    minusminus,      ///< --
};

/**
 * @brief A human-readable name for a token, used in error messages.
 * @param type The token to describe.
 * @return The token written the way a programmer would type it, e.g. "`gives`".
 */
inline std::string to_string(const TokenType type) {
    if (type == TokenType::exit) return "`gives`";
    if (type == TokenType::int_lit) return "int literal";
    if (type == TokenType::semi) return "`;`";
    if (type == TokenType::open_paren) return "`(`";
    if (type == TokenType::close_paren) return "`)`";
    if (type == TokenType::ident) return "identifier";
    if (type == TokenType::let) return "`my`";
    if (type == TokenType::eq) return "`=`";
    if (type == TokenType::plus) return "`+`";
    if (type == TokenType::star) return "`*`";
    if (type == TokenType::minus) return "`-`";
    if (type == TokenType::fslash) return "`/`";
    if (type == TokenType::open_curly) return "`{`";
    if (type == TokenType::close_curly) return "`}`";
    if (type == TokenType::if_) return "`if`";
    if (type == TokenType::elif) return "`elif`";
    if (type == TokenType::else_) return "`else`";
    if (type == TokenType::eqeq) return "`==`";
    if (type == TokenType::noteq) return "`!=`";
    if (type == TokenType::lt) return "`<`";
    if (type == TokenType::gt) return "`>`";
    if (type == TokenType::lteq) return "`<=`";
    if (type == TokenType::gteq) return "`>=`";
    if (type == TokenType::and_) return "`and`";
    if (type == TokenType::or_) return "`or`";
    if (type == TokenType::bang) return "`!`";
    if (type == TokenType::while_) return "`while`";
    if (type == TokenType::print_) return "`say`";
    if (type == TokenType::fn_) return "`fn`";
    if (type == TokenType::comma_) return "`,`";
    if (type == TokenType::string_lit) return "string literal";
    if (type == TokenType::colon_) return "`:`";
    if (type == TokenType::type_number_) return "number";
    if (type == TokenType::type_word_) return "word";
    if (type == TokenType::type_question_) return "question";
    if (type == TokenType::type_decimal_) return "decimal";
    if (type == TokenType::type_letter) return "letter";
    if (type == TokenType::open_square) return "`[`";
    if (type == TokenType::close_square) return "`]`";
    if (type == TokenType::modulo) return "`%`";
    if (type == TokenType::return_arrow) return "`->`";
    if (type == TokenType::break_) return "`break`";
    if (type == TokenType::continue_) return "`continue`";
    if (type == TokenType::switch_) return "`switch`";
    if (type == TokenType::case_) return "`case`";
    if (type == TokenType::default_) return "`default`";
    if (type == TokenType::for_) return "`for`";
    if (type == TokenType::in_) return "`in`";
    if (type == TokenType::push_) return "`push`";
    if (type == TokenType::pop_) return "`pop`";
    if (type == TokenType::pluseq) return "`+=`";
    if (type == TokenType::minuseq) return "`-=`";
    if (type == TokenType::stareq) return "`*=`";
    if (type == TokenType::fslasheq) return "`/=`";
    if (type == TokenType::moduloeq) return "`%=`";
    if (type == TokenType::plusplus) return "`++`";
    if (type == TokenType::minusminus) return "`--`";
    return "unknown token type";
}

/**
 * @brief The keyword table: spelling -> token type.
 *
 * This is the only place a new reserved word has to be added.
 */
inline const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> table = {
        {"gives", TokenType::exit},        {"my", TokenType::let},
        {"if", TokenType::if_},           {"elif", TokenType::elif},
        {"else", TokenType::else_},       {"and", TokenType::and_},
        {"or", TokenType::or_},           {"while", TokenType::while_},
        {"say", TokenType::print_},       {"fn", TokenType::fn_},
        {"number", TokenType::type_number_},   {"word", TokenType::type_word_},
        {"question", TokenType::type_question_}, {"decimal", TokenType::type_decimal_},
        {"letter", TokenType::type_letter}, {"break", TokenType::break_},
        {"continue", TokenType::continue_}, {"switch", TokenType::switch_},
        {"case", TokenType::case_},       {"default", TokenType::default_},
        {"for", TokenType::for_},         {"in", TokenType::in_},
        {"push", TokenType::push_},       {"pop", TokenType::pop_},
    };
    return table;
}

/**
 * @brief How tightly a binary operator binds.
 * @param type The token to check.
 * @return The precedence level, or nothing at all if it is not a binary operator.
 *
 * Used by the parser to work out grouping. Higher numbers bind tighter, so
 * `a + b * c` groups as `a + (b * c)`.
 *
 * Adding an operator? Add it here, and it will be grouped correctly for free.
 */
inline std::optional<int> bin_prec(const TokenType type) {
    if (type == TokenType::or_) return 0;
    if (type == TokenType::and_) return 1;
    if (type == TokenType::eqeq || type == TokenType::noteq || type == TokenType::lt ||
        type == TokenType::gt || type == TokenType::lteq || type == TokenType::gteq) {
        return 2;
    }
    if (type == TokenType::plus || type == TokenType::minus) return 3;
    if (type == TokenType::star || type == TokenType::fslash || type == TokenType::modulo) {
        return 4;
    }
    return std::nullopt; // not a binary operator
}

/**
 * @struct Token
 * @brief One lexical element, produced by the tokenizer.
 */
struct Token {
    TokenType type;                   ///< What kind of token this is.
    int line;                         ///< Which source line it came from, for errors.
    std::optional<std::string> value; ///< The text, for identifiers and literals only.
};

/**
 * @class Tokenizer
 * @brief Turns source code into a list of Token structs.
 *
 * Scans the text once, left to right. At each position it asks a series of
 * questions -- "is this a word?", "is this a two-character operator?", "is this
 * just whitespace?" -- and the first question that says yes wins.
 *
 * Usage:
 * @code
 *   Tokenizer tokenizer(source_code);
 *   std::vector<Token> tokens = tokenizer.tokenize();
 * @endcode
 */
class Tokenizer {
public:
    explicit Tokenizer(const std::string src) : m_src(std::move(src)) {}

    /**
     * @brief Scans the whole source text.
     * @return Every token, in source order.
     *
     * Exits the process with a message if it meets a character it does not
     * understand, or an unterminated string.
     */
    std::vector<Token> tokenize();

private:
    // Each try_* function looks at the current position and answers "is this
    // you?". If yes it consumes what it needs, adds a token, and returns true.
    // If no it consumes nothing and returns false. That is why they are safe to
    // call in any order -- except that two-character operators must be tried
    // before one-character ones, or `==` would be read as two `=`.

    bool try_read_word(std::string& buf, std::vector<Token>& tokens, int line);
    bool try_read_number(std::string& buf, std::vector<Token>& tokens, int line);
    bool try_skip_comment();
    bool try_read_two_char_operator(std::vector<Token>& tokens, int line);
    bool try_read_single_char_token(std::vector<Token>& tokens, int line);
    bool try_read_string_literal(std::string& buf, std::vector<Token>& tokens, int line);

    // Appends one token. `value` is left out for operators and punctuation,
    // which have no text of their own.
    void add_token(std::vector<Token>& tokens, TokenType type, int line,
                   const std::string& value = "");

    /// Looks at the character `offset` positions ahead of the cursor.
    std::optional<char> peek(int offset = 0) const;

    /// Takes the character at the cursor and steps forward one.
    char consume();

    const std::string m_src; ///< The source text being scanned.
    size_t m_index = 0;      ///< Where we are in that text.
};

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    std::string buf;   // reused for words, numbers and strings
    int line = 1;

    while (peek().has_value()) {
        if (try_read_word(buf, tokens, line)) continue;
        if (try_read_number(buf, tokens, line)) continue;
        if (try_skip_comment()) continue;
        // Two-character operators come before one-character ones on purpose.
        if (try_read_two_char_operator(tokens, line)) continue;
        if (try_read_single_char_token(tokens, line)) continue;
        if (try_read_string_literal(buf, tokens, line)) continue;

        // A newline is not a token, but it moves the line counter along so
        // error messages can point at the right line.
        if (peek().value() == '\n') {
            consume();
            line++;
            continue;
        }

        // Spaces and tabs carry no meaning, so drop them.
        if (std::isspace(peek().value())) {
            consume();
            continue;
        }

        std::cerr << "[ERROR] Nasty little token! '" << peek().value()
                  << "' is not understood, no it isn't, precious! (line " << line << ")"
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    m_index = 0; // rewind, so tokenize() can be called again
    return tokens;
}

// A word is a letter or underscore, followed by letters, digits or underscores.
// `my_count` is one word. It is either a keyword or a programmer's identifier.
bool Tokenizer::try_read_word(std::string& buf, std::vector<Token>& tokens, const int line) {
    if (!peek().has_value() || !(std::isalpha(peek().value()) || peek().value() == '_')) {
        return false;
    }

    buf.clear();
    while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_')) {
        buf.push_back(consume());
    }

    const auto& table = keywords();
    auto found = table.find(buf);
    if (found != table.end()) {
        add_token(tokens, found->second, line); // a reserved word
        return true;
    }
    add_token(tokens, TokenType::ident, line, buf);
    return true;
}

// A number is one or more digits: 0, 42, 1024.
bool Tokenizer::try_read_number(std::string& buf, std::vector<Token>& tokens, const int line) {
    if (!peek().has_value() || !std::isdigit(peek().value())) {
        return false;
    }

    buf.clear();
    while (peek().has_value() && std::isdigit(peek().value())) {
        buf.push_back(consume());
    }

    add_token(tokens, TokenType::int_lit, line, buf);
    return true;
}

// Two kinds of comment, both of which produce no tokens at all:
//   // everything up to the end of the line
//   /* everything up to the next */
// Comments deliberately do not count lines, so line numbers in error messages
// can be off inside a multi-line comment.
bool Tokenizer::try_skip_comment() {
    if (!peek().has_value() || peek().value() != '/') {
        return false;
    }
    const std::optional<char> next = peek(1);
    if (!next.has_value()) {
        return false;
    }

    if (next.value() == '/') {
        consume(); // /
        consume(); // /
        while (peek().has_value() && peek().value() != '\n') {
            consume();
        }
        return true;
    }

    if (next.value() == '*') {
        consume(); // /
        consume(); // *
        // Walk forwards until we are sitting on the closing * /.
        while (peek().has_value()) {
            if (peek().value() == '*' && peek(1).has_value() && peek(1).value() == '/') {
                break;
            }
            consume();
        }
        if (peek().has_value()) consume(); // *
        if (peek().has_value()) consume(); // /
        return true;
    }

    return false; // it is a `/` or `/=` instead, so not a comment
}

// Operators made of exactly two characters.
bool Tokenizer::try_read_two_char_operator(std::vector<Token>& tokens, const int line) {
    const std::optional<char> first = peek();
    const std::optional<char> second = peek(1);
    if (!first.has_value() || !second.has_value()) {
        return false;
    }

    const char a = first.value();
    const char b = second.value();

    TokenType type;
    if (a == '=' && b == '=') {
        type = TokenType::eqeq;
    } else if (a == '!' && b == '=') {
        type = TokenType::noteq;
    } else if (a == '<' && b == '=') {
        type = TokenType::lteq;
    } else if (a == '>' && b == '=') {
        type = TokenType::gteq;
    } else if (a == '+' && b == '=') {
        type = TokenType::pluseq;
    } else if (a == '+' && b == '+') {
        type = TokenType::plusplus;
    } else if (a == '-' && b == '>') {
        type = TokenType::return_arrow;
    } else if (a == '-' && b == '-') {
        type = TokenType::minusminus;
    } else if (a == '-' && b == '=') {
        type = TokenType::minuseq;
    } else if (a == '*' && b == '=') {
        type = TokenType::stareq;
    } else if (a == '/' && b == '=') {
        type = TokenType::fslasheq;
    } else if (a == '%' && b == '=') {
        type = TokenType::moduloeq;
    } else {
        return false;
    }

    consume();
    consume();
    add_token(tokens, type, line);
    return true;
}

// Operators and punctuation made of exactly one character.
bool Tokenizer::try_read_single_char_token(std::vector<Token>& tokens, const int line) {
    if (!peek().has_value()) {
        return false;
    }
    const char c = peek().value();

    TokenType type;
    if (c == '(') {
        type = TokenType::open_paren;
    } else if (c == ')') {
        type = TokenType::close_paren;
    } else if (c == ';') {
        type = TokenType::semi;
    } else if (c == '<') {
        type = TokenType::lt;
    } else if (c == '>') {
        type = TokenType::gt;
    } else if (c == '=') {
        type = TokenType::eq;
    } else if (c == '+') {
        type = TokenType::plus;
    } else if (c == '*') {
        type = TokenType::star;
    } else if (c == '-') {
        type = TokenType::minus;
    } else if (c == '/') {
        type = TokenType::fslash;
    } else if (c == '%') {
        type = TokenType::modulo;
    } else if (c == ',') {
        type = TokenType::comma_;
    } else if (c == '{') {
        type = TokenType::open_curly;
    } else if (c == '}') {
        type = TokenType::close_curly;
    } else if (c == '!') {
        type = TokenType::bang;
    } else if (c == ':') {
        type = TokenType::colon_;
    } else if (c == '[') {
        type = TokenType::open_square;
    } else if (c == ']') {
        type = TokenType::close_square;
    } else {
        return false;
    }

    consume();
    add_token(tokens, type, line);
    return true;
}

// A string is a double quote, then everything up to the next double quote.
// There is no way to escape a quote inside a string, so a string cannot contain
// one.
bool Tokenizer::try_read_string_literal(std::string& buf, std::vector<Token>& tokens,
                                        const int line) {
    if (!peek().has_value() || peek().value() != '"') {
        return false;
    }
    consume(); // opening quote

    buf.clear();
    while (peek().has_value() && peek().value() != '"') {
        buf.push_back(consume());
    }

    if (!peek().has_value()) {
        std::cerr << "[ERROR] Unterminated string literal at line " << line << std::endl;
        exit(EXIT_FAILURE);
    }
    consume(); // closing quote

    add_token(tokens, TokenType::string_lit, line, buf);
    return true;
}

void Tokenizer::add_token(std::vector<Token>& tokens, const TokenType type, const int line,
                          const std::string& value) {
    tokens.push_back({.type = type, .line = line});
    // Only identifiers and literals carry text.
    if (!value.empty()) {
        tokens.back().value = value;
    }
}

std::optional<char> Tokenizer::peek(const int offset) const {
    if (m_index + offset >= m_src.length()) {
        return std::nullopt;
    }
    return m_src.at(m_index + offset);
}

char Tokenizer::consume() { return m_src.at(m_index++); }

#endif // TOKENIZATION_HPP