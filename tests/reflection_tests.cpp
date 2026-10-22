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

        static int staticAdd(int a, int b) { return a + b; }

        int refQualified() & { return 1; }
        int refQualified() const & { return 2; }
        int refQualified() && { return 3; }
        int refQualified() const && { return 4; }

        int noexceptValue() const noexcept { return 7; }

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

    class ConstructorObject
    {
    public:
        ConstructorObject() : value_(0) {}
        explicit ConstructorObject(int value) : value_(value) {}
        ConstructorObject(int value, const std::string &name)
            : value_(value), name_(name) {}

        int value_;
        std::string name_;
    };

    class NoFactory
    {
    public:
        int value_ = 0;
    };

    class AutoObject
    {
    public:
        AutoObject() : value_(0) {}
        int getValue() const { return value_; }

        int value_;
    };

    class DynamicObject
    {
    public:
        int value_ = 0;
    };

    class BatchObject
    {
    public:
        int value_ = 0;
    };

    class RemovableObject
    {
    public:
        int value_ = 0;
    };

    class MultiAliasObject
    {
    public:
        int value_ = 0;
    };

    class ClearedObject
    {
    public:
        int value_ = 0;
    };

    class PluginObject
    {
    public:
        int value_ = 0;
    };

    EVENTLY_REGISTER_CLASS(AutoObject, "AutoObject")
    {
        Evently::ClassRegistration<AutoObject> registration("AutoObject");
        registration.constructor<>();
        registration.field("value", &AutoObject::value_);
        registration.method("getValue", &AutoObject::getValue);
    }

    int freeAdd(int a, int b)
    {
        return a + b;
    }

    typedef int (TestObject::*OverloadedInt)(int);
    typedef std::string (TestObject::*OverloadedString)(const std::string &) const;
    typedef int (TestObject::*QualifierMutable)();
    typedef int (TestObject::*QualifierConst)() const;
    typedef int (TestObject::*RefQualifiedLValue)() &;
    typedef int (TestObject::*RefQualifiedConstLValue)() const &;
    typedef int (TestObject::*RefQualifiedRValue)() &&;
    typedef int (TestObject::*RefQualifiedConstRValue)() const &&;

#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
    const std::size_t expectedMethodCount = 19u;
    const std::size_t expectedMethodNameCount = 14u;
#else
    const std::size_t expectedMethodCount = 18u;
    const std::size_t expectedMethodNameCount = 13u;
#endif

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
        registry.registerMethod<TestObject, int>(
            "TestObject", "refQualified",
            static_cast<RefQualifiedLValue>(&TestObject::refQualified));
        registry.registerMethod<TestObject, int>(
            "TestObject", "refQualified",
            static_cast<RefQualifiedConstLValue>(&TestObject::refQualified));
        registry.registerMethod<TestObject, int>(
            "TestObject", "refQualified",
            static_cast<RefQualifiedRValue>(&TestObject::refQualified));
        registry.registerMethod<TestObject, int>(
            "TestObject", "refQualified",
            static_cast<RefQualifiedConstRValue>(&TestObject::refQualified));
#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
        registry.registerMethod<TestObject, int>(
            "TestObject", "noexceptValue", &TestObject::noexceptValue);
#endif
        registry.registerStaticMethod<TestObject, int, int, int>(
            "TestObject", "staticAdd", &TestObject::staticAdd);
        registry.registerFunction<int, int, int>(
            "FreeFunctions", "add", &freeAdd);

        registry.registerClass<TestObject>("TestObject");
        registry.registerClass<TestObject, int, std::string>(
            "TestObjectWithArgs", 7, std::string("seven"));
        registry.registerClass<NoDefault, int>("NoDefaultWithArgs", 9);
        registry.registerClass<Lifetime>("Lifetime");

        registry.registerClassName<ConstructorObject>("ConstructorObject");
        registry.registerConstructor<ConstructorObject>("ConstructorObject");
        registry.registerConstructor<ConstructorObject, int>("ConstructorObject");
        registry.registerConstructor<ConstructorObject, int, const std::string &>(
            "ConstructorObject");
        registry.registerClassName<NoFactory>("NoFactory");
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

        CHECK(assigned.has_value());
        assigned.reset();
        CHECK(!assigned.has_value());
        CHECK(assigned.empty());

        Any emplaced;
        emplaced.emplace<std::string>("emplaced");
        CHECK(any_cast<std::string>(emplaced) == "emplaced");

        Any first(1);
        Any second(std::string("second"));
        first.swap(second);
        CHECK(any_cast<std::string>(first) == "second");
        CHECK(any_cast<int>(second) == 1);

        Any localSource(5);
        Any localCopy(localSource);
        Any localMoved(std::move(localSource));
        CHECK(localSource.empty());
        CHECK(any_cast<int>(localCopy) == 5);
        CHECK(any_cast<int>(localMoved) == 5);
    }

    template <typename T>
    void checkField(ReflectionRegistry &registry, TestObject &object,
                    const std::string &name, const T &value, const T &expected)
    {
        std::shared_ptr<PropertySetterBase> setter = registry.getSetter("TestObject", name);
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

        std::shared_ptr<PropertySetterBase> setter = registry.getSetter("TestObject", "value");
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
        CHECK(names.size() == expectedMethodNameCount);
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
        CHECK(methods.size() == expectedMethodCount);
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
        CHECK(info.constructors.empty());
        CHECK(registry.getConstructors("TestObject").empty());

        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getFields("Missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getMethods("Missing")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.getClassInfo("Missing")));
    }

    void testFunctions()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object;

        CHECK(any_cast<int>(registry.invokeFunction(
                  "TestObject", "staticAdd", {Any(2), Any(3)})) == 5);
        CHECK(any_cast<int>(registry.invokeFunction(
                  "FreeFunctions", "add", {Any(3), Any(4)})) == 7);
        CHECK(registry.hasClass("FreeFunctions"));
        CHECK(registry.hasMethod("TestObject", "staticAdd"));
        CHECK(registry.hasMethod("FreeFunctions", "add"));

        CHECK_ERROR(ReflectionErrorCode::MethodNotFound,
                    (registry.invokeMethod("TestObject", "staticAdd", &object,
                                           {Any(1), Any(2)})));
        CHECK_ERROR(ReflectionErrorCode::MethodNotFound,
                    (registry.invokeFunction("FreeFunctions", "missing", {})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentCountMismatch,
                    (registry.invokeFunction("FreeFunctions", "add", {Any(1)})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentTypeMismatch,
                    (registry.invokeFunction("FreeFunctions", "add",
                                             {Any(1.5), Any(2.5)})));

        std::vector<MethodInfo> functions = registry.getMethods("FreeFunctions");
        CHECK(functions.size() == 1u);
        if (functions.size() == 1u)
        {
            CHECK(functions[0].isStatic);
            CHECK(!functions[0].isConst);
            CHECK(functions[0].name == "add");
            CHECK(functions[0].arity == 2u);
        }

        std::vector<MethodInfo> methods = registry.getMethods("TestObject");
        bool foundStatic = false;
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            if (methods[index].name == "staticAdd")
            {
                foundStatic = true;
                CHECK(methods[index].isStatic);
            }
        }
        CHECK(foundStatic);
    }

    void testQualifiers()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        TestObject object(3, "Bob");
        const TestObject constObject(4, "Const");

        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "refQualified", &object, {})) == 1);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "refQualified", &constObject, {})) == 2);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "refQualified", RvalueInstance(&object), {})) == 3);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "refQualified", RvalueInstance(&constObject), {})) == 4);
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "refQualified",
                  RvalueInstance(static_cast<const void *>(&constObject)), {})) == 4);

#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
        CHECK(any_cast<int>(registry.invokeMethod(
                  "TestObject", "noexceptValue", &constObject, {})) == 7);
#endif

        std::vector<MethodInfo> methods = registry.getMethods("TestObject");
        int lvalueCount = 0;
        int constLvalueCount = 0;
        int rvalueCount = 0;
        int constRvalueCount = 0;
#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
        bool foundNoexcept = false;
#endif
        for (std::size_t index = 0; index < methods.size(); ++index)
        {
            if (methods[index].name == "refQualified")
            {
                if (methods[index].refQualifier == RefQualifier::LValue)
                {
                    ++lvalueCount;
                }
                else if (methods[index].refQualifier ==
                         RefQualifier::ConstLValue)
                {
                    ++constLvalueCount;
                }
                else if (methods[index].refQualifier == RefQualifier::RValue)
                {
                    ++rvalueCount;
                }
                else if (methods[index].refQualifier ==
                         RefQualifier::ConstRValue)
                {
                    ++constRvalueCount;
                }
            }
#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
            if (methods[index].name == "noexceptValue")
            {
                foundNoexcept = true;
                CHECK(methods[index].isConst);
            }
#endif
        }
        CHECK(lvalueCount == 1);
        CHECK(constLvalueCount == 1);
        CHECK(rvalueCount == 1);
        CHECK(constRvalueCount == 1);
#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
        CHECK(foundNoexcept);
#endif
    }

    void testConstructors()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        ObjectHandle defaultObject = registry.createInstance("ConstructorObject");
        ConstructorObject *defaultValue = defaultObject.get<ConstructorObject>();
        CHECK(defaultValue != nullptr);
        if (defaultValue)
        {
            CHECK(defaultValue->value_ == 0);
            CHECK(defaultValue->name_.empty());
        }

        ObjectHandle oneArg = registry.createInstance(
            "ConstructorObject", {Any(7)});
        ConstructorObject *oneArgValue = oneArg.get<ConstructorObject>();
        CHECK(oneArgValue != nullptr);
        if (oneArgValue)
        {
            CHECK(oneArgValue->value_ == 7);
        }

        ObjectHandle twoArgs = registry.createInstanceWithArgs(
            "ConstructorObject", 9, std::string("nine"));
        ConstructorObject *twoArgsValue = twoArgs.get<ConstructorObject>();
        CHECK(twoArgsValue != nullptr);
        if (twoArgsValue)
        {
            CHECK(twoArgsValue->value_ == 9);
            CHECK(twoArgsValue->name_ == "nine");
        }

        CHECK_ERROR(ReflectionErrorCode::ArgumentCountMismatch,
                    (registry.createInstance(
                        "ConstructorObject", {Any(1), Any(2), Any(3)})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentTypeMismatch,
                    (registry.createInstance(
                        "ConstructorObject", {Any(std::string("bad"))})));
        CHECK_ERROR(ReflectionErrorCode::ArgumentTypeMismatch,
                    (registry.createInstance(
                        "ConstructorObject", {Any(1), Any(2)})));
        CHECK_ERROR(ReflectionErrorCode::ConstructorNotFound,
                    (registry.createInstance("NoFactory")));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.createInstance("MissingConstructorClass")));

        CHECK(registry.getConstructors("ConstructorObject").size() == 3u);
        ClassInfo info = registry.getClassInfo("ConstructorObject");
        CHECK(info.constructors.size() == 3u);
        CHECK_ERROR(ReflectionErrorCode::DuplicateRegistration,
                    (registry.registerConstructor<ConstructorObject, int>(
                        "ConstructorObject")));
    }

    void testAutoRegistration()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        CHECK(registry.hasClass("AutoObject"));
        CHECK(registry.hasField("AutoObject", "value"));
        CHECK(registry.hasMethod("AutoObject", "getValue"));

        ObjectHandle object = registry.createInstance("AutoObject");
        AutoObject *value = object.get<AutoObject>();
        CHECK(value != nullptr);

        std::shared_ptr<PropertySetterBase> setter = registry.getSetter("AutoObject", "value");
        CHECK(setter != nullptr);
        if (value != nullptr && setter != nullptr)
        {
            setter->set(value, Any(11));
            CHECK(any_cast<int>(registry.invokeMethod(
                      "AutoObject", "getValue", value, {})) == 11);
        }

        CHECK(registry.getMethodNames("AutoObject").size() == 1u);
        CHECK(registry.getConstructors("AutoObject").size() == 1u);
    }

    void testRegistrationBatch()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        {
            RegistrationBatch batch(registry);
            registry.registerClassName<BatchObject>("BatchObject");
            registry.registerField<BatchObject>("BatchObject", "value",
                                                &BatchObject::value_);
            registry.registerConstructor<BatchObject>("BatchObject");
            batch.commit();
        }
        CHECK(registry.hasClass("BatchObject"));
        CHECK(registry.hasField("BatchObject", "value"));
        ObjectHandle batchObject = registry.createInstance("BatchObject");
        CHECK(batchObject.get<BatchObject>() != nullptr);

        bool duplicateCaught = false;
        try
        {
            RegistrationBatch batch(registry);
            registry.registerClassName<BatchObject>("BatchRollback");
            registry.registerField<BatchObject>("BatchRollback", "value",
                                                &BatchObject::value_);
            registry.registerField<BatchObject>("BatchRollback", "value",
                                                &BatchObject::value_);
            batch.commit();
        }
        catch (const ReflectionError &error)
        {
            duplicateCaught =
                error.code() == ReflectionErrorCode::DuplicateRegistration;
        }
        CHECK(duplicateCaught);
        CHECK(!registry.hasClass("BatchRollback"));

        bool nestedCaught = false;
        try
        {
            RegistrationBatch outer(registry);
            RegistrationBatch inner(registry);
        }
        catch (const ReflectionError &error)
        {
            nestedCaught = error.code() == ReflectionErrorCode::InvalidArgument;
        }
        CHECK(nestedCaught);
    }

    void testLifecycle()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();

        registry.registerClassName<RemovableObject>("RemovableObject");
        registry.registerField<RemovableObject>("RemovableObject", "value",
                                                &RemovableObject::value_);
        registry.registerConstructor<RemovableObject>("RemovableObject");
        CHECK(registry.hasClass("RemovableObject"));
        CHECK(registry.unregisterClass("RemovableObject"));
        CHECK(!registry.hasClass("RemovableObject"));
        CHECK(!registry.hasField("RemovableObject", "value"));
        CHECK_ERROR(ReflectionErrorCode::ClassNotFound,
                    (registry.createInstance("RemovableObject")));
        CHECK(!registry.unregisterClass("RemovableObject"));

        registry.registerClassName<RemovableObject>("RemovableObject");
        registry.registerField<RemovableObject>("RemovableObject", "value",
                                                &RemovableObject::value_);
        registry.registerConstructor<RemovableObject>("RemovableObject");
        CHECK(registry.hasClass("RemovableObject"));
        CHECK(registry.unregisterClass("RemovableObject"));

        registry.registerClass<MultiAliasObject>("AliasA");
        registry.registerClass<MultiAliasObject>("AliasB");
        registry.registerField<MultiAliasObject>("AliasA", "value",
                                                 &MultiAliasObject::value_);
        registry.registerField<MultiAliasObject>("AliasB", "value",
                                                 &MultiAliasObject::value_);
        CHECK(registry.hasClass("AliasA"));
        CHECK(registry.hasClass("AliasB"));
        CHECK(registry.unregisterClass("AliasA"));
        CHECK(!registry.hasClass("AliasA"));
        CHECK(registry.hasClass("AliasB"));
        CHECK(registry.hasField("AliasB", "value"));
        CHECK(registry.unregisterClass("AliasB"));
        CHECK(!registry.hasClass("AliasB"));
    }

    void testPluginLifecycle()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        CHECK(!registry.hasClass("PluginObject"));

        {
            Evently::PluginRegistration plugin(registry, "TestPlugin");
            CHECK(plugin.moduleName() == "TestPlugin");
            {
                Evently::RegistrationBatch batch(registry);
                registry.registerClassName<PluginObject>("PluginObject");
                registry.registerField<PluginObject>("PluginObject", "value",
                                                     &PluginObject::value_);
                registry.registerConstructor<PluginObject>("PluginObject");
                batch.commit();
            }
            CHECK(registry.hasClass("PluginObject"));
            CHECK(registry.hasField("PluginObject", "value"));
            CHECK_ERROR(ReflectionErrorCode::InvalidArgument,
                        (registry.registerField<TestObject>(
                            "TestObject", "pluginField",
                            &TestObject::value_)));
        }
        CHECK(!registry.hasClass("PluginObject"));

        {
            Evently::PluginRegistration plugin(registry, "DetachedPlugin");
            registry.registerClassName<PluginObject>("PluginDetached");
            registry.registerField<PluginObject>("PluginDetached", "value",
                                                 &PluginObject::value_);
            plugin.detach();
        }
        CHECK(registry.hasClass("PluginDetached"));
        CHECK(registry.unregisterClass("PluginDetached"));

        bool nestedCaught = false;
        try
        {
            Evently::PluginRegistration outer(registry, "Outer");
            Evently::PluginRegistration inner(registry, "Inner");
        }
        catch (const ReflectionError &error)
        {
            nestedCaught =
                error.code() == ReflectionErrorCode::InvalidArgument;
        }
        CHECK(nestedCaught);
    }

    void testClear()
    {
        ReflectionRegistry &registry = ReflectionRegistry::getInstance();
        registry.clear();
        CHECK(registry.getClassNames().empty());
        CHECK(!registry.hasClass("TestObject"));

        registry.registerClassName<ClearedObject>("ClearedObject");
        registry.registerField<ClearedObject>("ClearedObject", "value",
                                              &ClearedObject::value_);
        registry.registerConstructor<ClearedObject>("ClearedObject");
        CHECK(registry.hasClass("ClearedObject"));
        CHECK(registry.unregisterClass("ClearedObject"));
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

        registry.registerClassName<DynamicObject>("DynamicObject");
        registry.registerField<DynamicObject>("DynamicObject", "value",
                                              &DynamicObject::value_);
        registry.registerConstructor<DynamicObject>("DynamicObject");
        CHECK(registry.hasClass("DynamicObject"));
        CHECK(registry.hasField("DynamicObject", "value"));
        CHECK(registry.getConstructors("DynamicObject").size() == 1u);

        ObjectHandle dynamic = registry.createInstance("DynamicObject");
        CHECK(dynamic.get<DynamicObject>() != nullptr);

        std::atomic<int> errors(0);
        std::vector<std::thread> readers;
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex)
        {
            readers.push_back(std::thread([&registry, &object, &errors]() {
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
                        if (registry.getMethods("TestObject").size() !=
                            expectedMethodCount)
                        {
                            ++errors;
                        }
                        if (!registry.hasClass("DynamicObject"))
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

        std::thread writer([&registry, &errors]() {
            try
            {
                for (int index = 0; index < 200; ++index)
                {
                    const std::string name =
                        std::string("DynamicAlias") + std::to_string(index);
                    registry.registerClass<DynamicObject>(name);
                    registry.registerField<DynamicObject>(
                        name, "value", &DynamicObject::value_);
                }
            }
            catch (...)
            {
                ++errors;
            }
        });

        writer.join();
        for (std::size_t index = 0; index < readers.size(); ++index)
        {
            readers[index].join();
        }
        CHECK(errors.load() == 0);
        CHECK(registry.hasClass("DynamicAlias199"));
        CHECK(registry.hasField("DynamicAlias199", "value"));
        ObjectHandle late = registry.createInstance("DynamicAlias199");
        CHECK(late.get<DynamicObject>() != nullptr);
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
        {"Functions", testFunctions},
        {"Qualifiers", testQualifiers},
        {"Constructors", testConstructors},
        {"AutoRegistration", testAutoRegistration},
        {"RegistrationBatch", testRegistrationBatch},
        {"Lifecycle", testLifecycle},
        {"PluginLifecycle", testPluginLifecycle},
        {"RegistrationErrors", testRegistrationErrors},
        {"ThreadSafety", testThreadSafety},
        {"Clear", testClear}};
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
        ReflectionRegistry::getInstance().registerAll();
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
