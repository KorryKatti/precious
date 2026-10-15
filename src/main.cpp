/**
 * @file main.cpp
 * @brief Entry point for the Precious compiler.
 *
 * This file does the whole job in six steps, and nothing else:
 *
 *   1. read the .precious file into a string
 *   2. tokenize it          (tokenization.hpp)  text   -> list of tokens
 *   3. parse it             (parser.hpp)        tokens -> AST
 *   4. check its types      (typechecker.hpp)   AST    -> complaints, if any
 *   5. generate C++ from it (generation.hpp)   AST    -> C++ source
 *   6. hand that C++ to g++, which makes the executable
 *
 * Usage:
 *   precious my_program.precious
 *
 * The executable is named after the input file and is written into ./out/, so
 * my_program.precious becomes ./out/my_program. The generated C++ sits beside it
 * as ./out/my_program.cpp. Nothing is written into the current directory.
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "generation.hpp"
#include "parser.hpp"
#include "typechecker.hpp"

/// Everything the compiler produces lands here, so the project directory stays
/// free of generated files.
static const char* const OUTPUT_DIR = "out";

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

    // --- Step 4: AST -> are the types right? ---
    //
    // The checker never changes the program, it only reports. If it finds
    // anything wrong, stop here: there is no point handing g++ a program that
    // was never going to compile.
    TypeChecker checker(program.value());
    if (!checker.check()) {
        std::cerr << "[ERROR] It is not safe to cast these types, precious!" << std::endl;
        return EXIT_FAILURE;
    }

    // --- Step 5: AST -> C++ source ---
    //
    // The C++ and the executable both go into ./out/, which is created if it is
    // not there yet. That keeps the project directory free of generated files.
    const std::string base_name = base_name_of(input_path);
    const std::string cpp_path = std::string(OUTPUT_DIR) + "/" + base_name + ".cpp";
    const std::string exe_path = std::string(OUTPUT_DIR) + "/" + base_name;
    std::error_code ignored;
    std::filesystem::create_directories(OUTPUT_DIR, ignored);

    Generator generator(program.value());
    write_file(cpp_path, generator.gen_prog());

    // --- Step 6: C++ source -> executable ---
    //
    // -w silences warnings from the generated code. The generated C++ is not
    // something the programmer wrote, so warning about it would be noise.
    const std::string compile_command =
        "g++ -std=c++17 -o " + exe_path + " " + cpp_path + " -w";
    if (run(compile_command) != 0) {
        // g++ has already printed its own error, so just say which file it was.
        std::cerr << "[ERROR] The gollum-speak broke: g++ could not build " << base_name
                  << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}