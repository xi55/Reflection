#include "Reflection.h"
#include <iostream>
/* todo:
    1. 处理const成员函数
*/
#include "Reflection.h"
#include <iostream>
#include <any>
#include <vector>
#include <string>
#include <exception>

class MyClass 
{
public:
    MyClass() {
        std::cout << "MyClass default constructor\n";
        this->x = 0;
    }

    MyClass(int x) {
        std::cout << "MyClass constructor with param: " << x << "\n";
        this->x = x;
    }

    int getX() {
        return x;
    }

    int add(int y, int z) {
        return x + y + z;
    }

    void setX(int value) {
        x = value;
    }

private:
    int x;
};


class ComplexClass 
{
public:
    // 默认构造函数
    ComplexClass() : value(0), name("default") {
         std::cout << "ComplexClass default constructor\n";
    }
    
    // 带两个参数的构造函数
    ComplexClass(int v, const std::string& n) : value(v), name(n) {
         std::cout << "ComplexClass parameterized constructor: " << v << ", " << n << "\n";
    }
    
    // 更新内部状态，修改数值并拼接字符串
    void update(int delta, const std::string& suffix) {
         value += delta;
         name += suffix;
    }
    
    int getValue() {
         return value;
    }
    
    std::string getName() {
         return name;
    }
    
private:
    int value;
    std::string name;
};

//=================== OverloadClass ===================
// 用于测试方法重载和显式转换
class OverloadClass {
    public:
        OverloadClass() : a(0) {
            std::cout << "OverloadClass default constructor\n";
        }
        // 无参版本
        void foo() {
            std::cout << "OverloadClass foo() called\n";
            a = 10;
        }
        // 带参数版本
        void foo(int x) {
            std::cout << "OverloadClass foo(int) called with x = " << x << "\n";
            a = x;
        }
        int getA() {
            return a;
        }
    private:
        int a;
    };
    
    //=================== VectorClass ===================
    // 用于测试含 STL 容器参数的方法
    class VectorClass {
    public:
        VectorClass() {
            std::cout << "VectorClass default constructor\n";
        }
        void addElement(int x) {
            vec.push_back(x);
        }
        int getElement(size_t index) {
            if (index >= vec.size()) {
                throw std::out_of_range("Index out of range");
            }
            return vec[index];
        }
        size_t size() {
            return vec.size();
        }
    private:
        std::vector<int> vec;
    };
    
    //=================== ExceptionClass ===================
    // 用于测试方法中抛出异常的情况
    class ExceptionClass {
    public:
        ExceptionClass() {
            std::cout << "ExceptionClass constructor\n";
        }
        void riskyMethod(int x) {
            if (x < 0) {
                throw std::runtime_error("Negative value not allowed");
            }
            std::cout << "ExceptionClass::riskyMethod accepted: " << x << std::endl;
        }
        int safeMethod() {
            return 42;
        }
    };


int main() {
    Evently::ReflectionRegistry& registry = Evently::ReflectionRegistry::getInstance();

    // ------------- 测试默认构造版本 -------------
    std::string defaultKey = "MyClassDefault";
    // 注册默认构造函数版本
    registry.registerClass<MyClass>(defaultKey);
    // 注册成员函数
    registry.registerMethod<MyClass, void, int>(defaultKey, "setX", &MyClass::setX);
    registry.registerMethod<MyClass, int>(defaultKey, "getX", &MyClass::getX);
    registry.registerMethod<MyClass, int, int, int>(defaultKey, "add", &MyClass::add);

    // 通过反射创建默认构造的 MyClass 实例
    auto instanceDefault = registry.createInstance(defaultKey);
    if (!instanceDefault) {
        std::cerr << "创建 MyClassDefault 实例失败！\n";
        return -1;
    }

    // ------------- 测试带参构造版本 -------------
    std::string paramKey = "MyClassParam";
    // 传入参数 30 调用带参构造函数
    registry.registerClass<MyClass>(paramKey, 30);
    registry.registerMethod<MyClass, void, int>(paramKey, "setX", &MyClass::setX);
    registry.registerMethod<MyClass, int>(paramKey, "getX", &MyClass::getX);
    registry.registerMethod<MyClass, int, int, int>(paramKey, "add", &MyClass::add);

    auto instanceParam = registry.createInstance(paramKey);
    if (!instanceParam) {
        std::cerr << "创建 MyClassParam 实例失败！\n";
        return -1;
    }

    // ----------------- 对默认构造实例的基本测试 -----------------
    MyClass* objDefault = static_cast<MyClass*>(instanceDefault.get());
    std::cout << "默认构造实例初始 x: " << objDefault->getX() << std::endl; // 应输出 0

    // 调用 setX(42)
    registry.invokeMethod(defaultKey, "setX", objDefault, std::vector<std::any>{42});
    std::any x = registry.invokeMethod(defaultKey, "getX", objDefault, {});
    std::cout << "调用 setX(42) 后 x: " << std::any_cast<int>(x) << std::endl;

    // 调用 add(4, 4)
    std::any res = registry.invokeMethod(defaultKey, "add", objDefault, std::vector<std::any>{4, 4});
    std::cout << "调用 add(4,4) 后结果: " << std::any_cast<int>(res) << std::endl;

    // ----------------- 对带参构造实例的测试 -----------------
    MyClass* objParam = static_cast<MyClass*>(instanceParam.get());
    std::cout << "带参构造实例初始 x: " << objParam->getX() << std::endl; // 应输出 30

    // 调用 setX(100)
    registry.invokeMethod(paramKey, "setX", objParam, std::vector<std::any>{100});
    std::any xParam = registry.invokeMethod(paramKey, "getX", objParam, {});
    std::cout << "调用 setX(100) 后 x: " << std::any_cast<int>(xParam) << std::endl;

    // 调用 add(5, 10)
    std::any addResParam = registry.invokeMethod(paramKey, "add", objParam, std::vector<std::any>{5, 10});
    std::cout << "调用 add(5,10) 后结果: " << std::any_cast<int>(addResParam) << std::endl;

    // ----------------- 其他测试用例 -----------------

    // 1. 测试调用不存在的方法
    try {
        registry.invokeMethod(defaultKey, "nonExistentMethod", objDefault, {});
    } catch (const std::exception& ex) {
        std::cerr << "捕获不存在方法的异常: " << ex.what() << std::endl;
    }

    // 2. 测试创建不存在的类实例
    try {
        auto unknownInstance = registry.createInstance("UnknownClass");
        if (!unknownInstance) {
            std::cerr << "创建 UnknownClass 实例失败，符合预期" << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "捕获未知类实例的异常: " << ex.what() << std::endl;
    }

    // 3. 测试连续调用方法改变对象状态
    std::cout << "\n连续调用方法改变状态测试:" << std::endl;
    registry.invokeMethod(defaultKey, "setX", objDefault, std::vector<std::any>{10});
    std::any newX = registry.invokeMethod(defaultKey, "getX", objDefault, {});
    std::cout << "调用 setX(10) 后 x: " << std::any_cast<int>(newX) << std::endl;

    registry.invokeMethod(defaultKey, "setX", objDefault, std::vector<std::any>{20});
    newX = registry.invokeMethod(defaultKey, "getX", objDefault, {});
    std::cout << "调用 setX(20) 后 x: " << std::any_cast<int>(newX) << std::endl;

    // 4. 测试调用方法时参数数量错误
    try {
        // 调用 add 只传入一个参数（实际需要两个参数）
        registry.invokeMethod(defaultKey, "add", objDefault, std::vector<std::any>{5});
        std::cerr << "错误的参数数量未引发异常！" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "捕获参数数量错误的异常: " << ex.what() << std::endl;
    }

    // 5. 测试调用方法时参数类型错误
    try {
        // 调用 setX 时传入 string 类型（实际需要 int）
        registry.invokeMethod(defaultKey, "setX", objDefault, std::vector<std::any>{std::string("错误类型")});
        std::cerr << "错误的参数类型未引发异常！" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "捕获参数类型错误的异常: " << ex.what() << std::endl;
    }

    // 6. 测试多实例创建，验证各实例状态独立
    auto instance1 = registry.createInstance(defaultKey);
    auto instance2 = registry.createInstance(defaultKey);
    if (instance1 && instance2) {
        MyClass* obj1 = static_cast<MyClass*>(instance1.get());
        MyClass* obj2 = static_cast<MyClass*>(instance2.get());
        registry.invokeMethod(defaultKey, "setX", obj1, std::vector<std::any>{15});
        registry.invokeMethod(defaultKey, "setX", obj2, std::vector<std::any>{25});
        int val1 = std::any_cast<int>(registry.invokeMethod(defaultKey, "getX", obj1, {}));
        int val2 = std::any_cast<int>(registry.invokeMethod(defaultKey, "getX", obj2, {}));
        std::cout << "\n多实例测试: obj1 x=" << val1 << ", obj2 x=" << val2 << std::endl;
    } else {
        std::cerr << "多实例创建失败！" << std::endl;
    }

    // 7. 测试重复注册同一个方法（查看是否覆盖或报错）
    try {
        registry.registerMethod<MyClass, void, int>(defaultKey, "setX", &MyClass::setX);
        std::cout << "\n重复注册方法 'setX' 成功（覆盖或忽略旧注册）" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "\n重复注册方法 'setX' 抛出异常: " << ex.what() << std::endl;
    }

    // 8. 测试在空实例指针上调用方法
    try {
        registry.invokeMethod(defaultKey, "setX", nullptr, std::vector<std::any>{50});
        std::cerr << "\n在空实例上调用方法未引发异常！" << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "\n捕获在空实例调用方法的异常: " << ex.what() << std::endl;
    }

    // ================== ComplexClass 测试 ==================
    // 1. 测试默认构造版本
    std::string complexDefaultKey = "ComplexClassDefault";
    registry.registerClass<ComplexClass>(complexDefaultKey);
    registry.registerMethod<ComplexClass, void, int, const std::string&>(complexDefaultKey, "update", &ComplexClass::update);
    registry.registerMethod<ComplexClass, int>(complexDefaultKey, "getValue", &ComplexClass::getValue);
    registry.registerMethod<ComplexClass, std::string>(complexDefaultKey, "getName", &ComplexClass::getName);

    auto complexInstanceDefault = registry.createInstance(complexDefaultKey);
    if (!complexInstanceDefault) {
        std::cerr << "创建 ComplexClassDefault 实例失败！\n";
        return -1;
    }
    ComplexClass* complexObjDefault = static_cast<ComplexClass*>(complexInstanceDefault.get());
    std::cout << "\nComplexClass 默认实例: value = " 
              << complexObjDefault->getValue() << ", name = " 
              << complexObjDefault->getName() << std::endl;

    // 通过反射调用 update 更新状态
    registry.invokeMethod(complexDefaultKey, "update", complexObjDefault, std::vector<std::any>{10, std::string("_updated")});
    std::any newValue = registry.invokeMethod(complexDefaultKey, "getValue", complexObjDefault, {});
    std::any newName = registry.invokeMethod(complexDefaultKey, "getName", complexObjDefault, {});
    std::cout << "ComplexClass 默认实例更新后: value = " 
              << std::any_cast<int>(newValue) << ", name = " 
              << std::any_cast<std::string>(newName) << std::endl;

    // 2. 测试带参构造版本（传入初始参数）
    std::string complexParamKey = "ComplexClassParam";
    registry.registerClass<ComplexClass>(complexParamKey, 100, std::string("init"));
    registry.registerMethod<ComplexClass, void, int, const std::string&>(complexParamKey, "update", &ComplexClass::update);
    registry.registerMethod<ComplexClass, int>(complexParamKey, "getValue", &ComplexClass::getValue);
    registry.registerMethod<ComplexClass, std::string>(complexParamKey, "getName", &ComplexClass::getName);

    auto complexInstanceParam = registry.createInstance(complexParamKey);
    if (!complexInstanceParam) {
         std::cerr << "创建 ComplexClassParam 实例失败！\n";
         return -1;
    }
    ComplexClass* complexObjParam = static_cast<ComplexClass*>(complexInstanceParam.get());
    std::cout << "\nComplexClass 带参实例初始: value = " 
              << complexObjParam->getValue() << ", name = " 
              << complexObjParam->getName() << std::endl;

    // 连续调用更新方法（模拟链式调用效果）
    registry.invokeMethod(complexParamKey, "update", complexObjParam, std::vector<std::any>{-20, std::string("_minus")});
    std::any updatedValue = registry.invokeMethod(complexParamKey, "getValue", complexObjParam, {});
    std::any updatedName = registry.invokeMethod(complexParamKey, "getName", complexObjParam, {});
    std::cout << "ComplexClass 带参实例第一次更新后: value = " 
              << std::any_cast<int>(updatedValue) << ", name = " 
              << std::any_cast<std::string>(updatedName) << std::endl;

    registry.invokeMethod(complexParamKey, "update", complexObjParam, std::vector<std::any>{50, std::string("_plus")});
    updatedValue = registry.invokeMethod(complexParamKey, "getValue", complexObjParam, {});
    updatedName = registry.invokeMethod(complexParamKey, "getName", complexObjParam, {});
    std::cout << "ComplexClass 带参实例第二次更新后: value = " 
              << std::any_cast<int>(updatedValue) << ", name = " 
              << std::any_cast<std::string>(updatedName) << std::endl;

    // 3. 链式调用测试：连续调用 update 后再获取最新值
    registry.invokeMethod(complexDefaultKey, "update", complexObjDefault, std::vector<std::any>{5, std::string("_chain")});
    int chainedValue = std::any_cast<int>(registry.invokeMethod(complexDefaultKey, "getValue", complexObjDefault, {}));
    std::string chainedName = std::any_cast<std::string>(registry.invokeMethod(complexDefaultKey, "getName", complexObjDefault, {}));
    std::cout << "ComplexClass 默认实例链式调用后: value = " 
              << chainedValue << ", name = " << chainedName << std::endl;

    // ================== 错误情况复杂测试 ==================
    // a. 调用方法时传入错误参数类型
    try {
        // update 应该传入 int 和 string，传入错误类型的参数
        registry.invokeMethod(complexDefaultKey, "update", complexObjDefault, std::vector<std::any>{std::string("错误类型"), std::string("_error")});
        std::cerr << "错误参数类型未触发异常！" << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "\n捕获 ComplexClass update 错误参数类型异常: " << ex.what() << std::endl;
    }

    // b. 调用方法时参数数量错误
    try {
        // getName 不需要参数，传入了额外参数
        registry.invokeMethod(complexDefaultKey, "getName", complexObjDefault, std::vector<std::any>{42});
        std::cerr << "错误参数数量未触发异常！" << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "\n捕获 ComplexClass getName 参数数量错误异常: " << ex.what() << std::endl;
    }

    //---------------- OverloadClass 测试 ----------------
    std::string overloadKey = "OverloadClass";
    registry.registerClass<OverloadClass>(overloadKey);
    // 方法重载时，需显式转换以指定使用哪个重载版本
    registry.registerMethod<OverloadClass, void>(overloadKey, "foo_no_param", 
        static_cast<void (OverloadClass::*)()>(&OverloadClass::foo));
    registry.registerMethod<OverloadClass, void, int>(overloadKey, "foo_int", 
        static_cast<void (OverloadClass::*)(int)>(&OverloadClass::foo));
    registry.registerMethod<OverloadClass, int>(overloadKey, "getA", &OverloadClass::getA);

    auto overloadInstance = registry.createInstance(overloadKey);
    if (!overloadInstance) {
        std::cerr << "Failed to create OverloadClass instance\n";
        return -1;
    }
    OverloadClass* overloadObj = static_cast<OverloadClass*>(overloadInstance.get());
    
    // 调用无参版本
    registry.invokeMethod(overloadKey, "foo_no_param", overloadObj, {});
    int a1 = std::any_cast<int>(registry.invokeMethod(overloadKey, "getA", overloadObj, {}));
    std::cout << "After foo_no_param, getA = " << a1 << std::endl;
    
    // 调用带参版本
    registry.invokeMethod(overloadKey, "foo_int", overloadObj, std::vector<std::any>{25});
    int a2 = std::any_cast<int>(registry.invokeMethod(overloadKey, "getA", overloadObj, {}));
    std::cout << "After foo_int(25), getA = " << a2 << std::endl;
    
    //---------------- VectorClass 测试 ----------------
    std::string vectorKey = "VectorClass";
    registry.registerClass<VectorClass>(vectorKey);
    registry.registerMethod<VectorClass, void, int>(vectorKey, "addElement", &VectorClass::addElement);
    registry.registerMethod<VectorClass, int, size_t>(vectorKey, "getElement", &VectorClass::getElement);
    registry.registerMethod<VectorClass, size_t>(vectorKey, "size", &VectorClass::size);
    
    auto vectorInstance = registry.createInstance(vectorKey);
    if (!vectorInstance) {
        std::cerr << "Failed to create VectorClass instance\n";
        return -1;
    }
    VectorClass* vectorObj = static_cast<VectorClass*>(vectorInstance.get());
    registry.invokeMethod(vectorKey, "addElement", vectorObj, std::vector<std::any>{100});
    registry.invokeMethod(vectorKey, "addElement", vectorObj, std::vector<std::any>{200});
    size_t vecSize = std::any_cast<size_t>(registry.invokeMethod(vectorKey, "size", vectorObj, {}));
    std::cout << "VectorClass size = " << vecSize << std::endl;
    try {
        int elem0 = std::any_cast<int>(registry.invokeMethod(vectorKey, "getElement", vectorObj, std::vector<std::any>{0}));
        int elem1 = std::any_cast<int>(registry.invokeMethod(vectorKey, "getElement", vectorObj, std::vector<std::any>{1}));
        std::cout << "VectorClass elements: " << elem0 << ", " << elem1 << std::endl;
    } catch(const std::exception& ex) {
        std::cerr << "Exception in VectorClass getElement: " << ex.what() << std::endl;
    }
    
    //---------------- ExceptionClass 测试 ----------------
    std::string exceptionKey = "ExceptionClass";
    registry.registerClass<ExceptionClass>(exceptionKey);
    registry.registerMethod<ExceptionClass, void, int>(exceptionKey, "riskyMethod", &ExceptionClass::riskyMethod);
    registry.registerMethod<ExceptionClass, int>(exceptionKey, "safeMethod", &ExceptionClass::safeMethod);
    
    auto exceptionInstance = registry.createInstance(exceptionKey);
    if (!exceptionInstance) {
        std::cerr << "Failed to create ExceptionClass instance\n";
        return -1;
    }
    ExceptionClass* exceptionObj = static_cast<ExceptionClass*>(exceptionInstance.get());
    
    // 调用 safeMethod
    int safeVal = std::any_cast<int>(registry.invokeMethod(exceptionKey, "safeMethod", exceptionObj, {}));
    std::cout << "ExceptionClass safeMethod returns: " << safeVal << std::endl;
    
    // 调用 riskyMethod（正确参数）
    try {
        registry.invokeMethod(exceptionKey, "riskyMethod", exceptionObj, std::vector<std::any>{10});
    } catch(const std::exception& ex) {
        std::cerr << "Unexpected exception in riskyMethod: " << ex.what() << std::endl;
    }
    
    // 调用 riskyMethod（传入负数以触发异常）
    try {
        registry.invokeMethod(exceptionKey, "riskyMethod", exceptionObj, std::vector<std::any>{-5});
        std::cerr << "Expected exception for negative input not thrown!\n";
    } catch(const std::exception& ex) {
        std::cout << "Caught expected exception in riskyMethod: " << ex.what() << std::endl;
    }


    return 0;
}