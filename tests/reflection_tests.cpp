#include "Reflection.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <typeindex>
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

    template <typename Function>
    void checkError(ReflectionErrorCode expected, Function function,
                    const char *expression, const char *file, int line)
    {
        ++checks;
        try
        {
            function();
        }
        catch (const ReflectionError &error)
        {
            if (error.code() != expected)
            {
                ++failures;
                std::cerr << file << ":" << line
                          << ": wrong error code for: " << expression << "\n";
            }
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
                  << ": expected ReflectionError for: " << expression << "\n";
    }

#define CHECK(expression) \
    check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define CHECK_THROW(expression, exception_type) \
    checkThrow<exception_type>([&]() { expression; }, #expression, __FILE__, __LINE__)

#define CHECK_ERROR(error_code, expression) \
    checkError(error_code, [&]() { expression; }, #expression, __FILE__, __LINE__)

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

        int overloaded(int value) { return value + 1; }
        std::string overloaded(const std::string &value) const { return value + "!"; }
        int qualifier() { return 1; }
        int qualifier() const { return 2; }

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

    typedef int (TestObject::*OverloadedInt)(int);
    typedef std::string (TestObject::*OverloadedString)(const std::string &) const;
    typedef int (TestObject::*QualifierMutable)();
    typedef int (TestObject::*QualifierConst)() const;

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
        registry.registerMethod<TestObject, int, int>(
            "TestObject", "overloaded", static_cast<OverloadedInt>(&TestObject::overloaded));
        registry.registerMethod<TestObject, std::string, const std::string &>(
            "TestObject", "overloaded", static_cast<OverloadedString>(&TestObject::overloaded));
        registry.registerMethod<TestObject, int>(
            "TestObject", "qualifier", static_cast<QualifierMutable>(&TestObject::qualifier));
        registry.registerMethod<TestObject, int>(
            "TestObject", "qualifier", static_cast<QualifierConst>(&TestObject::qualifier));

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

        CHECK(registry.isFieldWritable("TestObject", "value"));
        CHECK(!registry.isFieldWritable("TestObject", "constInt"));
        CHECK(!registry.isFieldWritable("TestObject", "constString"));
        CHECK_ERROR(ReflectionErrorCode::FieldNotWritable,
                    (registry.getSetter("TestObject", "constInt")));
        CHECK_ERROR(ReflectionErrorCode::FieldNotWritable,
                    (registry.getSetter("TestObject", "constString")));

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

        CHECK(!registry.hasField("TestObject", "missing"));
        CHECK_ERROR(ReflectionErrorCode::FieldNotFound,
                    (registry.getValues("TestObject", "missing", &object)));
        CHECK_ERROR(ReflectionErrorCode::FieldNotFound,
                    (registry.getSetter("TestObject", "missing")));

        PropertySetterBase *setter = registry.getSetter("TestObject", "value");
        CHECK(setter != nullptr);
        if (setter)
        {
            CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                        (setter->get(nullptr)));
            CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                        (setter->set(nullptr, Any(1))));
            CHECK_ERROR(ReflectionErrorCode::FieldTypeMismatch,
                        (setter->set(&object, Any(std::string("bad")))));
        }

        CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                    (registry.getValues("TestObject", "value", nullptr)));
        CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                    (registry.getAllValues("TestObject", nullptr)));
    }

    void testMethods()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object(3, "Bob");
        const TestObject constObject(4, "Const");

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

        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "overloaded", &object, {Any(5)})) == 6);
        CHECK(any_cast<std::string>(registry.invokeMethod(
                  "TestObject", "overloaded", &object,
                  {Any(std::string("hello"))})) == "hello!");
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "qualifier", &object, {})) == 1);

        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "getValue", &constObject, {})) == 4);
        CHECK(any_cast<std::string>(registry.invokeMethod(
                  "TestObject", "greet", &constObject,
                  {Any(std::string("Hi "))})) == "Hi Const");
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "qualifier", &constObject, {})) == 2);
        registry.invokeMethod("TestObject", "constNoop", &constObject, {});
        CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                    (registry.invokeMethod("TestObject", "setValue", &constObject,
                                           {Any(1)})));

        CHECK_ERROR(ReflectionErrorCode::ArgumentCountMismatch,
                    (registry.invokeMethod("TestObject", "setValue", &object, {})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentCountMismatch,
                    (registry.invokeMethod("TestObject", "add", &object, {Any(1)})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentCountMismatch,
                    (registry.invokeMethod("TestObject", "overloaded", &object, {})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentTypeMismatch,
                    (registry.invokeMethod("TestObject", "setValue", &object,
                                           {Any(std::string("bad"))})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentTypeMismatch,
                    (registry.invokeMethod("TestObject", "overloaded", &object,
                                           {Any(1.5)})));
        CHECK_ERROR(ReflectionErrorCode::MethodNotFound,
                    (registry.invokeMethod("TestObject", "missing", &object, {})));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.invokeMethod("Missing", "getValue", &object, {})));
        CHECK_ERROR(ReflectionErrorCode::InvalidInstance,
                    (registry.invokeMethod("TestObject", "getValue", nullptr, {})));

        std::set<std::string> names = registry.getMethodNames("TestObject");
        CHECK(names.size() == 11u);
        CHECK(names.count("setValue") == 1u);
        CHECK(names.count("overloaded") == 1u);
        CHECK(names.count("qualifier") == 1u);
        CHECK(registry.hasMethod("TestObject", "add"));
        CHECK(!registry.hasMethod("TestObject", "missing"));
        CHECK(registry.getClassName<TestObject>() == "TestObject");
        CHECK(registry.hasClassName<TestObject>());
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getMethodNames("Missing")));
    }

    void testFactory()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        ObjectHandle object = registry.createInstance("TestObject");
        CHECK(static_cast<bool>(object));
        CHECK(!object.empty());
        CHECK(object.type() == typeid(TestObject));
        TestObject *value = object.get<TestObject>();
        CHECK(value != nullptr);
        if (value)
        {
            CHECK(value->value_ == 0);
        }
        CHECK(object.get<NoDefault>() == nullptr);

        ObjectHandle copy = object;
        CHECK(copy.get<TestObject>() == value);
        ObjectHandle moved(std::move(copy));
        CHECK(copy.empty());
        CHECK(moved.get<TestObject>() == value);

        ObjectHandle empty;
        CHECK(!empty);
        CHECK(empty.type() == typeid(void));

        ObjectHandle direct = ObjectHandle::create<NoDefault>(11);
        NoDefault *directValue = direct.get<NoDefault>();
        CHECK(directValue != nullptr);
        if (directValue)
        {
            CHECK(directValue->value_ == 11);
        }

        ObjectHandle withArgs = registry.createInstance("TestObjectWithArgs");
        TestObject *withArgsValue = withArgs.get<TestObject>();
        CHECK(withArgsValue != nullptr);
        if (withArgsValue)
        {
            CHECK(withArgsValue->value_ == 7);
            CHECK(withArgsValue->name_ == "seven");
        }

        ObjectHandle noDefault = registry.createInstance("NoDefaultWithArgs");
        NoDefault *noDefaultValue = noDefault.get<NoDefault>();
        CHECK(noDefaultValue != nullptr);
        if (noDefaultValue)
        {
            CHECK(noDefaultValue->value_ == 9);
        }

        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.createInstance("Missing")));

        Lifetime::alive_ = 0;
        {
            ObjectHandle lifetime = registry.createInstance("Lifetime");
            CHECK(static_cast<bool>(lifetime));
            CHECK(Lifetime::alive_ == 1);
            lifetime.reset();
            CHECK(Lifetime::alive_ == 0);
            CHECK(!lifetime);
        }
        CHECK(Lifetime::alive_ == 0);
    }

    void testMetadata()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        std::vector<std::string> classNames = registry.getClassNames();
        CHECK(std::find(classNames.begin(), classNames.end(), "TestObject") !=
              classNames.end());
        CHECK(std::find(classNames.begin(), classNames.end(), "TestObjectWithArgs") !=
              classNames.end());
        CHECK(registry.hasClass("TestObject"));
        CHECK(!registry.hasClass("Missing"));

        std::vector<FieldInfo> fields = registry.getFields("TestObject");
        CHECK(fields.size() == 11u);
        bool foundValue = false;
        bool foundConst = false;
        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            if (fields[index].name == "value")
            {
                foundValue = true;
                CHECK(fields[index].type == std::type_index(typeid(int)));
                CHECK(fields[index].writable);
            }
            if (fields[index].name == "constInt")
            {
                foundConst = true;
                CHECK(fields[index].type == std::type_index(typeid(int)));
                CHECK(!fields[index].writable);
            }
        }
        CHECK(foundValue);
        CHECK(foundConst);

        std::vector<MethodInfo> methods = registry.getMethods("TestObject");
        CHECK(methods.size() == 13u);
        bool foundIntOverload = false;
        bool foundStringOverload = false;
        bool foundMutableQualifier = false;
        bool foundConstQualifier = false;
        bool foundGreet = false;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            const MethodInfo &method = methods[index];
            if (method.name == "overloaded" && method.arity == 1u &&
                method.parameterTypes.size() == 1u &&
                method.parameterTypes[0] == std::type_index(typeid(int)))
            {
                foundIntOverload = true;
            }
            if (method.name == "overloaded" && method.arity == 1u &&
                method.parameterTypes.size() == 1u &&
                method.parameterTypes[0] == std::type_index(typeid(std::string)))
            {
                foundStringOverload = true;
                CHECK(method.isConst);
            }
            if (method.name == "qualifier" && method.arity == 0u)
            {
                if (method.isConst)
                {
                    foundConstQualifier = true;
                }
                else
                {
                    foundMutableQualifier = true;
                }
            }
            if (method.name == "greet")
            {
                foundGreet = true;
                CHECK(method.isConst);
                CHECK(method.returnType == std::type_index(typeid(std::string)));
            }
        }
        CHECK(foundIntOverload);
        CHECK(foundStringOverload);
        CHECK(foundMutableQualifier);
        CHECK(foundConstQualifier);
        CHECK(foundGreet);

        ClassInfo info = registry.getClassInfo("TestObject");
        CHECK(info.name == "TestObject");
        CHECK(info.fields.size() == fields.size());
        CHECK(info.methods.size() == methods.size());

        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getFields("Missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getMethods("Missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getClassInfo("Missing")));
    }

    void testRegistrationErrors()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object;

        CHECK(registry.hasClass("TestObject"));
        CHECK(registry.hasField("TestObject", "value"));
        CHECK(!registry.hasField("TestObject", "missing"));
        CHECK(registry.hasMethod("TestObject", "add"));
        CHECK(!registry.hasMethod("TestObject", "missing"));
        CHECK(registry.isFieldWritable("TestObject", "value"));
        CHECK(!registry.isFieldWritable("TestObject", "constInt"));
        CHECK(registry.hasClassName<TestObject>());
        CHECK(!registry.hasClassName<Unregistered>());
        CHECK_ERROR(ReflectionErrorCode::FieldNotFound,
                    (registry.isFieldWritable("TestObject", "missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getClassName<Unregistered>()));

        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerClassName<TestObject>("")));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerField<TestObject>("", "value",
                                                        &TestObject::value_)));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerField<TestObject>("TestObject", "",
                                                        &TestObject::value_)));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerField<TestObject>("",
                                                        &TestObject::value_)));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerMethod<TestObject, void>(
                        "TestObject", "", &TestObject::reset)));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerMethod<TestObject, void>(
                        "", "reset", &TestObject::reset)));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerClass<TestObject>("")));
        CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                    (registry.registerClass<NoDefault, int>("", 1)));

        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.registerField<Unregistered>(
                        "value", &Unregistered::value_)));

        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerClassName<TestObject>("Other")));
        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerField<TestObject>(
                        "TestObject", "value", &TestObject::value_)));
        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerMethod<TestObject, int, int, int>(
                        "TestObject", "add", &TestObject::add)));
        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerMethod<TestObject, int>(
                        "TestObject", "getValue", &TestObject::getValue)));
        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerClass<TestObject>("TestObject")));

        CHECK_ERROR(ReflectionErrorCode::FieldNotWritable,
                    (registry.getSetter("TestObject", "constInt")));
        CHECK_ERROR(ReflectionErrorCode::FieldNotFound,
                    (registry.getSetter("TestObject", "missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getSetter("Missing", "value")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getValues("Missing", "value", &object)));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getAllValues("Missing", &object)));
        CHECK_ERROR(ReflectionErrorCode::FieldNotFound,
                    (registry.getValues("TestObject", "missing", &object)));

        registry.registerClassName<Unregistered>("ConflictName");
        CHECK_ERROR(ReflectionErrorCode::ClassNameConflict,
                    (registry.registerClassName<NoDefault>("ConflictName")));
        CHECK_ERROR(ReflectionErrorCode::ClassNameConflict,
                    (registry.registerField<NoDefault>(
                        "ConflictName", "value", &NoDefault::value_)));
        CHECK_ERROR(ReflectionErrorCode::ClassNameConflict,
                    (registry.registerClass<NoDefault, int>("ConflictName", 1)));
    }

    void testThreadSafety()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object(3, "Bob");

        registry.freeze();
        CHECK(registry.isFrozen());

        CHECK_ERROR(ReflectionErrorCode::RegistryFrozen,
                    (registry.registerClass<NoDefault, int>(
                        "NoDefaultFrozen", 1)));
        CHECK_ERROR(ReflectionErrorCode::RegistryFrozen,
                    (registry.registerField<TestObject>(
                        "TestObject", "frozenField", &TestObject::value_)));
        CHECK_ERROR(ReflectionErrorCode::RegistryFrozen,
                    (registry.registerMethod<TestObject, int, int, int>(
                        "TestObject", "frozenMethod", &TestObject::add)));
        CHECK_ERROR(ReflectionErrorCode::RegistryFrozen,
                    (registry.registerClassName<NoDefault>("NoDefaultFrozenName")));

        std::atomic<int> errors(0);
        std::vector<std::thread> threads;
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex)
        {
            threads.push_back(std::thread([&registry, &object, &errors]() {
                try
                {
                    for (int index = 0; index < 500; ++index)
                    {
                        Any value = registry.getValues("TestObject", "value", &object);
                        if (any_cast<int>(value) != 3)
                        {
                            ++errors;
                        }
                        Any result = registry.invokeMethod(
                            "TestObject", "add", &object, {Any(1), Any(2)});
                        if (any_cast<int>(result) != 3)
                        {
                            ++errors;
                        }
                        if (!registry.hasField("TestObject", "value"))
                        {
                            ++errors;
                        }
                        if (registry.getFields("TestObject").size() != 11u)
                        {
                            ++errors;
                        }
                        if (registry.getMethods("TestObject").size() != 13u)
                        {
                            ++errors;
                        }
                    }
                }
                catch (...)
                {
                    ++errors;
                }
            }));
        }
        for (std::size_t index = 0; index < threads.size(); ++index)
        {
            threads[index].join();
        }
        CHECK(errors.load() == 0);
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
        {"Metadata", testMetadata},
        {"RegistrationErrors", testRegistrationErrors},
        {"ThreadSafety", testThreadSafety}};
}

int main(int argc, char **argv)
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::string filter;
    std::string exclude;
    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        if (argument == "--test" && index + 1 < argc)
        {
            filter = argv[++index];
        }
        else if (argument == "--exclude" && index + 1 < argc)
        {
            exclude = argv[++index];
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
        if (!exclude.empty() && exclude == tests[index].name)
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
