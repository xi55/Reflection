# AGENTS.md

## 项目与目标
- `Evently` 命名空间下的 C++ 运行时反射库。`Reflection.h` 是接口和模板实现，`Reflection.cpp` 是注册中心/哈希实现（必须参与链接，非 header-only），`Any.h` 是替代 `std::any` 的类型擦除容器，`IndexSequence.h` 是 C++11 版 `index_sequence`。
- `main.cpp` 是演示程序；真正的回归测试在 `tests/reflection_tests.cpp`。`scripts/build-msvc.ps1` 负责 Windows 的 VS + SDK 环境。
- CMake 生成静态库 `evently_reflection`、演示程序 `Test` 和测试程序 `ReflectionTests`，测试通过 CTest 运行。
- 改造目标：库代码同时兼容 C++11 与 C++17；Linux 用 GCC，Windows 用 MSVC。库头文件中不得使用 C++14/17 专属语法或库设施。

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
- 单测：`build\msvc-cxx14\ReflectionTests.exe --test Any`，或 `ctest --test-dir build\msvc-cxx14 -R reflection_tests --output-on-failure`。测试组：`Any`、`Fields`、`Methods`、`Factory`、`RegistrationErrors`。
- 源文件是无 BOM 的 UTF-8，含中文注释和字符串；MSVC 的 `/utf-8` 已由 CMake 统一加上，不要移除。
- 本机 Windows SDK 在 `D:\Windows Kits\10`，但 SDK 注册表键可能缺失：单独运行 `vcvars64.bat` 只会设置 MSVC 的 `INCLUDE/LIB`，不会设置 UCRT/SDK。使用 `scripts/build-msvc.ps1`，或手动注入 SDK 的 `INCLUDE/LIB/PATH`；Debug 程序还需要 SDK 的 `ucrt` 运行时目录和 MSVC Debug CRT 目录。
- Windows 默认使用静态 MSVC 运行时（Debug `/MTd`，Release `/MT`），所以 `Test.exe`/`ReflectionTests.exe` 可以在普通 PowerShell 中直接运行。若把 `evently_reflection` 链接到 `/MD` 项目，必须统一 `CMAKE_MSVC_RUNTIME_LIBRARY`。
- 没有 install/export；当前只构建库、演示和测试。

## 已知陷阱
- `Any` 的存储类型统一 decay。`any_cast<const T>`、`any_cast<const T&>` 是安全的；非 const `T&` 只能从非 const `Any` 取；`any_cast<T&>(const Any&)` 会触发 `static_assert`。不要重新引入 `Holder<T&>` / `Holder<const T>` 的向下转型。
- `getParam` 在编译期拒绝非 const 左值引用和右值引用参数；反射方法参数只能按值或 const 引用传递。
- `registerField<T>(fieldName, field)` 依赖先调用 `registerClassName<T>`；未注册时会抛 `std::runtime_error`，不会静默注册到空类名。类名内部索引是 `std::type_index`，对外仍使用显式字符串。
- `registerClass` 使用 `ObjectFactorySelector` 选择工厂，避免两个分支同时实例化；无默认构造函数的 T 必须带构造参数注册。
- 注册中心不是线程安全的；应在启动阶段完成注册，之后只读使用，或显式加锁。
- `ReflectionRegistry` 是单例且没有 `clear()`；测试使用不同类名隔离状态。
- `createInstance` 对未知类返回空 `unique_ptr`；`getSetter` 对缺失字段和 const 字段都返回 `nullptr`；`getValues` 缺失时返回空 `Any`；`invokeMethod` 的未知方法/空实例抛异常。不要假设所有查询都抛异常。
- 不要在库头文件中引入 `<windows.h>`、`<cxxabi.h>` 等平台头；`main.cpp` 里的控制台 UTF-8 设置只属于演示。
- `main.cpp` 中的测试函数是演示输出，不是回归门槛；改行为时以 `ReflectionTests` 为准。
- 保持 C++11 写法：不用 `std::make_unique`、`if constexpr`、泛型 lambda、`std::optional`/`std::string_view`。

## 代码约定
- 注册键是字符串对 `(className, memberName)`；通过单例 `ReflectionRegistry::getInstance()` 注册和查询；错误抛 `std::invalid_argument`/`std::runtime_error`。
- 类型 PascalCase、方法 camelCase、成员尾部下划线；注释和异常信息用中文，与现有代码一致。
- 修改模板代码后至少跑 Windows C++14/C++17；能访问 WSL 时再跑 GCC C++11/C++17。
