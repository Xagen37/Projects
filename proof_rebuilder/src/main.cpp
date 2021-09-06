#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

#include "Checker.h"
#include "expression.h"
#include "Parser.h"
#include "Rebuilder.h"

int main()
{
    //auto start = std::chrono::steady_clock::now();

    std::string inp;
    std::getline(std::cin, inp);
    auto parsed = Parser::parse_thesis(inp);

    std::vector<std::pair<std::unique_ptr<Base_expression>, Checker::Why>> statements;
    auto correctness = Checker::check_proof(parsed.first, parsed.second, statements);

    switch (correctness.first)
    {
        case Checker::Result::CORRECT: break;
        case Checker::Result::INCORRECT: std::cout << "Proof is incorrect at line " << correctness.second << std::endl; return 0;
        case Checker::Result::NOT_PROVE: std::cout << "The proof does not prove the required expression" << std::endl; return 0;
    }

    Rebuilder::rebuild(parsed.first, statements);

    /*auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "\n";*/
    return 0;
}
