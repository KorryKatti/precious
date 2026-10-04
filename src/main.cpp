/**
 * @file main.cpp
 * @brief Entry point for the Precious compiler.
 *
 * This file does the whole job in five steps, and nothing else:
 *
 *   1. read the .precious file into a string
 *   2. tokenize it          (tokenization.hpp)  text   -> list of tokens
 *   3. parse it             (parser.hpp)        tokens -> AST
 *   4. generate C++ from it (generation.hpp)   AST    -> C++ source
 *   5. hand that C++ to g++, which makes the executable
 *
 * Usage:
 *   precious my_program.precious
 *
 * The executable is named after the input file, so my_program.precious becomes
 * ./my_program. Note that the generated C++ and the executable are both written
 * into the current directory, not next to the source file.
 */

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "generation.hpp"
#include "parser.hpp"

/// Reads a whole file into a string. Returns an empty string if it cannot be
/// opened -- the tokenizer will then report that it found nothing, which is a
/// confusing message, so this is worth noticing while testing.
static std::string read_file(const std::string& path) {
    std::ifstream input(path);
    std::stringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

/// Writes `text` to `path`, replacing anything already there.
static void write_file(const std::string& path, const std::string& text) {
    std::ofstream output(path);
    output << text;
}

/// Runs a shell command and waits for it. Returns the command's exit status.
static int run(const std::string& command) { return system(command.c_str()); }

/// The file's name without its directory or extension:
///   "tests/01_basic.precious"  ->  "01_basic"
static std::string base_name_of(const std::string& path) {
    const size_t slash = path.rfind('/');
    const std::string file_name = path.substr(slash + 1);
    const size_t dot = file_name.rfind('.');
    return file_name.substr(0, dot);
}

int main(const int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "[ERROR] Where is the precious?! Correct usage is..." << std::endl;
        std::cerr << "  precious <input.precious>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input_path = argv[1];

    // --- Step 2: source text -> tokens ---
    const std::string source = read_file(input_path);
    Tokenizer tokenizer(source);
    const std::vector<Token> tokens = tokenizer.tokenize();

    // --- Step 3: tokens -> AST ---
    Parser parser(tokens);
    std::optional<NodeProg> program = parser.parse_prog();
    if (!program.has_value()) {
        std::cerr << "[ERROR] The precious... the precious is broken! Invalid program!"
                  << std::endl;
        return EXIT_FAILURE;
    }

    // --- Step 4: AST -> C++ source ---
    const std::string base_name = base_name_of(input_path);
    Generator generator(program.value());
    write_file(base_name + ".cpp", generator.gen_prog());

    // --- Step 5: C++ source -> executable ---
    //
    // -w silences warnings from the generated code. The generated C++ is not
    // something the programmer wrote, so warning about it would be noise.
    const std::string compile_command = "g++ -std=c++17 -o " + base_name + " " +
                                        base_name + ".cpp -w";
    if (run(compile_command) != 0) {
        // g++ has already printed its own error, so just say which file it was.
        std::cerr << "[ERROR] The gollum-speak broke: g++ could not build " << base_name
                  << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}