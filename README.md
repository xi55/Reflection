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
- 按类分片 COW 注册中心：读操作无锁，支持运行期动态注册

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
- ✅ `noexcept`、完整 ref-qualified（含 `&&`）、静态/自由函数
- ✅ 运行期构造函数注册与参数匹配
- ✅ `registerAll()` + `EVENTLY_REGISTER_CLASS` 自动注册队列
- ✅ `RegistrationBatch` 批量 COW 事务
- ✅ `unregisterClass` / `clear` 生命周期 API
- ✅ 注册中心 Pimpl 实现隐藏与按类分片 COW（不承诺跨工具链 ABI）
- ✅ `PluginRegistration` 插件注册作用域
- ✅ 基于字段元数据的 JSON 序列化示例
- ✅ `Any` 小对象内联存储、move、`has_value`/`reset`/`emplace`
- ✅ const 实例只能调用 const 方法
- ✅ 按类分片 COW 快照：`freeze()` 后仍可动态注册，读操作无锁
- ✅ 静态库/共享库构建（`BUILD_SHARED_LIBS`）
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
├── EventlyExport.h       # 共享库导出宏
├── Reflection.h          # 反射系统核心类与接口定义
├── Reflection.cpp        # 注册中心、哈希与查询实现
├── main.cpp              # 使用示例/演示程序
├── tests/reflection_tests.cpp  # 断言式回归测试
├── examples/consumer/          # find_package 消费方示例
├── examples/json_serialization.cpp # 字段元数据 JSON 序列化示例
├── benchmarks/reflection_benchmark.cpp # 注册/查询/调用/Any 基准
├── scripts/build-msvc.ps1      # Windows/MSVC + Windows SDK 构建脚本
├── cmake/EventlyConfig.cmake.in # CMake package 配置模板
├── docs/Doxyfile.in            # Doxygen 配置模板
├── .github/workflows/ci.yml    # Linux/Windows/共享库/消毒器/文档 CI
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
- ✅ 方法重载、const 实例调用、COW 并发读写
- ✅ `noexcept`、完整 ref-qualified、静态/自由函数、运行期构造函数
- ✅ 自动注册、批量事务、生命周期、插件作用域、按类分片、注册中心 Pimpl 实现隐藏
- ✅ JSON 示例、`Any` 小对象优化、基准、install/export、CI 与 290+ 项断言

---

## 📅 更新日志

### v4.2.0 - 分片、Pimpl 与插件生命周期 (2026-10-22)

- 注册中心改为 Pimpl，隐藏内部 STL/mutex 状态，移除 C4251 抑制
- COW 按类分片：单类写入和元数据查询只处理该类
- 新增 `PluginRegistration`：自动卸载作用域内注册的类，支持 `detach()`
- 批量注册复用类分片，`RegistrationBatch` 成本进一步降低
- 新增插件生命周期测试，并补充 v4.2 分片基准数据
- 精确类型匹配保持不变

### v4.1.0 - 批量 COW 与生命周期 (2026-10-22)

- 新增 `RegistrationBatch`，`registerAll()` 只复制/发布一次 COW 快照
- 批量注册失败回滚，`RegistrationQueue` 异常时恢复队列
- 新增 `unregisterClass()` / `clear()`
- 新增 `EVENTLY_BUILD_BENCHMARKS` 与 `ReflectionBenchmark`
- 新增共享库/Doxygen 发布验证 CI
- 扩展批量事务、生命周期和清理测试

### v4.0.0 - COW、共享库与文档 (2026-10-22)

- 注册中心改为 COW 快照：读取无锁，`freeze()` 后仍支持运行期动态注册
- 注册表值改为 `shared_ptr`，`getSetter` 返回 `std::shared_ptr<PropertySetterBase>`
- 新增 `BUILD_SHARED_LIBS` 支持与 `EVENTLY_API` 导出宏
- 新增 `scripts/build-msvc.ps1 -Shared`
- 新增 Doxygen 文档目标和 CI 文档任务
- CI 增加 Linux/Windows 共享库任务
- `ThreadSafety` 测试改为并发读 + 动态写

### v3.2.0 - 便利层与性能 (2026-10-22)

- 新增自动注册队列：`EVENTLY_REGISTER_CLASS` + `registerAll()`
- 新增 `ClassRegistration<T>` 链式注册辅助类
- 新增基于字段元数据的 JSON 序列化示例 `JsonSerializationExample`
- `Any` 增加小对象内联存储、move、`has_value`/`reset`/`emplace`
- 新增 `AutoRegistration` 测试与 JSON 示例 CTest

### v3.1.0 - 调用能力扩展 (2026-10-22)

- 支持 `noexcept` 成员函数（C++17 条件启用）
- 支持 `&`、`const &`、`&&`、`const &&` ref-qualified 成员函数，新增 `RvalueInstance`
- 支持静态成员函数和自由函数，新增 `invokeFunction`
- 支持运行期构造函数注册与匹配：`registerConstructor`、`createInstance(className, args)`、`createInstanceWithArgs`
- `MethodInfo` 增加 `isStatic`、`refQualifier`；`ClassInfo` 增加构造函数元数据
- 新增 `Functions`、`Qualifiers`、`Constructors` 测试组

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
- 注册中心新增 `freeze()`：发布只读快照（v4.0 起改为 COW，允许后续动态注册）
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

1. **COW 与插件生命周期边界**：
   - 按类分片后，单条注册复制外层类 map + 目标 `ClassData`；批量注册进一步复用类副本
   - `PluginRegistration` 自动卸载作用域内新类，但外部持有的 `ObjectHandle`/setter/`Any` 必须在插件卸载前销毁
   - Pimpl 稳定类布局，但公开签名仍含 `std::string`/`shared_ptr`/`vector`，不承诺跨工具链 ABI
   - `getSetter` 返回 `shared_ptr`，调用方可以安全持有

2. **方法/构造解析使用精确类型匹配**：
   - 不进行隐式数值转换；`int` 参数必须传 `Any(int)`
   - 构造参数同样按精确 `type_index` 匹配

3. **便利层边界**：
   - 自动注册宏应在 `.cpp` 中使用，并由用户显式调用 `registerAll()`
   - JSON 仅作为序列化示例，不含反序列化或通用协议支持
   - `Any` 小对象内联只覆盖小型、nothrow-move 类型，大对象仍堆分配

4. **Windows 运行时选择**：
   - 静态库构建默认使用静态 CRT；共享库构建使用动态 CRT
   - 消费方需要与库保持相同的 Debug/Release 配置和运行时

---

## 🚀 后续方向

### 🎯 v4.3.0 - 大规模与分发
- [ ] persistent/immutable 外层类索引，降低大规模类表的写入成本
- [ ] 完整 C ABI 与跨工具链二进制兼容（如确有需求）
- [ ] 插件句柄/对象生命周期追踪与更严格卸载策略
- [ ] 性能基准与文档持续完善

---

## 🛠️ 编译与测试

### 环境要求
- **CMake 3.20+**
- **Linux/GCC**：支持 C++11 和 C++17
- **Windows/MSVC**：Visual Studio 2019 + Windows SDK
- MSVC 没有 `/std:c++11`，使用默认或 `/std:c++14` 作为 C++11 基线；C++17 使用 `/std:c++17`
- CMake 选项默认值：`EVENTLY_BUILD_EXAMPLES=ON`、`EVENTLY_BUILD_BENCHMARKS=OFF`、`EVENTLY_BUILD_DOCS=OFF`；测试由 `BUILD_TESTING` 控制（CTest 默认开启）

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

静态库构建默认将 MSVC 运行时设置为静态（Debug 为 `/MTd`，Release 为 `/MT`），因此 `Test.exe` 和 `ReflectionTests.exe` 不依赖额外的 Debug CRT DLL。共享库构建（`BUILD_SHARED_LIBS=ON`）使用动态 CRT，并由构建脚本注入运行库路径。

共享库构建：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-msvc.ps1 -Standard 17 -Shared
```

Linux 共享库：

```bash
cmake -S . -B build/gcc-shared -DBUILD_SHARED_LIBS=ON -DEVENTLY_CXX_STANDARD=17
cmake --build build/gcc-shared -j
ctest --test-dir build/gcc-shared --output-on-failure
```

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

安装包会提供 `EventlyConfig.cmake`、`EventlyTargets.cmake`、共享库/静态库和 `include/Evently/` 头文件。Windows 消费方需要与库保持相同的 Debug/Release 配置和运行时；静态库 Debug 消费方需传 `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug`（Release 用 `MultiThreaded`），共享库消费方保持默认动态 CRT，并把安装目录的 `bin` 加入 `PATH`。仓库内的 `examples/consumer` 是完整的最小消费方示例。

Doxygen 文档：

```bash
cmake -S . -B build/docs -DEVENTLY_BUILD_DOCS=ON -DBUILD_TESTING=OFF
cmake --build build/docs --target docs
```

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

测试组：`Any`、`Fields`、`Methods`、`Factory`、`Metadata`、`Functions`、`Qualifiers`、`Constructors`、`AutoRegistration`、`RegistrationBatch`、`Lifecycle`、`PluginLifecycle`、`RegistrationErrors`、`ThreadSafety`、`Clear`，共 290+ 项断言（C++14 291 项、C++17 294 项，C++17 额外包含 `noexcept` 用例）。CTest 将核心测试和会执行并发读写的 `ThreadSafety` 分开进程运行，并运行 `JsonSerializationExample`。

v4.2 分片基准（WSL GCC 17 Release，100 个类）：逐条注册约 `1.14 ms`，批量注册约 `0.131 ms`，查询约 `0.023 ms`。相对 v4.1 的全局快照，逐条注册约从 `4.33 ms` 降至 `1.14 ms`，批量注册约从 `0.159 ms` 降至 `0.131 ms`，查询约从 `0.147 ms` 降至 `0.023 ms`。

---

## 📖 使用示例

```cpp
#include <Evently/Reflection.h>

#include <string>
#include <vector>

class Person {
public:
    Person() {}
    Person(const std::string& n, int a) : name(n), age(a) {}

    std::string name;
    int age = 0;

    void setName(const std::string& n) { name = n; }
    std::string getName() const { return name; }
    static int addStatic(int a, int b) { return a + b; }
    int consume() && { return age; }
};

int freeAdd(int a, int b) { return a + b; }

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
        auto setter = registry.getSetter("Person", "age");
        setter->set(person, Evently::Any(30));
    } catch (const Evently::ReflectionError& error) {
        return static_cast<int>(error.code()) + 2;
    }

    return Evently::any_cast<std::string>(name) == "John" ? 0 : 1;
}
```

### 静态函数、自由函数与运行期构造

```cpp
registry.registerStaticMethod<Person, int, int, int>(
    "Person", "addStatic", &Person::addStatic);
registry.registerFunction<int, int, int>(
    "Math", "add", &freeAdd);

Any sum = registry.invokeFunction("Math", "add", {Any(2), Any(3)});

registry.registerConstructor<Person>("Person");
registry.registerConstructor<Person, const std::string &, int>("Person");
ObjectHandle person = registry.createInstanceWithArgs(
    "Person", std::string("John"), 30);
```

`createInstance(className)` 优先使用 `registerClass` 绑定的工厂；没有工厂时使用无参构造函数。

自动注册：

```cpp
EVENTLY_REGISTER_CLASS(Person, "Person")
{
    Evently::ClassRegistration<Person> registration("Person");
    registration.constructor<>();
    registration.field("name", &Person::name);
    registration.field("age", &Person::age);
    registration.method("setName", &Person::setName);
}

int main() {
    Evently::ReflectionRegistry& registry =
        Evently::ReflectionRegistry::getInstance();
    registry.registerAll();
    registry.freeze();
}
```

宏应放在 `.cpp` 中；注册函数先进入队列，`registerAll()` 由用户显式执行，避免静态初始化阶段抛异常。JSON 序列化示例见 `examples/json_serialization.cpp`。

批量注册：

```cpp
{
    Evently::RegistrationBatch batch(registry);
    registry.registerClass<Person>("Person");
    registry.registerField<Person>("Person", "name", &Person::name);
    registry.registerField<Person>("Person", "age", &Person::age);
    batch.commit();
}
```

生命周期：

```cpp
registry.unregisterClass("Person");
registry.clear();
```

插件作用域：

```cpp
{
    Evently::PluginRegistration plugin(registry, "MyPlugin");
    Evently::RegistrationBatch batch(registry);
    registry.registerClassName<PluginType>("PluginType");
    registry.registerField<PluginType>("PluginType", "value", &PluginType::value);
    batch.commit();
    // plugin 析构时自动 unregisterClass("PluginType")
    // plugin.detach(); // 如需保留注册，由调用方保证插件代码生命周期
}
```

`PluginRegistration` 是线程局部作用域，同一线程不能嵌套；它只允许注册新类，尝试修改宿主已有类会抛 `ReflectionError(InvalidArgument)`。`detach()` 会保留作用域内已注册的类，此后其生命周期由调用方负责。

宿主必须在销毁 `PluginRegistration` 和卸载插件 DLL 前停止插件线程，并销毁所有引用插件类型的 `ObjectHandle`、setter `shared_ptr` 和 `Any`。

对 `&&` / `const &&` 成员函数使用 `RvalueInstance`：

```cpp
Person* raw = person.get<Person>();
Any result = registry.invokeMethod(
    "Person", "consume", RvalueInstance(raw), {});
```

---

## 📄 许可证

本项目采用 MIT 许可证 - 详情请查看 LICENSE 文件

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

*最后更新：2026-10-22*
