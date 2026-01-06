#include "Reflection.h"
#include <iostream>
#include <string>

#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#endif

using namespace Evently;

/**
 * @brief 测试用的Person类
 *
 * 包含基本的属性和方法，用于测试反射系统的各种功能
 */
class Person
{
public:
    /// 默认构造函数
    Person() : name_(""), age_(0) {}

    /// 带参数的构造函数
    Person(const std::string &name, int age) : name_(name), age_(age) {}

    /// 获取姓名
    std::string getName() const { return name_; }

    /// 设置姓名
    void setName(const std::string &name) { name_ = name; }

    /// 获取年龄
    int getAge() const { return age_; }

    /// 设置年龄
    void setAge(int age) { age_ = age; }

    /// 打印信息（void返回类型测试）
    void printInfo()
    {
        std::cout << "姓名: " << name_ << ", 年龄: " << age_ << std::endl;
    }

    /// 计算出生年份（带参数的方法测试）
    int calculateBirthYear(int currentYear)
    {
        return currentYear - age_;
    }

    /// 问候方法（带参数，返回字符串）
    std::string greet(const std::string &greeting)
    {
        return greeting + ", 我是 " + name_;
    }

    void TestConstMethod() const
    {
        std::cout << "这是一个const方法测试。" << std::endl;
    }

    void TestConstMethodWithParam(int value) const
    {
        std::cout << "这是一个带参数的const方法测试，参数值: " << value << std::endl;
    }

    int TestConstMethodWithReturn(int a) const
    {
        return a;
    }

public:
    std::string name_; ///< 姓名（公开成员，便于测试）
    int age_;          ///< 年龄（公开成员，便于测试）
    float money_; ///< 金钱（公开成员，便于测试）
    double height_; ///< 身高（公开成员，便于测试）
    bool isEmployed_; ///< 是否就业（公开成员，便于测试）
    char gender_; ///< 性别（公开成员，便于测试）
    long long id_; ///< ID（公开成员，便于测试）
    unsigned int score_; ///< 分数（公开成员，便于测试）
    unsigned long long timestamp_; ///< 时间戳（公开成员，便于测试）
    short int level_; ///< 等级（公开成员，便于测试）
    unsigned short int rank_; ///< 排名（公开成员，便于测试）
    signed char grade_; ///< 成绩（公开成员，便于测试）
    unsigned char status_; ///< 状态（公开成员，便于测试）

    const int constantValue_ = 42; ///< 常量值（测试const成员变量）
    const std::string constantString_ = "常量字符串"; ///< 常量字符串（测试const成员变量）
    const double constantDouble_ = 3.14159; ///< 常量双精度浮点数（测试const成员变量）
    const bool constantBool_ = true; ///< 常量布尔值（测试const成员变量）
    const char constantChar_ = 'C'; ///< 常量字符（测试const成员变量）
    const long long constantLongLong_ = 1234567890LL; ///< 常量长长整型（测试const成员变量）
    const unsigned int constantUnsignedInt_ = 987654321U; ///< 常量无符号整型（测试const成员变量）
    const float constantFloat_ = 2.71828f; ///< 常量浮点数（测试const成员变量）

};

/**
 * @brief 注册Person类的反射信息
 */
void registerPersonReflection()
{
    auto &registry = ReflectionRegistry::getInstance();

    // 注册类名
    registry.registerClassName<Person>("Person");

    // 注册成员变量
    registry.registerField<Person>("Person", "name", &Person::name_);
    registry.registerField<Person>("Person", "age", &Person::age_);
    registry.registerField<Person>("Person", "money", &Person::money_);
    registry.registerField<Person>("Person", "height", &Person::height_);
    registry.registerField<Person>("Person", "isEmployed", &Person::isEmployed_);
    registry.registerField<Person>("Person", "gender", &Person::gender_);
    registry.registerField<Person>("Person", "id", &Person::id_);
    registry.registerField<Person>("Person", "score", &Person::score_);
    registry.registerField<Person>("Person", "timestamp", &Person::timestamp_);
    registry.registerField<Person>("Person", "level", &Person::level_);
    registry.registerField<Person>("Person", "rank", &Person::rank_);
    registry.registerField<Person>("Person", "grade", &Person::grade_);
    registry.registerField<Person>("Person", "status", &Person::status_);

    //注册常量成员变量
    registry.registerField<Person>("constantValue", &Person::constantValue_);
    registry.registerField<Person>("constantString", &Person::constantString_);
    registry.registerField<Person>("constantDouble", &Person::constantDouble_);
    registry.registerField<Person>("constantBool", &Person::constantBool_);
    registry.registerField<Person>("constantChar", &Person::constantChar_);
    registry.registerField<Person>("constantLongLong", &Person::constantLongLong_);
    registry.registerField<Person>("constantUnsignedInt", &Person::constantUnsignedInt_);
    registry.registerField<Person>("constantFloat", &Person::constantFloat_);


    // 注册成员方法（暂时移除const方法）
    // registry.registerMethod<Person, std::string>("Person", "getName", &Person::getName);  // 这是const方法，暂时注释
    registry.registerMethod<Person, void, const std::string &>("Person", "setName", &Person::setName);
    // registry.registerMethod<Person, int>("Person", "getAge", &Person::getAge);  // 这是const方法，暂时注释
    registry.registerMethod<Person, void, int>("Person", "setAge", &Person::setAge);
    registry.registerMethod<Person, void>("Person", "printInfo", &Person::printInfo);
    registry.registerMethod<Person, int, int>("Person", "calculateBirthYear", &Person::calculateBirthYear);
    registry.registerMethod<Person, std::string, const std::string &>("Person", "greet", &Person::greet);

    // 注册类工厂（支持默认构造和带参数构造）
    registry.registerClass<Person>("Person");
    registry.registerClass<Person, std::string, int>("PersonWithParams", std::string("默认姓名"), 25);

    // 注册const成员方法
    registry.registerMethod<Person, void>("Person", "TestConstMethod", &Person::TestConstMethod);
    registry.registerMethod<Person, void, int>("Person", "TestConstMethodWithParam", &Person::TestConstMethodWithParam);
    registry.registerMethod<Person, int, int>("Person", "TestConstMethodWithReturn", &Person::TestConstMethodWithReturn);
}

/**
 * @brief 测试反射创建对象功能
 */
void testObjectCreation()
{
    std::cout << "\n=== 测试对象创建 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();

    // 创建默认构造的Person对象
    auto personPtr = registry.createInstance("Person");
    if (personPtr)
    {
        std::cout << "✓ 成功创建Person对象" << std::endl;

        // 获取原始指针进行测试
        Person *person = static_cast<Person *>(personPtr.get());
        std::cout << "初始姓名: " << person->name_ << ", 初始年龄: " << person->age_ << std::endl;
    }
    else
    {
        std::cout << "✗ 创建Person对象失败" << std::endl;
    }

    // 创建带参数的Person对象
    auto personWithParamsPtr = registry.createInstance("PersonWithParams");
    if (personWithParamsPtr)
    {
        std::cout << "✓ 成功创建带参数的Person对象" << std::endl;

        Person *person = static_cast<Person *>(personWithParamsPtr.get());
        std::cout << "初始姓名: " << person->name_ << ", 初始年龄: " << person->age_ << std::endl;
    }
    else
    {
        std::cout << "✗ 创建带参数的Person对象失败" << std::endl;
    }
}

/**
 * @brief 测试属性访问功能
 */
void testPropertyAccess()
{
    std::cout << "\n=== 测试属性访问 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();
    Person person("张三", 30);

    // 测试获取所有属性值
    auto allValues = registry.getAllValues("Person", &person);
    std::cout << "所有属性值:" << std::endl;
    for (const auto &pair : allValues)
    {
        std::cout << "  " << pair.first << ": ";
        try
        {
            if (pair.first == "name")
            {
                std::cout << any_cast<std::string>(pair.second);
            }
            else if (pair.first == "age")
            {
                std::cout << any_cast<int>(pair.second);
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "类型转换错误: " << e.what();
        }
        std::cout << std::endl;
    }

    // 测试获取单个属性值
    Any nameValue = registry.getValues("Person", "name", &person);
    Any ageValue = registry.getValues("Person", "age", &person);

    try
    {
        std::cout << "✓ 获取姓名: " << any_cast<std::string>(nameValue) << std::endl;
        std::cout << "✓ 获取年龄: " << any_cast<int>(ageValue) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 获取属性值失败: " << e.what() << std::endl;
    }

    // 测试设置属性值
    auto setter = registry.getSetter("Person", "name");
    if (setter)
    {
        setter->set(&person, Any(std::string("李四")));
        std::cout << "✓ 设置姓名成功，新姓名: " << person.name_ << std::endl;
    }

    setter = registry.getSetter("Person", "age");
    if (setter)
    {
        setter->set(&person, Any(35));
        std::cout << "✓ 设置年龄成功，新年龄: " << person.age_ << std::endl;
    }

    setter = registry.getSetter("Person", "money");
    if (setter)
    {
        setter->set(&person, Any(1000.50f));
        std::cout << "✓ 设置金钱成功，新金钱: " << person.money_ << std::endl;
    }

    setter = registry.getSetter("Person", "height");
    if (setter)
    {
        setter->set(&person, Any(1.75));
        std::cout << "✓ 设置身高成功，新身高: " << person.height_ << std::endl;
    }
    setter = registry.getSetter("Person", "isEmployed");    
    if (setter)
    {
        setter->set(&person, Any(true));
        std::cout << "✓ 设置就业状态成功，新状态: " << person.isEmployed_ << std::endl;
    }
    setter = registry.getSetter("Person", "gender");
    if (setter)
    {
        setter->set(&person, Any('M'));
        std::cout << "✓ 设置性别成功，新性别: " << person.gender_ << std::endl;
    }

    setter = registry.getSetter("Person", "id");
    if (setter)
    {
        setter->set(&person, Any(123456789LL));
        std::cout << "✓ 设置ID成功，新ID: " << person.id_ << std::endl;
    }
    setter = registry.getSetter("Person", "score");
    if (setter)
    {
        setter->set(&person, Any(95U));
        std::cout << "✓ 设置分数成功，新分数: " << person.score_ << std::endl;
    }
    setter = registry.getSetter("Person", "timestamp");
    if (setter)
    {
        setter->set(&person, Any(1700000000ULL));
        std::cout << "✓ 设置时间戳成功，新时间戳: " << person.timestamp_ << std::endl;
    }
    setter = registry.getSetter("Person", "level");
    if (setter)
    {
        setter->set(&person, Any(static_cast<short int>(5)));
        std::cout << "✓ 设置等级成功，新等级: " << person.level_ << std::endl;
    }   
    setter = registry.getSetter("Person", "rank");
    if (setter)
    {
        setter->set(&person, Any(static_cast<unsigned short int>(3)));
        std::cout << "✓ 设置排名成功，新排名: " << person.rank_ << std::endl;
    }
    setter = registry.getSetter("Person", "grade");
    if (setter)
    {
        setter->set(&person, Any(static_cast<signed char>(90)));
        std::cout << "✓ 设置成绩成功，新成绩: " << static_cast<int>(person.grade_) << std::endl;
    }
    setter = registry.getSetter("Person", "status");
    if (setter)
    {
        setter->set(&person, Any(static_cast<unsigned char>(1)));
        std::cout << "✓ 设置状态成功，新状态: " << static_cast<int>(person.status_) << std::endl;
    }
    // 再次获取属性值验证设置结果
    allValues = registry.getAllValues("Person", &person);
    std::cout << "更新后的所有属性值:" << std::endl;
    for (const auto &pair : allValues)
    {
        std::cout << "  " << pair.first << ": ";
        try
        {
            if (pair.first == "name")
            {
                std::cout << any_cast<std::string>(pair.second);    
            }
            else if (pair.first == "age")
            {
                std::cout << any_cast<int>(pair.second);
            }
            else if (pair.first == "money")
            {
                std::cout << any_cast<float>(pair.second);
            }
            else if (pair.first == "height")
            {
                std::cout << any_cast<double>(pair.second);
            }
            else if (pair.first == "isEmployed")
            {
                std::cout << any_cast<bool>(pair.second);
            }
            else if (pair.first == "gender")
            {
                std::cout << any_cast<char>(pair.second);
            }
            else if (pair.first == "id")
            {
                std::cout << any_cast<long long>(pair.second);
            }
            else if (pair.first == "score")
            {
                std::cout << any_cast<unsigned int>(pair.second);
            }
            else if (pair.first == "timestamp")
            {
                std::cout << any_cast<unsigned long long>(pair.second);
            }
            else if (pair.first == "level")
            {
                std::cout << any_cast<short int>(pair.second);
            }
            else if (pair.first == "rank")
            {
                std::cout << any_cast<unsigned short int>(pair.second);
            }
            else if (pair.first == "grade")
            {
                std::cout << static_cast<int>(any_cast<signed char>(pair.second));
            }
            else if (pair.first == "status")
            {
                std::cout << static_cast<int>(any_cast<unsigned char>(pair.second));
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "类型转换错误: " << e.what();
        }
        std::cout << std::endl;
    }
}


/**
 * @brief 测试方法调用功能
 */
void testMethodInvocation()
{
    std::cout << "\n=== 测试方法调用 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();
    Person person("王五", 28);

    // 获取所有方法名
    auto methodNames = registry.getMethodNames("Person");
    std::cout << "注册的方法列表:" << std::endl;
    for (const auto &methodName : methodNames)
    {
        std::cout << "  " << methodName << std::endl;
    }

    try
    {
        // 测试无参数方法调用
        std::cout << "\n调用printInfo方法:" << std::endl;
        registry.invokeMethod("Person", "printInfo", &person, {});

        // 暂时注释掉const方法的测试
        // Any nameResult = registry.invokeMethod("Person", "getName", &person, {});
        // std::cout << "✓ getName返回: " << any_cast<std::string>(nameResult) << std::endl;

        // Any ageResult = registry.invokeMethod("Person", "getAge", &person, {});
        // std::cout << "✓ getAge返回: " << any_cast<int>(ageResult) << std::endl;

        // 测试带参数的void方法
        std::vector<Any> setNameArgs = {Any(std::string("赵六"))};
        registry.invokeMethod("Person", "setName", &person, setNameArgs);
        std::cout << "✓ setName调用成功，新姓名: " << person.name_ << std::endl;

        std::vector<Any> setAgeArgs = {Any(32)};
        registry.invokeMethod("Person", "setAge", &person, setAgeArgs);
        std::cout << "✓ setAge调用成功，新年龄: " << person.age_ << std::endl;

        // 测试带参数有返回值的方法
        std::vector<Any> birthYearArgs = {Any(2024)};
        Any birthYearResult = registry.invokeMethod("Person", "calculateBirthYear", &person, birthYearArgs);
        std::cout << "✓ calculateBirthYear返回: " << any_cast<int>(birthYearResult) << std::endl;

        // 测试字符串参数和返回值
        std::vector<Any> greetArgs = {Any(std::string("你好"))};
        Any greetResult = registry.invokeMethod("Person", "greet", &person, greetArgs);
        std::cout << "✓ greet返回: " << any_cast<std::string>(greetResult) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 方法调用失败: " << e.what() << std::endl;
    }
}

/**
 * @brief 测试错误处理
 */
void testErrorHandling()
{
    std::cout << "\n=== 测试错误处理 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();
    Person person("测试", 25);

    try
    {
        // 测试调用不存在的方法
        registry.invokeMethod("Person", "nonExistentMethod", &person, {});
    }
    catch (const std::exception &e)
    {
        std::cout << "✓ 正确捕获不存在方法的异常: " << e.what() << std::endl;
    }

    try
    {
        // 测试参数数量不匹配
        std::vector<Any> wrongArgs = {Any(1), Any(2)}; // setAge只需要一个参数
        registry.invokeMethod("Person", "setAge", &person, wrongArgs);
    }
    catch (const std::exception &e)
    {
        std::cout << "✓ 正确捕获参数数量不匹配的异常: " << e.what() << std::endl;
    }

    try
    {
        // 测试类型不匹配
        auto setter = registry.getSetter("Person", "age");
        if (setter)
        {
            setter->set(&person, Any(std::string("不是数字"))); // age应该是int类型
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "✓ 正确捕获类型不匹配的异常: " << e.what() << std::endl;
    }

    // 测试获取不存在的类名
    std::string className = registry.getClassName<Person>();
    std::cout << "✓ Person类的注册名称: " << className << std::endl;
}


/** 
 * @brief 测试const成员变量访问
*/
void testConstMemberAccess()
{
    std::cout << "\n=== 测试const成员变量访问 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();
    Person person;

    try
    {
        // 获取const成员变量值（保持const限定）
        Any constValue = registry.getValues("Person", "constantValue", &person);
        auto constVal = any_cast<const int>(constValue);
        constVal += 10; // 更新constVal以反映修改后的值
        std::cout << "✓ constantValue: " << constVal << std::endl;

        auto setter = registry.getSetter("Person", "constantValue");
        if (setter) {
            std::cout << "✗ 错误：不应为 const 成员暴露 setter" << std::endl;
        } else {
            std::cout << "✓ const 成员没有 setter（预期）" << std::endl;
        }
        Any constString = registry.getValues("Person", "constantString", &person);
        std::cout << "✓ constantString: " << any_cast<std::string>(constString) << std::endl;

        Any constDouble = registry.getValues("Person", "constantDouble", &person);
        std::cout << "✓ constantDouble: " << any_cast<double>(constDouble) << std::endl;

        Any constBool = registry.getValues("Person", "constantBool", &person);
        std::cout << "✓ constantBool: " << any_cast<bool>(constBool) << std::endl;

        Any constChar = registry.getValues("Person", "constantChar", &person);
        std::cout << "✓ constantChar: " << any_cast<char>(constChar) << std::endl;

        Any constLongLong = registry.getValues("Person", "constantLongLong", &person);
        std::cout << "✓ constantLongLong: " << any_cast<long long>(constLongLong) << std::endl;

        Any constUnsignedInt = registry.getValues("Person", "constantUnsignedInt", &person);
        std::cout << "✓ constantUnsignedInt: " << any_cast<unsigned int>(constUnsignedInt) << std::endl;

        Any constFloat = registry.getValues("Person", "constantFloat", &person);
        std::cout << "✓ constantFloat: " << any_cast<float>(constFloat) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 访问const成员变量失败: " << e.what() << std::endl;
    }
}


/** 
  * @brief 测试const成员方法调用
*/
void testConstMethodInvocation()
{
    std::cout << "\n=== 测试const成员方法调用 ===" << std::endl;

    auto &registry = ReflectionRegistry::getInstance();
    Person person("测试", 25);

    try
    {
        // 调用无参数的const方法
        Any result = registry.invokeMethod("Person", "TestConstMethod", &person, {});
        std::cout << "✓ 成功调用TestConstMethod方法" << std::endl;

        // 调用带参数的const方法
        std::vector<Any> args = { Any(42) };
        result = registry.invokeMethod("Person", "TestConstMethodWithParam", &person, args);
        std::cout << "✓ 成功调用TestConstMethodWithParam方法" << std::endl;

        // 调用带参数有返回值的const方法
        std::vector<Any> returnArgs = { Any(55) };
        result = registry.invokeMethod("Person", "TestConstMethodWithReturn", &person, returnArgs);
        std::cout << "✓ 成功调用TestConstMethodWithReturn方法，返回值: " << any_cast<int>(result) << std::endl;

    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 调用const成员方法失败: " << e.what() << std::endl;
    }
}

/**
 * @brief 主函数
 */
int main()
{

#if defined(_WIN64) || defined(_WIN32)
    // 设置控制台输出为UTF-8编码，解决Windows控制台中文乱码问题
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "=== C++11反射系统测试程序 ===" << std::endl;

    try
    {
        // 注册反射信息
        registerPersonReflection();
        std::cout << "✓ 反射信息注册完成" << std::endl;

        // 执行各项测试
        // testObjectCreation();
        // testPropertyAccess();
        // testMethodInvocation();
        // testErrorHandling();
        // testConstMemberAccess();
        testConstMethodInvocation();


        std::cout << "\n=== 所有测试完成 ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "✗ 程序执行出错: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
