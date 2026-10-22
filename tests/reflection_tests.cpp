#include "Reflection.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace Evently;

namespace
{
    int checks = 0;
    int failures = 0;

    void check(bool condition, const char *expression, const char *file, int line)
    {
        ++checks;
        if (!condition)
        {
            ++failures;
            std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        }
    }

    template <typename Exception, typename Function>
    void checkThrow(Function function, const char *expression, const char *file, int line)
    {
        ++checks;
        try
        {
            function();
        }
        catch (const Exception &)
        {
            return;
        }
        catch (...)
        {
            ++failures;
            std::cerr << file << ":" << line
                      << ": wrong exception type for: " << expression << "\n";
            return;
        }
        ++failures;
        std::cerr << file << ":" << line
                  << ": expected exception for: " << expression << "\n";
    }

#define CHECK(expression) \
    check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define CHECK_THROW(expression, exception_type) \
    checkThrow<exception_type>([&]() { expression; }, #expression, __FILE__, __LINE__)

    class TestObject
    {
    public:
        TestObject() {}
        TestObject(int value, const std::string &name) : value_(value), name_(name) {}

        void setValue(int value) { value_ = value; }
        int getValue() const { return value_; }
        int getValueMutable() { return value_; }
        int add(int a, int b) { return a + b; }
        int add3(int a, int b, int c) { return a + b + c; }
        void setName(const std::string &name) { name_ = name; }
        std::string greet(const std::string &prefix) const { return prefix + name_; }
        void reset() { value_ = 0; }
        void constNoop() const {}

        int value_ = 0;
        std::string name_;
        double score_ = 0.0;
        bool flag_ = false;
        char letter_ = '\0';
        long long big_ = 0;
        unsigned int count_ = 0;
        short small_ = 0;
        float ratio_ = 0.0f;
        const int constInt_ = 42;
        const std::string constString_ = "const";
    };

    class NoDefault
    {
    public:
        explicit NoDefault(int value) : value_(value) {}
        int value_;
    };

    class Lifetime
    {
    public:
        Lifetime() { ++alive_; }
        ~Lifetime() { --alive_; }
        static int alive_;
    };

    int Lifetime::alive_ = 0;

    class Unregistered
    {
    public:
        int value_ = 0;
    };

    void registerTypes()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        registry.registerClassName<TestObject>("TestObject");

        registry.registerField<TestObject>("TestObject", "value", &TestObject::value_);
        registry.registerField<TestObject>("TestObject", "name", &TestObject::name_);
        registry.registerField<TestObject>("TestObject", "score", &TestObject::score_);
        registry.registerField<TestObject>("TestObject", "flag", &TestObject::flag_);
        registry.registerField<TestObject>("TestObject", "letter", &TestObject::letter_);
        registry.registerField<TestObject>("TestObject", "big", &TestObject::big_);
        registry.registerField<TestObject>("TestObject", "count", &TestObject::count_);
        registry.registerField<TestObject>("TestObject", "small", &TestObject::small_);
        registry.registerField<TestObject>("TestObject", "ratio", &TestObject::ratio_);
        registry.registerField<TestObject>("constInt", &TestObject::constInt_);
        registry.registerField<TestObject>("constString", &TestObject::constString_);

        registry.registerMethod<TestObject, void, int>(
            "TestObject", "setValue", &TestObject::setValue);
        registry.registerMethod<TestObject, int>(
            "TestObject", "getValue", &TestObject::getValue);
        registry.registerMethod<TestObject, int>(
            "TestObject", "getValueMutable", &TestObject::getValueMutable);
        registry.registerMethod<TestObject, int, int, int>(
            "TestObject", "add", &TestObject::add);
        registry.registerMethod<TestObject, int, int, int, int>(
            "TestObject", "add3", &TestObject::add3);
        registry.registerMethod<TestObject, void, const std::string &>(
            "TestObject", "setName", &TestObject::setName);
        registry.registerMethod<TestObject, std::string, const std::string &>(
            "TestObject", "greet", &TestObject::greet);
        registry.registerMethod<TestObject, void>(
            "TestObject", "reset", &TestObject::reset);
        registry.registerMethod<TestObject, void>(
            "TestObject", "constNoop", &TestObject::constNoop);

        registry.registerClass<TestObject>("TestObject");
        registry.registerClass<TestObject, int, std::string>(
            "TestObjectWithArgs", 7, std::string("seven"));
        registry.registerClass<NoDefault, int>("NoDefaultWithArgs", 9);
        registry.registerClass<Lifetime>("Lifetime");
    }

    void testAny()
    {
        Any empty;
        CHECK(empty.empty());
        CHECK(empty.type() == typeid(void));
        CHECK_THROW((any_cast<int>(empty)), bad_any_cast);

        Any integer(42);
        CHECK(!integer.empty());
        CHECK(integer.type() == typeid(int));
        CHECK(any_cast<int>(integer) == 42);
        CHECK(any_cast<const int>(integer) == 42);

        const Any constInteger(7);
        CHECK(any_cast<int>(constInteger) == 7);
        CHECK(any_cast<const int &>(constInteger) == 7);

        const int constValue = 9;
        Any storedConst(constValue);
        CHECK(storedConst.type() == typeid(int));
        CHECK(any_cast<int>(storedConst) == 9);
        CHECK(any_cast<const int>(storedConst) == 9);

        Any text(std::string("hello"));
        std::string *textPointer = any_cast<std::string>(&text);
        CHECK(textPointer != nullptr);
        if (textPointer)
        {
            CHECK(*textPointer == "hello");
        }
        CHECK(&any_cast<const std::string &>(text) == textPointer);

        any_cast<std::string &>(text) = "world";
        CHECK(any_cast<std::string>(text) == "world");
        CHECK(any_cast<int>(&text) == nullptr);
        CHECK_THROW((any_cast<int>(text)), bad_any_cast);

        const Any constText(std::string("const"));
        const std::string *constTextPointer = any_cast<std::string>(&constText);
        CHECK(constTextPointer != nullptr);
        if (constTextPointer)
        {
            CHECK(*constTextPointer == "const");
        }
        CHECK(any_cast<const std::string &>(constText) == "const");
        CHECK(any_cast<int>(&constText) == nullptr);

        Any copy = text;
        copy = std::string("copy");
        CHECK(any_cast<std::string>(text) == "world");
        CHECK(any_cast<std::string>(copy) == "copy");

        Any moved(std::move(copy));
        CHECK(any_cast<std::string>(moved) == "copy");
        CHECK(copy.empty());

        Any target(1);
        target = std::move(moved);
        CHECK(any_cast<std::string>(target) == "copy");
        CHECK(moved.empty());

        Any assigned;
        assigned = 5;
        CHECK(any_cast<int>(assigned) == 5);
    }

    template <typename T>
    void checkField(ReflectionRegistry &registry, TestObject &object,
                    const std::string &name, const T &value, const T &expected)
    {
        PropertySetterBase *setter = registry.getSetter("TestObject", name);
        CHECK(setter != nullptr);
        if (!setter)
        {
            return;
        }
        setter->set(&object, Any(value));
        Any stored = registry.getValues("TestObject", name, &object);
        CHECK(any_cast<T>(stored) == expected);
    }

    void testFields()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object;

        checkField<int>(registry, object, "value", 123, 123);
        checkField<std::string>(registry, object, "name", std::string("Alice"),
                                std::string("Alice"));
        checkField<double>(registry, object, "score", 1.5, 1.5);
        checkField<bool>(registry, object, "flag", true, true);
        checkField<char>(registry, object, "letter", 'Z', 'Z');
        checkField<long long>(registry, object, "big", 1234567890123LL,
                              1234567890123LL);
        checkField<unsigned int>(registry, object, "count", 42U, 42U);
        checkField<short>(registry, object, "small", static_cast<short>(-7),
                          static_cast<short>(-7));
        checkField<float>(registry, object, "ratio", 2.5f, 2.5f);

        CHECK(registry.getSetter("TestObject", "constInt") == nullptr);
        CHECK(registry.getSetter("TestObject", "constString") == nullptr);

        Any constInt = registry.getValues("TestObject", "constInt", &object);
        CHECK(any_cast<int>(constInt) == 42);
        CHECK(any_cast<const int>(constInt) == 42);

        Any constString = registry.getValues("TestObject", "constString", &object);
        CHECK(any_cast<std::string>(constString) == "const");
        CHECK(any_cast<const std::string &>(constString) == "const");

        std::unordered_map<std::string, Any> values =
            registry.getAllValues("TestObject", &object);
        CHECK(values.size() == 11u);
        CHECK(values.find("value") != values.end());
        CHECK(values.find("constInt") != values.end());
        CHECK(any_cast<int>(values["value"]) == 123);
        CHECK(any_cast<std::string>(values["name"]) == "Alice");
        CHECK(any_cast<int>(values["constInt"]) == 42);

        CHECK(registry.getSetter("TestObject", "missing") == nullptr);
        CHECK(registry.getValues("TestObject", "missing", &object).empty());

        PropertySetterBase *setter = registry.getSetter("TestObject", "value");
        CHECK(setter != nullptr);
        if (setter)
        {
            CHECK_THROW((setter->get(nullptr)), std::invalid_argument);
            CHECK_THROW((setter->set(nullptr, Any(1))), std::invalid_argument);
            CHECK_THROW((setter->set(&object, Any(std::string("bad")))),
                        std::invalid_argument);
        }

        CHECK_THROW((registry.getValues("TestObject", "value", nullptr)),
                    std::invalid_argument);
        CHECK_THROW((registry.getAllValues("TestObject", nullptr)),
                    std::invalid_argument);
    }

    void testMethods()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object(3, "Bob");

        registry.invokeMethod("TestObject", "setValue", &object, {Any(10)});
        CHECK(object.value_ == 10);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "getValue", &object, {})) == 10);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "getValueMutable", &object, {})) == 10);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "add", &object, {Any(2), Any(3)})) == 5);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "add3", &object,
                  {Any(1), Any(2), Any(3)})) == 6);

        registry.invokeMethod("TestObject", "setName", &object,
                              {Any(std::string("Alice"))});
        CHECK(object.name_ == "Alice");
        CHECK(any_cast<std::string>(registry.invokeMethod(
                  "TestObject", "greet", &object,
                  {Any(std::string("Hi "))})) == "Hi Alice");

        registry.invokeMethod("TestObject", "reset", &object, {});
        CHECK(object.value_ == 0);
        registry.invokeMethod("TestObject", "constNoop", &object, {});

        CHECK_THROW((registry.invokeMethod("TestObject", "setValue", &object, {})),
                    std::invalid_argument);
        CHECK_THROW((registry.invokeMethod("TestObject", "add", &object, {Any(1)})),
                    std::invalid_argument);
        CHECK_THROW((registry.invokeMethod("TestObject", "add3", &object,
                                           {Any(1), Any(2)})),
                    std::invalid_argument);
        CHECK_THROW((registry.invokeMethod(
                        "TestObject", "setValue", &object,
                        {Any(std::string("bad"))})),
                    bad_any_cast);
        CHECK_THROW((registry.invokeMethod("TestObject", "missing", &object, {})),
                    std::runtime_error);
        CHECK_THROW((registry.invokeMethod("TestObject", "getValue", nullptr, {})),
                    std::runtime_error);
        CHECK_THROW((registry.invokeMethod("Missing", "getValue", &object, {})),
                    std::runtime_error);

        std::set<std::string> names = registry.getMethodNames("TestObject");
        CHECK(names.count("setValue") == 1u);
        CHECK(names.count("getValue") == 1u);
        CHECK(names.count("getValueMutable") == 1u);
        CHECK(names.count("add") == 1u);
        CHECK(names.count("add3") == 1u);
        CHECK(names.count("setName") == 1u);
        CHECK(names.count("greet") == 1u);
        CHECK(names.count("reset") == 1u);
        CHECK(names.count("constNoop") == 1u);
        CHECK(registry.getMethodNames("Missing").empty());
        CHECK(registry.getClassName<TestObject>() == "TestObject");
        CHECK(registry.getClassName<Unregistered>() == "unregistered");
    }

    void testFactory()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        std::unique_ptr<void, void (*)(void *)> object =
            registry.createInstance("TestObject");
        CHECK(object.get() != nullptr);
        if (object)
        {
            TestObject *value = static_cast<TestObject *>(object.get());
            CHECK(value->value_ == 0);
        }

        std::unique_ptr<void, void (*)(void *)> withArgs =
            registry.createInstance("TestObjectWithArgs");
        CHECK(withArgs.get() != nullptr);
        if (withArgs)
        {
            TestObject *value = static_cast<TestObject *>(withArgs.get());
            CHECK(value->value_ == 7);
            CHECK(value->name_ == "seven");
        }

        std::unique_ptr<void, void (*)(void *)> noDefault =
            registry.createInstance("NoDefaultWithArgs");
        CHECK(noDefault.get() != nullptr);
        if (noDefault)
        {
            NoDefault *value = static_cast<NoDefault *>(noDefault.get());
            CHECK(value->value_ == 9);
        }

        std::unique_ptr<void, void (*)(void *)> missing =
            registry.createInstance("Missing");
        CHECK(missing.get() == nullptr);

        Lifetime::alive_ = 0;
        {
            std::unique_ptr<void, void (*)(void *)> lifetime =
                registry.createInstance("Lifetime");
            CHECK(lifetime.get() != nullptr);
            CHECK(Lifetime::alive_ == 1);
        }
        CHECK(Lifetime::alive_ == 0);
    }

    void testRegistrationErrors()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        CHECK_THROW((registry.registerClassName<TestObject>("")),
                    std::invalid_argument);
        CHECK(registry.getClassName<TestObject>() == "TestObject");

        CHECK_THROW((registry.registerField<TestObject>(
                        "", "value", &TestObject::value_)),
                    std::invalid_argument);
        CHECK_THROW((registry.registerField<TestObject>(
                        "TestObject", "", &TestObject::value_)),
                    std::invalid_argument);
        CHECK_THROW((registry.registerField<TestObject>(
                        "", &TestObject::value_)),
                    std::invalid_argument);
        CHECK_THROW((registry.registerField<Unregistered>(
                        "value", &Unregistered::value_)),
                    std::runtime_error);

        CHECK_THROW((registry.registerMethod<TestObject, void>(
                        "TestObject", "", &TestObject::reset)),
                    std::invalid_argument);
        CHECK_THROW((registry.registerMethod<TestObject, void>(
                        "", "reset", &TestObject::reset)),
                    std::invalid_argument);
        CHECK_THROW((registry.registerClass<TestObject>("")),
                    std::invalid_argument);
        CHECK_THROW((registry.registerClass<NoDefault, int>("", 1)),
                    std::invalid_argument);
    }

    struct TestCase
    {
        const char *name;
        void (*function)();
    };

    const TestCase tests[] = {
        {"Any", testAny},
        {"Fields", testFields},
        {"Methods", testMethods},
        {"Factory", testFactory},
        {"RegistrationErrors", testRegistrationErrors}};
}

int main(int argc, char **argv)
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::string filter;
    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        if (argument == "--test" && index + 1 < argc)
        {
            filter = argv[++index];
        }
    }

    try
    {
        registerTypes();
    }
    catch (const std::exception &error)
    {
        std::cerr << "registration failed: " << error.what() << "\n";
        return 1;
    }

    int executed = 0;
    const std::size_t testCount = sizeof(tests) / sizeof(tests[0]);
    for (std::size_t index = 0; index < testCount; ++index)
    {
        if (!filter.empty() && filter != tests[index].name)
        {
            continue;
        }

        ++executed;
        std::cout << "[ RUN      ] " << tests[index].name << "\n";
        const int failuresBefore = failures;
        try
        {
            tests[index].function();
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::cerr << "uncaught exception: " << error.what() << "\n";
        }
        catch (...)
        {
            ++failures;
            std::cerr << "uncaught non-standard exception\n";
        }

        if (failures == failuresBefore)
        {
            std::cout << "[       OK ] " << tests[index].name << "\n";
        }
        else
        {
            std::cout << "[  FAILED  ] " << tests[index].name << "\n";
        }
    }

    if (executed == 0)
    {
        std::cerr << "no test matched: " << filter << "\n";
        return 2;
    }

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
