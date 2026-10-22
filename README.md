# 🎯 C++ Reflection System

一个轻量级、现代化的 **C++ 运行时反射系统**，支持：
- 动态注册类、字段、方法
- 动态创建对象实例（含构造参数）
- 运行时访问/设置字段
- 动态方法调用（支持参数传递和返回值）

---

## 📦 项目简介

该反射系统最初使用了 **C++17/20** 的新特性，现已降级兼容至 **C++11**，包括：
- 自定义 `Any` 类：实现类型擦除，统一存储任意字段和方法参数
- `std::unique_ptr`：智能内存管理
- 可变参数模板 + 自定义 `index_sequence`：实现泛型的成员方法调用器
- 类注册、字段注册、方法注册、对象工厂抽象接口设计

> 类似于 Java/C# 中的反射系统，是构建脚本系统、插件系统、序列化框架、可视化编辑器等的基础组件。

---

## ✨ 当前功能

- ✅ 类名注册（自定义类名）
- ✅ 字段注册和字段值访问（支持任意类型）
- ✅ 方法注册与动态调用（支持参数传递和返回值）
- ✅ 动态创建对象（默认构造 & 带参构造）
- ✅ 方法调用参数校验与异常处理
- ✅ 查询类的全部方法名
- ✅ 获取类的所有字段值
- ✅ void 返回类型方法的正确处理
- ✅ 引用参数的安全处理

---

## 🧩 项目结构

```
Reflection/
├── Any.h                 # 自定义 Any 类型实现（替代 std::any）
├── IndexSequence.h       # C++11 兼容的 index_sequence 实现
├── Reflection.h          # 反射系统核心类与接口定义
├── Reflection.cpp        # 接口实现，包括哈希函数、注册中心逻辑等
├── main.cpp             # 使用示例/演示程序
├── tests/reflection_tests.cpp  # 断言式回归测试
├── scripts/build-msvc.ps1      # Windows/MSVC + Windows SDK 构建脚本
├── CMakeLists.txt       # CMake 构建配置
└── README.md            # 项目文档
```

---

## 📌 项目现状

目前系统已经可以用于实际工程中的基础反射功能，包括：

- ✅ 成功支持字段和方法的运行时调用
- ✅ 工厂系统可创建带参和无参对象
- ✅ 支持运行时方法和字段查询
- ✅ 提供统一的注册中心 `ReflectionRegistry` 管理所有类信息
- ✅ C++11 与 C++17 双标准构建
- ✅ Linux/GCC 与 Windows/MSVC 构建
- ✅ 自定义 Any 类型擦除系统，修正 cv/引用转换
- ✅ void 返回类型方法的正确处理
- ✅ 断言式回归测试与 CTest 集成

代码结构清晰，易于扩展，已初步具备实际应用能力。

---

## 📅 更新日志

### v2.2.0 - C++11/C++17 双标准与 Windows/MSVC 支持 (2026-10-22)

#### 🎯 主要更新
- 移除 MSVC 不支持的 `<cxxabi.h>`，移除 C++17 专属 `if constexpr`
- CMake 支持 `EVENTLY_CXX_STANDARD=11/14/17`，新增静态库目标与 CTest
- 新增 Windows/MSVC + Windows SDK 构建脚本
- 新增断言式测试 `ReflectionTests`，覆盖 Any、字段、方法、工厂和错误处理

#### 🐛 修复
- 修正 `Any` 对 `const T`、`const T&`、`T&` 的类型转换
- 修正 const 引用方法参数和 const 字段读取路径
- 修正 `registerClass` 对无默认构造函数类型的实例化问题
- `registerField<T>(fieldName, field)` 未注册类名时改为抛异常
- 类名内部索引改用 `std::type_index`，不再依赖 mangled name

### v2.1.0 - const 方法与 const 语义增强 (2026-01-06)

#### 🎯 主要更新
- 支持注册与调用 `const` 成员函数（新增 `ConstMethodInvoker`）
- const 字段的 setter 自动屏蔽，防止误写
- `Any`/`getParam` 增加 const 字段与 const 引用方法参数的类型匹配

#### 🐛 修复
- 修正 `any_cast` const 情况下的类型匹配错误

#### 📚 文档
- 更新 README、示例与变更记录

### v2.0.0 - C++11 兼容性更新 (2024-01-XX)

#### 🎯 重大更新
- **重构整个代码库以兼容 C++11 标准**
- **替换 `std::any` 为自定义 `Any` 类实现**
- **实现 C++11 兼容的 `index_sequence`**

#### ✨ 新增功能
- 新增 `Any.h` - 自定义类型擦除容器类
- 新增 `IndexSequence.h` - C++11 兼容的编译时整数序列
- 新增详细的中文注释和文档
- 新增完整的测试程序 `main.cpp`

#### 🔧 技术改进
- **void 返回类型处理**：使用模板特化正确处理 void 返回类型的方法
- **引用参数安全处理**：使用 `std::decay` 避免悬空引用问题
- **内存管理优化**：改用显式的 `new`/`delete` 替代 `std::make_unique`
- **类型转换优化**：友元函数访问提升 `any_cast` 性能

#### 🐛 Bug 修复
- 修复 void 返回类型方法调用时的编译错误
- 修复 const 引用参数导致的段错误
- 修复模板实例化时的类型推导问题
- 修复 CMake 构建配置

#### 📚 文档完善
- 添加详细的代码注释（中文）
- 完善 README.md 文档
- 添加使用示例和测试用例
- 添加构建说明

---

### v1.0.0 - 初始版本 (2024-01-XX)

#### ✨ 核心功能
- 基础反射系统实现
- 支持类、字段、方法注册
- 动态对象创建
- 运行时方法调用

#### ⚠️ 已知问题
- 仅支持 C++17/20
- void 返回类型处理存在问题
- 缺乏完整的测试用例

---

## ⚠️ 存在的不足

虽然系统功能完整，但目前仍存在以下问题：

1. **对象所有权仍偏底层**：
   - 使用 `std::unique_ptr<void, void(*)(void*)>` 管理对象内存，`void*` 不具备类型安全
   - 创建出的对象需要调用方自行转换到具体类型

2. **缺乏线程安全保证**：
   - `ReflectionRegistry` 尚未引入锁机制，不能保证多线程注册/调用安全
   - 建议在启动阶段完成注册，之后只读使用

3. **错误处理仍不完全统一**：
   - 大部分查询和调用通过异常报告错误
   - 部分查询仍以空 `Any` 或 `nullptr` 表示“未找到”

---

## 🚀 未来更新方向

计划中的更新包括：

### 🎯 v2.1.0 - const 方法支持
- [x] 实现 `ConstMethodInvoker` 类
- [x] 支持 const 成员方法的注册和调用
- [x] 更新测试用例 / 示例

### 🎯 v2.3.0 - 错误处理优化
- [ ] 改用 `std::optional` 风格接口返回调用结果
- [ ] 统一“未找到”与“类型错误”的返回/异常策略

### 🎯 v2.4.0 - 工厂接口重构
- [ ] 使用 `std::shared_ptr<void>` 替代 `unique_ptr<void, void(*)(void*)>`
- [ ] 引入模板化工厂，简化对象生命周期管理
- [ ] 提升类型安全性

### 🎯 v3.0.0 - 高级特性
- [ ] 使用 `std::mutex` 提供线程安全版本
- [ ] 支持方法重载、`noexcept` 与 ref-qualified 成员函数
- [ ] 提供自动注册宏简化用户使用：
  ```cpp
  REGISTER_CLASS(MyClass)
  REGISTER_METHOD(MyClass, foo)
  REGISTER_FIELD(MyClass, bar)
  ```
- [ ] 增加 install/export 与 CI 构建矩阵

---

## 🛠️ 编译与测试

### 环境要求
- **CMake 3.20+**
- **Linux/GCC**：支持 C++11 和 C++17
- **Windows/MSVC**：Visual Studio 2019 + Windows SDK
- MSVC 没有 `/std:c++11`，使用默认或 `/std:c++14` 作为 C++11 基线；C++17 使用 `/std:c++17`

### Linux/GCC

```bash
cmake -S . -B build/gcc-cxx11 -DEVENTLY_CXX_STANDARD=11 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/gcc-cxx11 -j
ctest --test-dir build/gcc-cxx11 --output-on-failure
./build/gcc-cxx11/Test

cmake -S . -B build/gcc-cxx17 -DEVENTLY_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/gcc-cxx17 -j
ctest --test-dir build/gcc-cxx17 --output-on-failure
```

### Windows/MSVC

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-msvc.ps1 -Standard 14
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-msvc.ps1 -Standard 17
```

脚本会定位 Visual Studio 2019 和 `D:\Windows Kits\10`，向当前进程注入 SDK 的 `INCLUDE`、`LIB`、`PATH`，然后使用 Ninja 配置、构建并运行 CTest。它不会修改系统注册表。如果你的安装路径不同：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-msvc.ps1 `
  -Standard 17 -VsRoot "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community" `
  -SdkRoot "C:\Program Files (x86)\Windows Kits\10"
```

构建目录分别为 `build\msvc-cxx14` 和 `build\msvc-cxx17`。

CMake 默认将 MSVC 运行时设置为静态（Debug 为 `/MTd`，Release 为 `/MT`），因此 `Test.exe` 和 `ReflectionTests.exe` 不依赖额外的 Debug CRT DLL。如果要把 `evently_reflection` 集成到使用 `/MD` 的项目，请把双方的 `CMAKE_MSVC_RUNTIME_LIBRARY` 设置为一致。

### 手动编译

```bash
g++ -std=c++11 -Wall -Wextra -I. main.cpp Reflection.cpp -o Test
g++ -std=c++17 -Wall -Wextra -I. main.cpp Reflection.cpp -o Test
```

### 运行单个测试

```powershell
build\msvc-cxx14\ReflectionTests.exe --test Any
ctest --test-dir build\msvc-cxx14 -R reflection_tests --output-on-failure
```

Linux 下对应运行 `build/gcc-cxx11/ReflectionTests --test Any`。

---

## 📖 使用示例

```cpp
#include "Reflection.h"

class Person {
public:
    std::string name;
    int age;
    
    void setName(const std::string& n) { name = n; }
    std::string getName() const { return name; }
};

int main() {
    auto& registry = Evently::ReflectionRegistry::getInstance();
    
    // 注册类
    registry.registerClassName<Person>("Person");
    registry.registerClass<Person>("Person");
    
    // 注册字段
    registry.registerField<Person>("Person", "name", &Person::name);
    registry.registerField<Person>("Person", "age", &Person::age);
    
    // 注册方法
    registry.registerMethod<Person, void, const std::string&>(
        "Person", "setName", &Person::setName);
    
    // 创建对象
    auto instance = registry.createInstance("Person");
    Person* person = static_cast<Person*>(instance.get());
    
    // 调用方法
    std::vector<Evently::Any> args = {Evently::Any(std::string("John"))};
    registry.invokeMethod("Person", "setName", person, args);
    
    return 0;
}
```

---

## 📄 许可证

本项目采用 MIT 许可证 - 详情请查看 LICENSE 文件

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

*最后更新：2026-10-22*
