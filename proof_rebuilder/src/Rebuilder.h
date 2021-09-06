#pragma once
#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Checker.h"
#include "expression.h"

class Rebuilder
{
public:
    struct Node;

    using ptr_t = std::unique_ptr<Base_expression>;
    using base_ptr_t = Base_expression *;
    using vec_t = std::vector<Node>;
    
    struct Node
    {
        std::string rule;
        base_ptr_t below;
        std::vector<Node> up;
        std::vector<base_ptr_t> hypothesis;

        Node(std::string r, base_ptr_t b, vec_t u, std::vector<base_ptr_t> h)
            : rule(std::move(r))
            , below(std::move(b))
            , up(std::move(u))
            , hypothesis(std::move(h))
        {}

        Node(Node&& other) noexcept
            : rule(std::move(other.rule))
            , below(std::move(other.below))
            , up(std::move(other.up))
            , hypothesis(std::move(other.hypothesis))
        {}
    };

    static void rebuild(const std::vector<ptr_t>& thesis_hypothesis, std::vector<std::pair<ptr_t, Checker::Why>>& statements)
    {
        std::vector<base_ptr_t> curr_hypot;
        curr_hypot.reserve(thesis_hypothesis.size());
        for (auto& ptr : thesis_hypothesis)
        { curr_hypot.push_back(ptr.get()); }

        Node result = rebuild(statements.back(), curr_hypot, statements);
        std::vector<std::pair<int, Node>> output;
        dfs(std::move(result), output, 0);
    }
private:

    static void print_hypothesis(const std::vector<base_ptr_t>& hypothesis)
    {
        if (!hypothesis.empty())
        {
            print_statement(hypothesis[0]);
            for (int i = 1; i < hypothesis.size(); i++)
            {
                std::cout << ',';
                print_statement(hypothesis[i]);
            }
        }
    }

    static void print_statement(base_ptr_t statement)
    {
        std::cout << statement->as_natural_string();
    }

    static void dfs(Node result, std::vector<std::pair<int, Node>>& output, int level)
    {
        for (int it = 0; it < result.up.size(); it++)
        {
            dfs(std::move(result.up[it]), output, level + 1);
        }
        std::cout << "[" << level << "] ";
        const Node& curr = result;
        print_hypothesis(curr.hypothesis);
        std::cout << "|-";
        print_statement(curr.below);
        std::cout << " [" << curr.rule << "]" << std::endl;;
    }

    static Node rebuild(std::pair<ptr_t, Checker::Why>& statement, const std::vector<base_ptr_t>& curr_hypot, std::vector<std::pair<ptr_t, Checker::Why>>& statements)
    {
        switch (statement.second.from)
        {
            case Checker::Why::From::AXIOM:
            {
                return rebuild_axiom(statement, curr_hypot);
            }
            case Checker::Why::From::HYPOTHESIS:
            {
                return rebuild_hypothesis(statement, curr_hypot);
            }
            case Checker::Why::From::MODUS_PONENS:
            {
                return rebuild_modus_ponens(statement, curr_hypot, statements);
            }
        }

        throw std::runtime_error("Rebuild error");
    }

    static Node rebuild_hypothesis(std::pair<ptr_t, Checker::Why>& statement, const std::vector<base_ptr_t>& curr_hypot)
    {
        return Node
        {
            "Ax",
            statement.first.get(),
            vec_t(),
            curr_hypot
        };
    }

    static Node rebuild_modus_ponens(std::pair<ptr_t, Checker::Why>& statement, const std::vector<base_ptr_t>& curr_hypot, std::vector<std::pair<ptr_t, Checker::Why>>& statements)
    {
        vec_t up;
        up.emplace_back(rebuild(statements[std::get<2>(statement.second.details).second], curr_hypot, statements));
        up.emplace_back(rebuild(statements[std::get<2>(statement.second.details).first], curr_hypot, statements));
        return Node
        {
            "E->",
            statement.first.get(),
            std::move(up),
            curr_hypot
        };
    }

    static Node rebuild_axiom(std::pair<ptr_t, Checker::Why>& statement, const std::vector<base_ptr_t>& curr_hypot)
    {
        switch (std::get<0>(statement.second.details))
        {
            // a -> b -> a
            case 0:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a = casted_below->get_left();
                base_ptr_t b = casted_right->get_left();

                hypothesis.push_back(a);
                hypothesis.push_back(b);
                Node zero_layer
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t zero_layer_vec; zero_layer_vec.emplace_back(std::move(zero_layer));
                Node first_layer
                {
                    "I->",
                    casted_below->get_right(),
                    std::move(zero_layer_vec),
                    hypothesis
                };

                hypothesis.pop_back();
                vec_t first_layer_vec; first_layer_vec.emplace_back(std::move(first_layer));
                return Node
                {
                    "I->",
                    below,
                    std::move(first_layer_vec),
                    hypothesis
                };
            }
            // (a -> b) -> (a -> b -> c) -> (a -> c)
            case 1:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a_im_b = casted_below->get_left();
                base_ptr_t a = static_cast<Operation*>(a_im_b)->get_left();
                base_ptr_t b = static_cast<Operation*>(a_im_b)->get_right();
                base_ptr_t a_im_b_im_c = casted_right->get_left();
                base_ptr_t b_im_c = static_cast<Operation*>(a_im_b_im_c)->get_right();
                base_ptr_t a_im_c = casted_right->get_right();
                base_ptr_t c = static_cast<Operation*>(a_im_c)->get_right();

                hypothesis.push_back(a_im_b);
                hypothesis.push_back(a_im_b_im_c);
                hypothesis.push_back(a);

                Node zero_layer1
                (
                    "Ax",
                    a_im_b_im_c,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer2
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer3
                (
                    "Ax",
                    a_im_b,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer4
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1; 
                prev1.emplace_back(std::move(zero_layer1));
                prev1.emplace_back(std::move(zero_layer2));
                Node first_layer1
                (
                    "E->",
                    b_im_c,
                    std::move(prev1),
                    hypothesis
                );

                vec_t prev2;
                prev2.emplace_back(std::move(zero_layer3));
                prev2.emplace_back(std::move(zero_layer4));
                Node first_layer2
                (
                    "E->",
                    b,
                    std::move(prev2),
                    hypothesis
                );

                vec_t prev3;
                prev3.emplace_back(std::move(first_layer1));
                prev3.emplace_back(std::move(first_layer2));
                Node second_layer
                (
                    "E->",
                    c,
                    std::move(prev3),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev4;
                prev4.emplace_back(std::move(second_layer));
                Node third_layer
                (
                    "I->",
                    a_im_c,
                    std::move(prev4),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev5;
                prev5.emplace_back(std::move(third_layer));
                Node fourth_layer
                (
                    "I->",
                    casted_below->get_right(),
                    std::move(prev5),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev6;
                prev6.emplace_back(std::move(fourth_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev6),
                    hypothesis
                );
            }
            // a -> b -> a & b
            case 2:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a = casted_below->get_left();
                base_ptr_t b = casted_right->get_left();
                base_ptr_t a_and_b = casted_right->get_right();

                hypothesis.push_back(a);
                hypothesis.push_back(b);

                Node upper_first
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );
                Node upper_second
                (
                    "Ax",
                    b,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(upper_first));
                prev1.emplace_back(std::move(upper_second));
                Node first_layer
                (
                    "I&",
                    a_and_b,
                    std::move(prev1),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                Node second_layer
                (
                    "I->",
                    casted_below->get_right(),
                    std::move(prev2),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev3;
                prev3.emplace_back(std::move(second_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev3),
                    hypothesis
                );
            }
            // a & b -> a
            case 3:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                base_ptr_t a_and_b = casted_below->get_left();
                base_ptr_t a = casted_below->get_right();

                hypothesis.push_back(a_and_b);
                Node zero_layer
                (
                    "Ax",
                    a_and_b,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer));
                Node first_layer
                (
                    "El&",
                    a,
                    std::move(prev1),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev2),
                    hypothesis
                );
            }
            // a & b -> b
            case 4:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                base_ptr_t a_and_b = casted_below->get_left();
                base_ptr_t b = casted_below->get_right();

                hypothesis.push_back(a_and_b);
                Node zero_layer
                (
                    "Ax",
                    a_and_b,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer));
                Node first_layer
                (
                    "Er&",
                    b,
                    std::move(prev1),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev2),
                    hypothesis
                );
            }
            // a -> a | b
            case 5:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                base_ptr_t a = casted_below->get_left();
                base_ptr_t a_or_b = casted_below->get_right();

                hypothesis.push_back(a);
                Node zero_layer
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer));
                Node first_layer
                (
                    "Il|",
                    a_or_b,
                    std::move(prev1),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev2),
                    hypothesis
                );
            }
            // b -> a | b
            case 6:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                base_ptr_t b = casted_below->get_left();
                base_ptr_t a_or_b = casted_below->get_right();

                hypothesis.push_back(b);
                Node zero_layer
                (
                    "Ax",
                    b,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer));
                Node first_layer
                (
                    "Ir|",
                    a_or_b,
                    std::move(prev1),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev2),
                    hypothesis
                );
            }
            // (a -> c) -> (b -> c) -> (a | b -> c)
            case 7:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a_im_c = casted_below->get_left();
                base_ptr_t b_im_c = casted_right->get_left();
                base_ptr_t a = static_cast<Operation*>(a_im_c)->get_left();
                base_ptr_t b = static_cast<Operation*>(b_im_c)->get_left();
                base_ptr_t c = static_cast<Operation*>(a_im_c)->get_right();
                base_ptr_t a_or_b_im_c = casted_right->get_right();
                base_ptr_t a_or_b = static_cast<Operation*>(a_or_b_im_c)->get_left();

                hypothesis.push_back(a_im_c);
                hypothesis.push_back(b_im_c);
                hypothesis.push_back(a_or_b);
                
                hypothesis.push_back(a);
                Node zero_layer1
                (
                    "Ax",
                    a_im_c,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer2
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );
                hypothesis.pop_back();

                hypothesis.push_back(b);
                Node zero_layer3
                (
                    "Ax",
                    b_im_c,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer4
                (
                    "Ax",
                    b,
                    vec_t(),
                    hypothesis
                );
                hypothesis.pop_back();

                hypothesis.push_back(a);
                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer1));
                prev1.emplace_back(std::move(zero_layer2));
                Node first_layer1
                (
                    "E->",
                    c,
                    std::move(prev1),
                    hypothesis
                );
                hypothesis.pop_back();

                hypothesis.push_back(b);
                vec_t prev2;
                prev2.emplace_back(std::move(zero_layer3));
                prev2.emplace_back(std::move(zero_layer4));
                Node first_layer2
                (
                    "E->",
                    c,
                    std::move(prev2),
                    hypothesis
                );
                hypothesis.pop_back();

                Node first_layer3
                (
                    "Ax",
                    a_or_b,
                    vec_t(),
                    hypothesis
                );

                vec_t prev3;
                prev3.emplace_back(std::move(first_layer1));
                prev3.emplace_back(std::move(first_layer2));
                prev3.emplace_back(std::move(first_layer3));
                Node second_layer
                (
                    "E|",
                    c,
                    std::move(prev3),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev4;
                prev4.emplace_back(std::move(second_layer));
                Node third_layer
                (
                    "I->",
                    a_or_b_im_c,
                    std::move(prev4),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev5;
                prev5.emplace_back(std::move(third_layer));
                Node fourth_layer
                (
                    "I->",
                    casted_below->get_right(),
                    std::move(prev5),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev6;
                prev6.emplace_back(std::move(fourth_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev6),
                    hypothesis
                );
            }
            // (a -> b) -> (a -> !b) -> !a
            case 8:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a_im_b = casted_below->get_left();
                base_ptr_t a = static_cast<Operation*>(a_im_b)->get_left();
                base_ptr_t b = static_cast<Operation*>(a_im_b)->get_right();
                base_ptr_t a_im_not_b = casted_right->get_left();
                base_ptr_t not_b = static_cast<Operation*>(a_im_not_b)->get_right();
                base_ptr_t not_a = casted_right->get_right();
                base_ptr_t lie_ptr = &lie;
                
                hypothesis.push_back(a_im_b);
                hypothesis.push_back(a_im_not_b);
                hypothesis.push_back(a);

                Node zero_layer1
                (
                    "Ax",
                    a_im_not_b,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer2
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                Node zero_layer3
                (
                    "Ax",
                    a_im_b,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer4
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer1));
                prev1.emplace_back(std::move(zero_layer2));
                Node first_layer1
                (
                    "E->",
                    not_b,
                    std::move(prev1),
                    hypothesis
                );

                vec_t prev2;
                prev2.emplace_back(std::move(zero_layer3));
                prev2.emplace_back(std::move(zero_layer4));
                Node first_layer2
                (
                    "E->",
                    b,
                    std::move(prev2),
                    hypothesis
                );

                vec_t prev3;
                prev3.emplace_back(std::move(first_layer1));
                prev3.emplace_back(std::move(first_layer2));
                Node second_layer
                (
                    "E->",
                    lie_ptr,
                    std::move(prev3),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev4;
                prev4.emplace_back(std::move(second_layer));
                Node third_layer
                (
                    "I->",
                    not_a,
                    std::move(prev4),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev5;
                prev5.emplace_back(std::move(third_layer));
                Node fourth_layer
                (
                    "I->",
                    casted_below->get_right(),
                    std::move(prev5),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev6;
                prev6.emplace_back(std::move(fourth_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev6),
                    hypothesis
                );
            }
            // a -> !a -> b
            case 9:
            {
                std::vector<base_ptr_t> hypothesis = curr_hypot;
                base_ptr_t below = statement.first.get();
                Operation* casted_below = static_cast<Operation*>(below);
                Operation* casted_right = static_cast<Operation*>(casted_below->get_right());
                base_ptr_t a = casted_below->get_left();
                base_ptr_t not_a = casted_right->get_left();
                base_ptr_t b = casted_right->get_right();
                base_ptr_t lie_ptr = &lie;

                hypothesis.push_back(a);
                hypothesis.push_back(not_a);
                Node zero_layer_1
                (
                    "Ax",
                    not_a,
                    vec_t(),
                    hypothesis
                );
                Node zero_layer_2
                (
                    "Ax",
                    a,
                    vec_t(),
                    hypothesis
                );

                vec_t prev1;
                prev1.emplace_back(std::move(zero_layer_1));
                prev1.emplace_back(std::move(zero_layer_2));
                Node first_layer
                (
                    "E->",
                    lie_ptr,
                    std::move(prev1),
                    hypothesis
                );

                vec_t prev2;
                prev2.emplace_back(std::move(first_layer));
                Node second_layer
                (
                    "E_|_",
                    b,
                    std::move(prev2),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev3;
                prev3.emplace_back(std::move(second_layer));
                Node third_layer
                (
                    "I->",
                    casted_below->get_right(),
                    std::move(prev3),
                    hypothesis
                );

                hypothesis.pop_back();
                vec_t prev4;
                prev4.emplace_back(std::move(third_layer));
                return Node
                (
                    "I->",
                    below,
                    std::move(prev4),
                    hypothesis
                );
            }
        }

        throw std::runtime_error("Unknown axiom");
    }
};