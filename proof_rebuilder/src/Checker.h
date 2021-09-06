#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "expression.h"
#include "Parser.h"

class Checker
{
    using ptr_t = std::unique_ptr<Base_expression>;
    using base_ptr_t = Base_expression *;
public:
    enum class Result
    {
        INCORRECT,
        NOT_PROVE,
        CORRECT
    };

    struct Why
    {
        enum class From
        {
            AXIOM,
            HYPOTHESIS,
            MODUS_PONENS
        };
        From from;
        std::variant<int, Base_expression*, std::pair<int, int>> details;
    };

    static std::pair<Result, int> check_proof(const std::vector<ptr_t>& hypothesis, const ptr_t& thesis, std::vector<std::pair<ptr_t, Why>>& statements)
    {
        if (axioms.empty())
        {
            axioms.reserve(10);
            axioms.emplace_back(std::move(Parser::parse("a -> b -> a")));
            axioms.emplace_back(std::move(Parser::parse("(a -> b) -> (a -> b -> c) -> (a -> c)")));
            axioms.emplace_back(std::move(Parser::parse("a -> b -> a & b")));
            axioms.emplace_back(std::move(Parser::parse("a & b -> a")));
            axioms.emplace_back(std::move(Parser::parse("a & b -> b")));
            axioms.emplace_back(std::move(Parser::parse("a -> a | b")));
            axioms.emplace_back(std::move(Parser::parse("b -> a | b")));
            axioms.emplace_back(std::move(Parser::parse("(a -> c) -> (b -> c) -> (a | b -> c)")));
            axioms.emplace_back(std::move(Parser::parse("(a -> b) -> (a -> !b) -> !a")));
            axioms.emplace_back(std::move(Parser::parse("a -> !a -> b")));
        }

        std::pair<Result, int> return_result = { Result::CORRECT, -1 };
        bool is_proof_ok = true;
        std::unordered_map<std::string, std::pair<int, base_ptr_t>> proofed;
        std::string expr;
        int row_cnt = 1;
        ptr_t ptr;
        while (std::getline(std::cin, expr))
        {
            row_cnt++;
            if (!is_proof_ok)
            { continue; }

            int ok = false;
            ptr = Parser::parse(expr);
            base_ptr_t checked = ptr.get();
            std::string checked_string = checked->as_string();

            if (proofed.count(checked_string) == 0)
            {
                ok = is_hypothesis(checked, hypothesis);
                if (ok != -1)
                {
                    proofed.insert({ checked_string, std::make_pair(statements.size(), checked) });
                    statements.emplace_back(std::make_pair(
                            std::move(ptr), 
                            Why{Why::From::HYPOTHESIS, std::variant<int, Base_expression*, std::pair<int, int>>(hypothesis[ok].get())}));
                    continue;
                }

                ok = is_axiom(checked);
                if (ok != -1)
                {
                    proofed.insert({ checked_string, std::make_pair(statements.size(), checked) });
                    statements.emplace_back(std::make_pair(
                        std::move(ptr),
                        Why{ Why::From::AXIOM, std::variant<int, Base_expression*, std::pair<int, int>>(ok) }));
                    continue;
                }

                std::pair<int, int> ok2 = is_modus_ponens(checked, proofed);
                if (ok2.first != -1)
                {
                    proofed.insert({ checked_string, std::make_pair(statements.size(), checked) });
                    statements.emplace_back(std::make_pair(
                        std::move(ptr),
                        Why{ Why::From::MODUS_PONENS, std::variant<int, Base_expression*, std::pair<int, int>>(ok2) }));
                    continue;
                }

                return_result = { Result::INCORRECT, row_cnt };
                is_proof_ok = false;
            }
        }

        if (!is_proof_ok)
        { return return_result; }

        std::size_t thesis_hash = thesis->get_hash();
        if (ptr && ptr->get_hash() != thesis_hash ||
            !ptr && statements.back().first->get_hash() != thesis_hash)
        { return { Result::NOT_PROVE, 0 }; }

        if (ptr)
        {
            while(statements.back().first->get_hash() != thesis_hash)
            { statements.pop_back(); }
        }
        

        return { Result::CORRECT, -1 };
    }

private:
    static int is_hypothesis(base_ptr_t checked, const std::vector<ptr_t>& hypothesis)
    {
        std::size_t checked_hash = checked->get_hash();
        for (int i = 0; i < hypothesis.size(); i++)
        {
            if (checked_hash == hypothesis[i]->get_hash())
            { return i; }
        }
        return -1;
    }

    static int is_axiom(base_ptr_t checked)
    {
        std::unordered_map<std::string, const Base_expression*> translator = { {"a", nullptr}, {"b", nullptr}, {"c", nullptr} };
        for (int i = 0; i < axioms.size(); i++)
        {
            if (checked->is_equal(axioms[i].get(), translator))
            { return i; }
            translator["a"] = nullptr;
            translator["b"] = nullptr;
            translator["c"] = nullptr;
        }
        return -1;
    }

    static std::pair<int, int> is_modus_ponens(base_ptr_t checked, std::unordered_map<std::string, std::pair<int, base_ptr_t>>& proofed)
    {
        std::size_t checked_hash = checked->get_hash();
        for (auto& it : proofed)
        {
            if (it.second.second->get_sign() == "->")
            {
                Operation* casted = static_cast<Operation*>(it.second.second);
                if (proofed.count(casted->get_left()->as_string()) && casted->get_right()->get_hash() == checked_hash)
                {
                    return { proofed[casted->get_left()->as_string()].first, it.second.first };
                }
            }
        }
        return {-1, -1};
    }

private:
    static inline std::vector<ptr_t> axioms{};
};