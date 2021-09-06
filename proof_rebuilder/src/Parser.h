#pragma once
#include <exception>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "expression.h"

class Parser
{
    using ptr_t = std::unique_ptr<Base_expression>;
public:
    static std::pair<std::vector<ptr_t>, ptr_t> parse_thesis(const std::string& string)
    {
        std::vector<ptr_t> hypothesis;
        int turnstile_pos = string.find("|-");
        if (turnstile_pos == std::string::npos)
        { throw std::runtime_error("Wrong string format"); }
        
        hypothesis = parse_hypos(string.substr(0, turnstile_pos));
        ptr_t statement = parse(string.substr(turnstile_pos + 2));
        return std::make_pair(std::move(hypothesis), std::move(statement));
    }

    static ptr_t parse(const std::string& string)
    {
        std::string processed = prepare_string(string);
        return parse_expression(processed);
    }
private:
    static std::vector<ptr_t> parse_hypos(const std::string& string)
    {
        std::vector<ptr_t> hypothesis;
        int prev_pos = -1;
        int cnt = 0;
        for (int pos = 0; pos < string.length(); pos++, cnt++)
        {
            if (string[pos] == ',')
            {
                ptr_t hypo = parse(string.substr(prev_pos + 1, cnt));
                cnt = -1;
                prev_pos = pos;
                hypothesis.emplace_back(hypo.release());
            }
        }
        if (string.length() != 0)
        {
            ptr_t hypo = parse(string.substr(prev_pos + 1));
            hypothesis.emplace_back(hypo.release());
        }
        return hypothesis;
    }

    static std::string prepare_string(const std::string& string)
    {
        std::string copy;
        copy.reserve(string.length());
        int balance = 0;
        int prev_pos = -1;
        for (int i = 0; i < string.length(); i++)
        {
            if (string[i] == '(')
            {
                if (balance == 0)
                { prev_pos = i; }
                balance++;
            }
            else if (string[i] == ')')
            {
                if (balance <= 0)
                { throw std::runtime_error("Wrong string format"); }

                balance--;
                if (balance == 0)
                {
                    int start = prev_pos; prev_pos = -1;
                    ptr_t expr = parse(string.substr(start + 1, i - start - 1));
                    std::string placeholder = "#" + std::to_string(placeholder_id++) + "#";
                    placeholders.emplace(std::make_pair(placeholder, std::move(expr)));
                    copy += placeholder;
                }
            }
            else if (balance == 0 && !std::isspace(static_cast<unsigned char>(string[i])))
            { copy += string[i]; }
        }
        return copy;
    }

    static ptr_t parse_expression(const std::string& string)
    {
        int impl_pos = string.find("->");

        if (impl_pos == std::string::npos)
        {
            return parse_disjunction(string);
        }
        else
        {
            ptr_t left = parse_expression(string.substr(0, impl_pos));
            ptr_t right = parse_expression(string.substr(impl_pos + 2));
            Base_expression* res = new Operation("->", left, right);
            return ptr_t(res);
        }
    }

    static ptr_t parse_disjunction(const std::string& string)
    {
        int dis_pos = string.rfind('|');
        if (dis_pos == std::string::npos)
        {
            return parse_conjunction(string);
        }
        else
        {
            ptr_t left = parse_expression(string.substr(0, dis_pos));
            ptr_t right = parse_expression(string.substr(dis_pos + 1));
            Base_expression* res = new Operation("|", left, right);
            return ptr_t(res);
        }
    }

    static ptr_t parse_conjunction(const std::string& string)
    {
        int con_pos = string.rfind("&");
        if (con_pos == std::string::npos)
        {
            return parse_negation(string);
        }
        else
        {
            ptr_t left = parse_expression(string.substr(0, con_pos));
            ptr_t right = parse_expression(string.substr(con_pos + 1));
            Base_expression* res = new Operation("&", left, right);
            return ptr_t(res);
        }
    }

    static ptr_t parse_negation(const std::string& string)
    {
        int start_pos = skip_ws(string);
        if (start_pos == std::string::npos)
        { throw std::runtime_error("Error in string format"); }

        int end_pos = rskip_ws(string);
        if (end_pos == std::string::npos)
        { throw std::runtime_error("Wrong string format"); }

        if (string[start_pos] == '!')
        {
            ptr_t neg = parse_expression(string.substr(start_pos + 1));
            Base_expression* res = new Negate(neg);
            return ptr_t(res);
        }
        else if (string[start_pos] == '(' && string[end_pos] == ')')
        {
            ptr_t expr = parse_expression(string.substr(start_pos + 1, end_pos - start_pos - 1));
            return expr;
        }
        else if (is_letter(string[start_pos]))
        {
            ptr_t var = parse_variable(string.substr(start_pos, end_pos - start_pos + 1));
            return var;
        }
        else if (string[start_pos] == '#' && string[end_pos] == '#')
        {
            return ptr_t(placeholders[string.substr(start_pos, end_pos - start_pos + 1)].release());
        }
        else
        { throw std::runtime_error("Wrong string format"); }
    }

    static ptr_t parse_variable(const std::string& string)
    {
        Base_expression* res = new Variable(string);
        return ptr_t(res);
    }

    static int skip_ws(const std::string& string)
    {
        for (int i = 0; i < string.length(); i++)
        {
            if (!std::isspace(static_cast<unsigned char>(string[i])))
            { return i; }
        }
        return std::string::npos;
    }

    static int rskip_ws(const std::string& string)
    {
        for (int i = string.length() - 1; i >= 0; i--)
        {
            if (!std::isspace(static_cast<unsigned char>(string[i])))
            {
                return i;
            }
        }
        return std::string::npos;
    }

    static bool is_letter(char c)
    {
        return std::islower(static_cast<unsigned char>(c)) || std::isupper(static_cast<unsigned char>(c));
    }

private:
    static std::unordered_map<std::string, ptr_t> placeholders;
    static int placeholder_id;
};

int Parser::placeholder_id = 0;
std::unordered_map<std::string, Parser::ptr_t> Parser::placeholders;