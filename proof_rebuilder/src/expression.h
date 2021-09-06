#pragma once
#include <memory>
#include <string>
#include <unordered_map>

class Base_expression
{
public:
    std::string as_string() const noexcept
    {
        return string;
    }
    std::string as_natural_string() const noexcept
    {
        return natural_string;
    }
    virtual std::string get_sign() const noexcept = 0;

    std::size_t get_hash() const noexcept
    { return hash; }

    virtual ~Base_expression(){};
    virtual bool is_equal(const Base_expression* other, std::unordered_map<std::string, const Base_expression*>& translator) const noexcept = 0;
protected:
    static bool is_axiom_var(const std::string& name)
    {
        return islower(static_cast<unsigned char>(name.front()));
    }
protected:
    std::size_t hash = 0;
    std::string string = "";
    std::string natural_string = "";
};

class Variable : public Base_expression
{
public:
    Variable(const std::string& str)
    {
        natural_string = string = str;
        hash = std::hash<std::string>{}(string);
    }

    virtual std::string get_sign() const noexcept /*override*/
    {
        return "var";
    }

    virtual bool is_equal(const Base_expression* other, std::unordered_map<std::string, const Base_expression*>& translator) const noexcept /*override*/
    {
        if (get_sign() != other->get_sign())
        {
            return false;
        }
        else
        {
            std::string oth_name = other->as_string();
            if (is_axiom_var(oth_name))
            {
                if (translator[oth_name] == nullptr)
                {
                    translator[oth_name] = this;
                    return true;
                }
                else
                {
                    return is_equal(translator[oth_name], translator);
                }
            }
            else
            {
                return hash == other->get_hash();
            }
        }
    }
};

static inline Variable lie("_|_");

class Negate : public Base_expression
{
public:
    Negate(Base_expression* val)
        : arg(val)
    {
        string = "(!" + arg->as_string() + ")";
        natural_string = "(" + arg->as_natural_string() + "->_|_)";
        hash = std::hash<std::string>{}(as_string());
    }

    Negate(std::unique_ptr<Base_expression>& val)
        : arg(std::move(val))
    {
        string = "(!" + arg->as_string() + ")";
        natural_string = "(" + arg->as_natural_string() + "->_|_)";
        hash = std::hash<std::string>{}(as_string());
    }

    virtual std::string get_sign() const noexcept /*override*/
    {
        return "!";
    }

    virtual bool is_equal(const Base_expression* other, std::unordered_map<std::string, const Base_expression*>& translator) const noexcept /*override*/
    {
        if (get_sign() != other->get_sign())
        {
            std::string oth_name = other->as_string();
            if (other->get_sign() == "var" && is_axiom_var(oth_name))
            {
                if (translator[oth_name] == nullptr)
                {
                    translator[oth_name] = this;
                    return true;
                }
                else
                {
                    return is_equal(translator[oth_name], translator);
                }
            }
            return false;
        }
        else
        {
            return arg->is_equal(static_cast<const Negate*>(other)->arg.get(), translator);
        }
    }
private:
    std::unique_ptr<Base_expression> arg;
};

class Operation : public Base_expression
{
public:
    Operation(const std::string& type, Base_expression* left_val, Base_expression* right_val)
        : op(type)
        , left(left_val)
        , right(right_val)
    {
        string = "(" + op + "," + left->as_string() + "," + right->as_string() + ")";
        std::string left_s = left->as_natural_string();
        std::string right_s = right->as_natural_string();
        natural_string = "(" + left_s + op + right_s + ")";
        hash = std::hash<std::string>{}(as_string());
    }

    Operation(const std::string& type, std::unique_ptr<Base_expression>& left_val, std::unique_ptr<Base_expression>& right_val)
        : op(type)
        , left(std::move(left_val))
        , right(std::move(right_val))
    {
        string = "(" + op + "," + left->as_string() + "," + right->as_string() + ")";
        std::string left_s = left->as_natural_string();
        std::string right_s = right->as_natural_string();
        natural_string = "(" + left_s + op + right_s + ")";
        hash = std::hash<std::string>{}(as_string());
    }

    virtual std::string get_sign() const noexcept /*override*/
    {
        return op;
    }

    Base_expression* get_left() const noexcept
    {
        return left.get();
    }
    Base_expression* get_right() const noexcept
    {
        return right.get();
    }

    std::pair<Base_expression*, Base_expression*> get() const noexcept
    {
        return std::make_pair(get_left(), get_right());
    }

    virtual bool is_equal(const Base_expression* other, std::unordered_map<std::string, const Base_expression*>& translator) const noexcept /*override*/
    {
        if (get_sign() != other->get_sign())
        {
            if (other->get_sign() == "var" && is_axiom_var(other->as_string()))
            {
                std::string oth_name = other->as_string();
                if (translator[oth_name] == nullptr)
                {
                    translator[oth_name] = this;
                    return true;
                }
                else
                {
                    return is_equal(translator[oth_name], translator);
                }
            }
            return false;
        }
        else
        {
            const Operation* casted = static_cast<const Operation*>(other);
            return left->is_equal(casted->get_left(), translator) && right->is_equal(casted->get_right(), translator);
        }
    }
private:
    std::string op;
    std::unique_ptr<Base_expression> left, right;
};