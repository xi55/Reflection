# AGENTS.md

## 项目与目标
- `Evently` 命名空间下的 C++ 运行时反射库。`Reflection.h` 是接口/模板实现，`Reflection.cpp` 是注册中心实现（必须链接，非 header-only），`Any.h` 是类型擦除容器，`IndexSequence.h` 是 C++11 版 `index_sequence`，`ObjectHandle.h` 是对象所有权句柄，`ReflectionError.h` 是统一错误类型。
- `main.cpp` 是演示程序；回归测试在 `tests/reflection_tests.cpp`。`scripts/build-msvc.ps1` 负责 Windows 的 VS + SDK 环境。
- CMake 生成静态库 `evently_reflection`（别名/安装目标 `Evently::Reflection`）、演示程序 `Test` 和测试程序 `ReflectionTests`；支持 install/export 与 CTest。
- 目标：库代码同时兼容 C++11 与 C++17；Linux 用 GCC，Windows 用 MSVC。库头文件中不得使用 C++14/17 专属语法或库设施。

## 构建与验证
- Linux/GCC C++11：
  `cmake -S . -B build/gcc-cxx11 -DEVENTLY_CXX_STANDARD=11 -DCMAKE_BUILD_TYPE=Debug`
  `cmake --build build/gcc-cxx11 -j`
  `ctest --test-dir build/gcc-cxx11 --output-on-failure`
- Linux/GCC C++17：把上面的 `gcc-cxx11` 和 `11` 换成 `gcc-cxx17` 和 `17`。
- Windows/MSVC C++14（C++11 代码的基线）：
  `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-msvc.ps1 -Standard 14`
  C++17 用 `-Standard 17`。构建目录是 `build\msvc-cxx14` / `build\msvc-cxx17`。
- MSVC 没有 `/std:c++11`；CMake 的 `CXX_STANDARD 11` 在 MSVC 上不加 `/std` 参数，实际使用默认 C++14。真正的 C++11 门槛是 GCC 的 `-std=c++11`。
- CMake 变量 `EVENTLY_CXX_STANDARD` 可取 `11/14/17`，默认 `11`；`CXX_EXTENSIONS OFF`。
- 单测：`build\msvc-cxx14\ReflectionTests.exe --test Any`，或 `ctest --test-dir build\msvc-cxx14 -R reflection_ --output-on-failure`。测试组：`Any`、`Fields`、`Methods`、`Factory`、`Metadata`、`Functions`、`Qualifiers`、`Constructors`、`AutoRegistration`、`RegistrationErrors`、`ThreadSafety`，共 250+ 项断言（C++17 额外包含 `noexcept` 用例）。CTest 把核心测试和会冻结注册中心的 `ThreadSafety` 分成两个独立进程，并运行 `JsonSerializationExample`。
- `EVENTLY_BUILD_EXAMPLES` 控制 `JsonSerializationExample`；`examples/consumer` 是独立安装消费方示例，不在主构建中。
- 源文件是无 BOM 的 UTF-8，含中文注释和字符串；MSVC 的 `/utf-8` 已由 CMake 统一加到库和测试，并通过 `evently_reflection` 的 INTERFACE 选项传播给安装后的消费方，不要移除。
- 本机 Windows SDK 在 `D:\Windows Kits\10`，但 SDK 注册表键可能缺失：单独运行 `vcvars64.bat` 只会设置 MSVC 的 `INCLUDE/LIB`，不会设置 UCRT/SDK。使用 `scripts/build-msvc.ps1`，或手动注入 SDK 的 `INCLUDE/LIB/PATH`；Debug 程序还需要 SDK 的 `ucrt` 运行时目录和 MSVC Debug CRT 目录。
- Windows 默认使用静态 MSVC 运行时（Debug `/MTd`，Release `/MT`），所以 `Test.exe`/`ReflectionTests.exe` 可以在普通 PowerShell 中直接运行。若把 `evently_reflection` 链接到 `/MD` 项目，必须统一 `CMAKE_MSVC_RUNTIME_LIBRARY`；安装包只包含本次构建配置，Debug/Release 也要匹配。
- 安装：`cmake --install build/gcc-cxx11 --prefix install`。消费方用 `find_package(Evently CONFIG REQUIRED)`、`target_link_libraries(app PRIVATE Evently::Reflection)`、`#include <Evently/Reflection.h>`。

## 已知陷阱
- `Any` 的存储类型统一 decay。`any_cast<const T>`、`any_cast<const T&>` 是安全的；非 const `T&` 只能从非 const `Any` 取；`any_cast<T&>(const Any&)` 会触发 `static_assert`。不要重新引入引用/cv 的向下转型。
- `Any` 对小型、nothrow-move 且对齐合适的类型使用 16 字节内联存储，其他类型堆分配；支持 `has_value`/`reset`/`emplace`/move。修改 `Any` 时重点验证拷贝、移动、析构和 `any_cast` 指针稳定性。
- 自动注册使用 `EVENTLY_REGISTER_CLASS` + `ClassRegistration<T>`；宏必须在 `.cpp` 中使用，运行时显式调用 `registerAll()`。不要把头文件中的自动注册宏包含到多个 TU，否则会重复注册。
- `getParam` 在编译期拒绝非 const 左值引用和右值引用参数；反射方法参数只能按值或 const 引用传递。
- `registerField<T>(fieldName, field)` 依赖先调用 `registerClassName<T>`；未注册时抛 `ReflectionError(ClassNotFound)`。
- `registerClass` 使用 `ObjectFactorySelector` 选择工厂，避免两个分支同时实例化；无默认构造函数的 T 必须带构造参数注册。字段/方法/构造函数/工厂/类名重复注册默认抛 `ReflectionError(DuplicateRegistration)`。
- 类名有所有权：同一个类名只能属于一个类型，不同类型同名抛 `ReflectionError(ClassNameConflict)`；同一类型可以通过 `registerClass<T>` 注册多个别名类名。
- 注册中心使用“冻结后只读”模型：启动阶段注册，`freeze()` 之后禁止注册；冻结后读操作无锁且可并发。冻结前不要并发注册/查询，也不要在冻结前长期持有 `getSetter` 返回的指针。
- `ReflectionRegistry` 是单例且没有 `clear()`；测试使用不同类名隔离状态，不要重复注册同一签名。
- `createInstance` 返回 `ObjectHandle`，未知类抛 `ReflectionError(ClassNotFound)`；`getSetter` 对缺失字段抛 `FieldNotFound`，对 const 字段抛 `FieldNotWritable`；`getValues`/`getAllValues`/`getMethodNames` 对缺失项抛错；用 `hasClass`/`hasField`/`hasMethod`/`isFieldWritable` 做非抛出查询。
- 方法重载按参数数量和精确 `type_index` 匹配，不做隐式数值转换；`invokeMethod` 对 const 实例只允许 const 方法，非 const 方法会抛 `InvalidInstance`。`nullptr` 实例由 `std::nullptr_t` 重载处理，避免 `void*`/`const void*` 歧义。
- ref-qualified 成员函数：`&`/`const &` 用普通实例，`&&`/`const &&` 必须通过 `RvalueInstance` 传入；`MethodInfo::refQualifier` 记录限定符。
- 静态成员函数和自由函数通过 `registerStaticMethod` / `registerFunction` 注册，用 `invokeFunction` 调用；`invokeMethod` 会跳过 static 候选。自由函数的命名空间名也受类名所有权约束。
- 运行期构造函数用 `registerConstructor<T, Args...>` 注册；`createInstance(className, args)`/`createInstanceWithArgs` 按精确类型匹配。`createInstance(className)` 优先使用 `registerClass` 绑定的工厂，没有工厂时使用无参构造函数。
- `MethodInfo` 包含 `isStatic`、`refQualifier`；`ClassInfo` 包含 `constructors`；修改元数据时同步更新 `getMethods`/`getClassInfo`/测试。
- `noexcept` 重载仅在 C++17 下编译（`EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE`）；C++11/C++14 构建不要引用这些重载。
- `ObjectHandle::get<T>()` 只接受精确类型（忽略 cv），不做基类转换；不匹配返回 `nullptr`。
- 不要在库头文件中引入 `<windows.h>`、`<cxxabi.h>` 等平台头；`main.cpp` 里的控制台 UTF-8 设置只属于演示。
- `main.cpp` 中的测试函数是演示输出，不是回归门槛；改行为时以 `ReflectionTests` 为准。
- 保持 C++11 写法：不用 `std::make_unique`、`if constexpr`、泛型 lambda、`std::optional`/`std::string_view`、`std::shared_mutex`。

## 代码约定
- 注册键是字符串对 `(className, memberName)`；通过单例 `ReflectionRegistry::getInstance()` 注册和查询；库错误统一抛 `ReflectionError`（`std::runtime_error` 派生），不要混用空 `Any`/`nullptr` 表示错误。
- 类型 PascalCase、方法 camelCase、成员尾部下划线；注释和异常信息用中文，与现有代码一致。
- 修改模板代码后至少跑 Windows C++14/C++17；能访问 WSL 时再跑 GCC C++11/C++17。
