# 🎯 C++ Reflection System

一个轻量级、现代化的 **C++ 运行时反射系统**，支持：
- 动态注册类、字段、方法
- 动态创建对象实例（含构造参数）
- 运行时访问/设置字段
- 动态方法调用（支持参数传递和返回值）

---

## 📦 项目简介

该反射系统最初使用了 **C++17/20** 的新特性，现已兼容 **C++11 与 C++17**，包括：
- 自定义 `Any` 类：类型擦除，统一存储字段和方法参数
- `ObjectHandle`：基于 `std::shared_ptr` 的类型安全对象句柄
- 可变参数模板 + 自定义 `index_sequence`：泛型成员方法调用器
- 类、字段、方法、对象工厂注册，以及字段/方法元数据
- 初始化后冻结的线程安全模型：冻结后可并发只读访问

> 类似于 Java/C# 中的反射系统，是构建脚本系统、插件系统、序列化框架、可视化编辑器等的基础组件。

---

## ✨ 当前功能

- ✅ 类名注册（自定义类名）
- ✅ 字段注册和字段值访问（支持任意类型）
- ✅ 方法注册与动态调用（支持参数传递和返回值）
- ✅ 动态创建对象（默认构造 & 带参构造），返回类型安全 `ObjectHandle`
- ✅ 统一 `ReflectionError` 错误码与 `hasClass` / `hasField` / `hasMethod` 查询
- ✅ 字段与方法元数据（字段类型、可写性、参数类型、返回类型、const 标记）
- ✅ 方法重载解析（按参数数量和精确类型匹配）
- ✅ const 实例只能调用 const 方法
- ✅ 初始化后 `freeze()`，之后可并发只读访问
- ✅ CMake install/export，支持 `find_package(Evently CONFIG REQUIRED)`
- ✅ C++11/C++17、GCC/MSVC、CTest 与 CI

---

## 🧩 项目结构

```
Reflection/
├── Any.h                 # 自定义 Any 类型实现（替代 std::any）
├── IndexSequence.h       # C++11 兼容的 index_sequence 实现
├── ObjectHandle.h        # 类型安全对象句柄
├── ReflectionError.h     # 统一错误码与 ReflectionError
├── Reflection.h          # 反射系统核心类与接口定义
├── Reflection.cpp        # 注册中心、哈希与查询实现
├── main.cpp              # 使用示例/演示程序
├── tests/reflection_tests.cpp  # 断言式回归测试
├── examples/consumer/          # find_package 消费方示例
├── scripts/build-msvc.ps1      # Windows/MSVC + Windows SDK 构建脚本
├── cmake/EventlyConfig.cmake.in # CMake package 配置模板
├── .github/workflows/ci.yml    # Linux/Windows/消毒器 CI
├── CMakeLists.txt        # CMake 构建与安装配置
├── LICENSE               # MIT 许可证
└── README.md             # 项目文档
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
- ✅ 类型安全 `ObjectHandle`、统一错误码与元数据查询
- ✅ 方法重载、const 实例调用、冻结后并发只读
- ✅ install/export、CI 与 190+ 项断言的回归测试

---

## 📅 更新日志

### v3.0.1 - 稳定性修复 (2026-10-22)

- 不同类型禁止注册同名类名，新增 `ClassNameConflict`
- CTest 拆分 `reflection_core` 与独立的 `reflection_thread_safety`
- 测试 runner 支持 `--exclude`
- 增加 MIT `LICENSE`

### v3.0.0 - 生产级核心 (2026-10-22)

#### 🎯 主要更新
- 新增 `ObjectHandle`，替代 `unique_ptr<void, deleter>` 作为对象所有权句柄
- 新增 `ReflectionError` 错误码体系与 `hasClass` / `hasField` / `hasMethod` / `isFieldWritable`
- 新增字段/方法元数据：`FieldInfo`、`MethodInfo`、`ClassInfo`
- 支持同名方法重载，按参数数量和精确类型解析
- 支持 const 实例调用 const 方法，非 const 方法拒绝 const 实例
- 注册中心新增 `freeze()`：冻结后禁止注册，之后支持并发只读访问
- 重复注册、缺失类/字段/方法、参数错误全部统一为 `ReflectionError`
- CMake install/export，支持 `find_package(Evently CONFIG REQUIRED)`
- 新增 GitHub Actions CI（GCC/Clang/MSVC、C++11/17、ASan/UBSan/TSan）

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

当前版本仍有以下边界：

1. **线程安全模型是“冻结后只读”**：
   - 注册阶段需在启动时完成；`freeze()` 之后不允许再注册
   - 需要运行期动态注册时，应改为 COW 快照模型或在外部加锁

2. **方法解析使用精确类型匹配**：
   - 不进行隐式数值转换；`int` 参数必须传 `Any(int)`
   - 暂不支持 `noexcept`、ref-qualified、静态/自由函数

3. **便利层尚未完成**：
   - 暂无自动注册宏
   - 暂无内置序列化/JSON 支持
   - `Any` 仍以堆分配为主，未做小对象优化

4. **Windows 默认静态 CRT**：
   - 集成到 `/MD` 项目时需要统一 `CMAKE_MSVC_RUNTIME_LIBRARY`

---

## 🚀 后续方向

### 🎯 v3.1.0 - 调用能力扩展
- [ ] 支持 `noexcept`、ref-qualified 成员函数
- [ ] 支持静态函数/自由函数注册
- [ ] 可选的隐式数值转换策略

### 🎯 v3.2.0 - 便利层
- [ ] 自动注册宏（显式 `registerAll()` 优先）
- [ ] 基于字段元数据的 JSON 序列化示例
- [ ] `Any` 小对象优化与 move 优化

### 🎯 v4.0.0 - 动态与分发
- [ ] COW 快照，支持冻结后动态注册
- [ ] 共享库导出宏与 `BUILD_SHARED_LIBS`
- [ ] Doxygen API 文档与更多平台 CI

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

### 安装与集成

```bash
cmake --install build/gcc-cxx11 --prefix install
```

消费方：

```cmake
find_package(Evently CONFIG REQUIRED)
target_link_libraries(app PRIVATE Evently::Reflection)
```

```cpp
#include <Evently/Reflection.h>
```

安装包会提供 `EventlyConfig.cmake`、`EventlyTargets.cmake` 和 `include/Evently/` 头文件。Windows 消费方需要与库使用相同的 MSVC 运行时（默认静态），并保持 Debug/Release 配置一致。仓库内的 `examples/consumer` 是完整的最小消费方示例。

### 手动编译

```bash
g++ -std=c++11 -Wall -Wextra -I. main.cpp Reflection.cpp -o Test
g++ -std=c++17 -Wall -Wextra -I. main.cpp Reflection.cpp -o Test
```

### 运行单个测试

```powershell
build\msvc-cxx14\ReflectionTests.exe --test Any
ctest --test-dir build\msvc-cxx14 -R reflection_ --output-on-failure
```

Linux 下对应运行 `build/gcc-cxx11/ReflectionTests --test Any`。

测试组：`Any`、`Fields`、`Methods`、`Factory`、`Metadata`、`RegistrationErrors`、`ThreadSafety`，共 190+ 项断言。CTest 将核心测试和会冻结注册中心的 `ThreadSafety` 分开进程运行。

---

## 📖 使用示例

```cpp
#include <Evently/Reflection.h>

#include <string>
#include <vector>

class Person {
public:
    std::string name;
    int age = 0;

    void setName(const std::string& n) { name = n; }
    std::string getName() const { return name; }
};

int main() {
    Evently::ReflectionRegistry& registry =
        Evently::ReflectionRegistry::getInstance();

    registry.registerClassName<Person>("Person");
    registry.registerField<Person>("Person", "name", &Person::name);
    registry.registerField<Person>("Person", "age", &Person::age);
    registry.registerMethod<Person, void, const std::string&>(
        "Person", "setName", &Person::setName);
    registry.registerMethod<Person, std::string>(
        "Person", "getName", &Person::getName);
    registry.registerClass<Person>("Person");

    registry.freeze();

    Evently::ObjectHandle instance = registry.createInstance("Person");
    Person* person = instance.get<Person>();
    if (person == nullptr) {
        return 1;
    }

    std::vector<Evently::Any> args = {Evently::Any(std::string("John"))};
    registry.invokeMethod("Person", "setName", person, args);
    Evently::Any name = registry.invokeMethod("Person", "getName", person, {});

    try {
        Evently::PropertySetterBase* setter =
            registry.getSetter("Person", "age");
        setter->set(person, Evently::Any(30));
    } catch (const Evently::ReflectionError& error) {
        return static_cast<int>(error.code()) + 2;
    }

    return Evently::any_cast<std::string>(name) == "John" ? 0 : 1;
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
