/**
 * @file main.cpp
 * @brief Entry point for the Precious compiler.
 *
 * This file orchestrates the compilation pipeline:
 * 1. Read source file
 * 2. Tokenize (lexer)
 * 3. Parse (build AST)
 * 4. Generate x86-64 assembly
 * 5. Assemble with NASM
 * 6. Link with ld
 *
 * Usage:
 *   precious <input.precious>
 *
 * The compiler produces an executable named "out" in the current directory.
 */

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

#include "./generation.hpp"

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
 * 5. Generates x86-64 assembly from the AST
 * 6. Writes assembly to out.asm
 * 7. Assembles out.asm to out.o using NASM
 * 8. Links out.o to create the final executable using ld
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

    {
        Generator generator(prog.value());
        std::fstream file("out.asm", std::ios::out);
        file << generator.gen_prog();
    }

    system("nasm -felf64 out.asm");
    system("ld -o out out.o");

    return EXIT_SUCCESS;
}