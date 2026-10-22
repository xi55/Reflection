#include "Reflection.h"

#include <algorithm>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class Person
{
public:
    Person() {}
    Person(const std::string &name, int age, double score, bool active)
        : name_(name), age_(age), score_(score), active_(active) {}

    std::string name_;
    int age_ = 0;
    double score_ = 0.0;
    bool active_ = false;
};

std::string escapeJson(const std::string &value)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const char character = value[index];
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << character;
            break;
        }
    }
    return output.str();
}

std::string numberToJson(double value)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << value;
    return output.str();
}

std::string valueToJson(const Evently::Any &value)
{
    using namespace Evently;

    if (value.empty())
    {
        return "null";
    }
    if (value.type() == typeid(std::string))
    {
        return "\"" + escapeJson(any_cast<std::string>(value)) + "\"";
    }
    if (value.type() == typeid(bool))
    {
        return any_cast<bool>(value) ? "true" : "false";
    }
    if (value.type() == typeid(char))
    {
        return "\"" + escapeJson(std::string(1, any_cast<char>(value))) + "\"";
    }
    if (value.type() == typeid(int))
    {
        return std::to_string(any_cast<int>(value));
    }
    if (value.type() == typeid(unsigned int))
    {
        return std::to_string(any_cast<unsigned int>(value));
    }
    if (value.type() == typeid(long long))
    {
        return std::to_string(any_cast<long long>(value));
    }
    if (value.type() == typeid(unsigned long long))
    {
        return std::to_string(any_cast<unsigned long long>(value));
    }
    if (value.type() == typeid(short))
    {
        return std::to_string(static_cast<int>(any_cast<short>(value)));
    }
    if (value.type() == typeid(unsigned short))
    {
        return std::to_string(static_cast<unsigned int>(
            any_cast<unsigned short>(value)));
    }
    if (value.type() == typeid(signed char))
    {
        return std::to_string(static_cast<int>(any_cast<signed char>(value)));
    }
    if (value.type() == typeid(unsigned char))
    {
        return std::to_string(static_cast<unsigned int>(
            any_cast<unsigned char>(value)));
    }
    if (value.type() == typeid(float))
    {
        return numberToJson(static_cast<double>(any_cast<float>(value)));
    }
    if (value.type() == typeid(double))
    {
        return numberToJson(any_cast<double>(value));
    }
    return "null";
}

std::string serializeObject(Evently::ReflectionRegistry &registry,
                            const std::string &className,
                            const void *instance)
{
    std::vector<Evently::FieldInfo> fields = registry.getFields(className);
    std::sort(fields.begin(), fields.end(),
              [](const Evently::FieldInfo &lhs,
                 const Evently::FieldInfo &rhs) {
                  return lhs.name < rhs.name;
              });

    std::unordered_map<std::string, Evently::Any> values =
        registry.getAllValues(className, instance);

    std::ostringstream output;
    output << "{";
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index != 0)
        {
            output << ",";
        }
        output << "\"" << escapeJson(fields[index].name) << "\":";
        std::unordered_map<std::string, Evently::Any>::const_iterator it =
            values.find(fields[index].name);
        if (it != values.end())
        {
            output << valueToJson(it->second);
        }
        else
        {
            output << "null";
        }
    }
    output << "}";
    return output.str();
}

int main()
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();

    registry.registerClassName<Person>("Person");
    registry.registerField<Person>("Person", "name", &Person::name_);
    registry.registerField<Person>("Person", "age", &Person::age_);
    registry.registerField<Person>("Person", "score", &Person::score_);
    registry.registerField<Person>("Person", "active", &Person::active_);
    registry.registerConstructor<Person>("Person");
    registry.freeze();

    Evently::ObjectHandle handle = registry.createInstance("Person");
    Person *person = handle.get<Person>();
    if (person == nullptr)
    {
        return 1;
    }
    person->name_ = "Ada";
    person->age_ = 30;
    person->score_ = 9.5;
    person->active_ = true;

    const std::string json = serializeObject(registry, "Person", person);
    std::cout << json << "\n";
    return json == "{\"active\":true,\"age\":30,\"name\":\"Ada\",\"score\":9.5}"
               ? 0
               : 2;
}
