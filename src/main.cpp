/**
 * @file main.cpp
 * @brief Entry point for the Precious compiler.
 *
 * This file orchestrates the compilation pipeline:
 * 1. Read source file
 * 2. Tokenize (lexer)
 * 3. Parse (build AST)
 * 4. Generate C++ source code
 * 5. Compile with g++
 *
 * Usage:
 *   precious <input.precious>
 *
 * The compiler produces an executable named after the input file (e.g., "foo" from "foo.precious").
 */

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

#include "./generation.hpp"
#include "./parser.hpp"

/**
 * @brief Main entry point.
 * @param argc Argument count (expects 2: program name + input file).
 * @param argv Argument vector (expects argv[1] = input filename).
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 *
 * Compilation pipeline:
 * 1. Validates command-line arguments
 * 2. Reads the input .precious file into a string
 * 3. Tokenizes the source code into a token stream
 * 4. Parses tokens into an AST
 * 5. Generates C++ source code from the AST
 * 6. Writes C++ code to <basename>.cpp
 * 7. Compiles <basename>.cpp with g++ to produce the final executable
 */
int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "[ERROR] Where is the precious?! Correct usage is..." << std::endl;
        std::cerr << "  precious <input.precious>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string contents;
    {
        std::stringstream contents_stream;
        std::fstream input(argv[1], std::ios::in);
        contents_stream << input.rdbuf();
        contents = contents_stream.str();
    }

    Tokenizer tokenizer(std::move(contents));
    std::vector<Token> tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if (!prog.has_value()) {
        std::cerr << "[ERROR] The precious... the precious is broken! Invalid program!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string inputPath(argv[1]);
    std::string baseName = inputPath.substr(inputPath.rfind('/') + 1);
    baseName = baseName.substr(0, baseName.rfind('.'));

    {
        Generator generator(prog.value());
        std::fstream file(baseName + ".cpp", std::ios::out);
        file << generator.gen_prog();
    }

    std::string cmd = "g++ -std=c++17 -o " + baseName + " " + baseName + ".cpp -w";
    system(cmd.c_str());

    return EXIT_SUCCESS;
}