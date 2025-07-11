#include "Reflection.h"
#include <iostream>
#include <string>

using namespace Evently;

/**
 * @brief 测试用的Person类
 * 
 * 包含基本的属性和方法，用于测试反射系统的各种功能
 */
class Person {
public:
    /// 默认构造函数
    Person() : name_(""), age_(0) {}
    
    /// 带参数的构造函数
    Person(const std::string& name, int age) : name_(name), age_(age) {}
    
    /// 获取姓名
    std::string getName() const { return name_; }
    
    /// 设置姓名
    void setName(const std::string& name) { name_ = name; }
    
    /// 获取年龄
    int getAge() const { return age_; }
    
    /// 设置年龄
    void setAge(int age) { age_ = age; }
    
    /// 打印信息（void返回类型测试）
    void printInfo() {
        std::cout << "姓名: " << name_ << ", 年龄: " << age_ << std::endl;
    }
    
    /// 计算出生年份（带参数的方法测试）
    int calculateBirthYear(int currentYear) {
        return currentYear - age_;
    }
    
    /// 问候方法（带参数，返回字符串）
    std::string greet(const std::string& greeting) {
        return greeting + ", 我是 " + name_;
    }

public:
    std::string name_;  ///< 姓名（公开成员，便于测试）
    int age_;          ///< 年龄（公开成员，便于测试）
};

/**
 * @brief 注册Person类的反射信息
 */
void registerPersonReflection() {
    auto& registry = ReflectionRegistry::getInstance();
    
    // 注册类名
    registry.registerClassName<Person>("Person");
    
    // 注册成员变量
    registry.registerField<Person>("Person", "name", &Person::name_);
    registry.registerField<Person>("Person", "age", &Person::age_);
    
    // 注册成员方法（暂时移除const方法）
    // registry.registerMethod<Person, std::string>("Person", "getName", &Person::getName);  // 这是const方法，暂时注释
    registry.registerMethod<Person, void, const std::string&>("Person", "setName", &Person::setName);
    // registry.registerMethod<Person, int>("Person", "getAge", &Person::getAge);  // 这是const方法，暂时注释
    registry.registerMethod<Person, void, int>("Person", "setAge", &Person::setAge);
    registry.registerMethod<Person, void>("Person", "printInfo", &Person::printInfo);
    registry.registerMethod<Person, int, int>("Person", "calculateBirthYear", &Person::calculateBirthYear);
    registry.registerMethod<Person, std::string, const std::string&>("Person", "greet", &Person::greet);
    
    // 注册类工厂（支持默认构造和带参数构造）
    registry.registerClass<Person>("Person");
    registry.registerClass<Person, std::string, int>("PersonWithParams", std::string("默认姓名"), 25);
}

/**
 * @brief 测试反射创建对象功能
 */
void testObjectCreation() {
    std::cout << "\n=== 测试对象创建 ===" << std::endl;
    
    auto& registry = ReflectionRegistry::getInstance();
    
    // 创建默认构造的Person对象
    auto personPtr = registry.createInstance("Person");
    if (personPtr) {
        std::cout << "✓ 成功创建Person对象" << std::endl;
        
        // 获取原始指针进行测试
        Person* person = static_cast<Person*>(personPtr.get());
        std::cout << "初始姓名: " << person->name_ << ", 初始年龄: " << person->age_ << std::endl;
    } else {
        std::cout << "✗ 创建Person对象失败" << std::endl;
    }
    
    // 创建带参数的Person对象
    auto personWithParamsPtr = registry.createInstance("PersonWithParams");
    if (personWithParamsPtr) {
        std::cout << "✓ 成功创建带参数的Person对象" << std::endl;
        
        Person* person = static_cast<Person*>(personWithParamsPtr.get());
        std::cout << "初始姓名: " << person->name_ << ", 初始年龄: " << person->age_ << std::endl;
    } else {
        std::cout << "✗ 创建带参数的Person对象失败" << std::endl;
    }
}

/**
 * @brief 测试属性访问功能
 */
void testPropertyAccess() {
    std::cout << "\n=== 测试属性访问 ===" << std::endl;
    
    auto& registry = ReflectionRegistry::getInstance();
    Person person("张三", 30);
    
    // 测试获取所有属性值
    auto allValues = registry.getAllValues("Person", &person);
    std::cout << "所有属性值:" << std::endl;
    for (const auto& pair : allValues) {
        std::cout << "  " << pair.first << ": ";
        try {
            if (pair.first == "name") {
                std::cout << any_cast<std::string>(pair.second);
            } else if (pair.first == "age") {
                std::cout << any_cast<int>(pair.second);
            }
        } catch (const std::exception& e) {
            std::cout << "类型转换错误: " << e.what();
        }
        std::cout << std::endl;
    }
    
    // 测试获取单个属性值
    Any nameValue = registry.getValues("Person", "name", &person);
    Any ageValue = registry.getValues("Person", "age", &person);
    
    try {
        std::cout << "✓ 获取姓名: " << any_cast<std::string>(nameValue) << std::endl;
        std::cout << "✓ 获取年龄: " << any_cast<int>(ageValue) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ 获取属性值失败: " << e.what() << std::endl;
    }
    
    // 测试设置属性值
    auto setter = registry.getSetter("Person", "name");
    if (setter) {
        setter->set(&person, Any(std::string("李四")));
        std::cout << "✓ 设置姓名成功，新姓名: " << person.name_ << std::endl;
    }
    
    setter = registry.getSetter("Person", "age");
    if (setter) {
        setter->set(&person, Any(35));
        std::cout << "✓ 设置年龄成功，新年龄: " << person.age_ << std::endl;
    }
}

/**
 * @brief 测试方法调用功能
 */
void testMethodInvocation() {
    std::cout << "\n=== 测试方法调用 ===" << std::endl;
    
    auto& registry = ReflectionRegistry::getInstance();
    Person person("王五", 28);
    
    // 获取所有方法名
    auto methodNames = registry.getMethodNames("Person");
    std::cout << "注册的方法列表:" << std::endl;
    for (const auto& methodName : methodNames) {
        std::cout << "  " << methodName << std::endl;
    }
    
    try {
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
        
    } catch (const std::exception& e) {
        std::cout << "✗ 方法调用失败: " << e.what() << std::endl;
    }
}

/**
 * @brief 测试错误处理
 */
void testErrorHandling() {
    std::cout << "\n=== 测试错误处理 ===" << std::endl;
    
    auto& registry = ReflectionRegistry::getInstance();
    Person person("测试", 25);
    
    try {
        // 测试调用不存在的方法
        registry.invokeMethod("Person", "nonExistentMethod", &person, {});
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获不存在方法的异常: " << e.what() << std::endl;
    }
    
    try {
        // 测试参数数量不匹配
        std::vector<Any> wrongArgs = {Any(1), Any(2)}; // setAge只需要一个参数
        registry.invokeMethod("Person", "setAge", &person, wrongArgs);
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获参数数量不匹配的异常: " << e.what() << std::endl;
    }
    
    try {
        // 测试类型不匹配
        auto setter = registry.getSetter("Person", "age");
        if (setter) {
            setter->set(&person, Any(std::string("不是数字"))); // age应该是int类型
        }
    } catch (const std::exception& e) {
        std::cout << "✓ 正确捕获类型不匹配的异常: " << e.what() << std::endl;
    }
    
    // 测试获取不存在的类名
    std::string className = registry.getClassName<Person>();
    std::cout << "✓ Person类的注册名称: " << className << std::endl;
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "=== C++11反射系统测试程序 ===" << std::endl;
    
    try {
        // 注册反射信息
        registerPersonReflection();
        std::cout << "✓ 反射信息注册完成" << std::endl;
        
        // 执行各项测试
        testObjectCreation();
        testPropertyAccess();
        testMethodInvocation();
        testErrorHandling();
        
        std::cout << "\n=== 所有测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ 程序执行出错: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
