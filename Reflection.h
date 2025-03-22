#ifndef REFLECTION_H
#define REFLECTION_H
#pragma once

#include <string>
#include <any>
#include <unordered_map>
#include <memory>
#include <set>
#include <vector>
#include <iostream>
#include <cxxabi.h>

namespace Evently
{
    // 属性设置器基类
    class PropertySetterBase 
    {
    public:
        virtual ~PropertySetterBase() = default;
        virtual void set(void* instance, const std::any& value) = 0;
        virtual std::any get(const void* instance) const = 0;
    };

    // 函数调用器基类
    class MethodInvokerBase 
    {
    public:
        virtual ~MethodInvokerBase() = default;
        virtual std::any invoke(void* instance, const std::vector<std::any>& args) const = 0;
    };

    // 对象工厂基类
    class ObjectFactory 
    {
    public:
        virtual ~ObjectFactory() = default;
        virtual std::unique_ptr<void, void(*)(void*)> create() = 0;
    };


    struct PairHash 
    {
        std::size_t operator() (const std::pair<std::string, std::string>& pair) const;
    };

    struct PairEqual 
    {
        bool operator() (const std::pair<std::string, std::string>& lhs, const std::pair<std::string, std::string>& rhs) const;
    };

    // 成员变量属性设置器
    template <typename T, typename FieldType>
    class PropertySetter : public PropertySetterBase 
    {
    public:
        PropertySetter(FieldType T::*field);
        void set(void* instance, const std::any& value) override;
        std::any get(const void* instance) const override;

    private:
        FieldType T::*field_;
    };

    // 成员函数调用器
    template <typename T, typename ReturnType, typename... Args>
    class MethodInvoker : public MethodInvokerBase 
    {
    public:
        using MethodType = ReturnType (T::*)(Args...);

        MethodInvoker(MethodType method);
        std::any invoke(void* instance, const std::vector<std::any>& args) const override;

    private:
        MethodType method_;
        template <std::size_t... Indexes>
        std::any invokeImpl(T* obj, const std::vector<std::any>& args, std::index_sequence<Indexes...>) const;
    };

    template <typename T, typename... Args>
    class ObjectFactoryImpl : public ObjectFactory 
    {
    public:
        // 默认构造版本
        std::unique_ptr<void, void(*)(void*)> create() override {
            if constexpr (std::is_default_constructible_v<T>) {
                return std::unique_ptr<void, void(*)(void*)>(new T(), [](void* p) { delete static_cast<T*>(p); });
            } else {
                return {nullptr, [](void*){}};
            }
        }
    
    };

    template <typename T, typename... Args>
    class ObjectFactoryWithParamImpl : public ObjectFactory {
    public:
        ObjectFactoryWithParamImpl(Args... args)
            : args_(std::forward<Args>(args)...) {}

        std::unique_ptr<void, void(*)(void*)> create() override {
            // 使用 std::apply 解包 tuple，将参数传递给 T 的构造函数
            T* ptr = std::apply([](auto&&... params) -> T* {
                return new T(std::forward<decltype(params)>(params)...);
            }, args_);
            return std::unique_ptr<void, void(*)(void*)>(
                ptr,
                [](void* p) { delete static_cast<T*>(p); }
            );
        }
        
    private:
        std::tuple<Args...> args_;
    };


    class ReflectionRegistry 
    {
    public:
        static ReflectionRegistry& getInstance();

        template <typename T>
        void registerClassName(const std::string& className);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName, ReturnType (T::*method)(Args...));

        template <typename T, typename ReturnType>
        void registerMethod(const std::string& className, const std::string& methodName, ReturnType (T::*method)());

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string& className, const std::string& methodName, ReturnType (T::*method)(Args...) const);

        template <typename T, typename FieldType>
        void registerField(const std::string& className, const std::string& fieldName, FieldType T::*field);

        template <typename T, typename FieldType>
        void registerField(const std::string& fieldName, FieldType T::*field);

        // 注册可实例化的类
        template <typename T, typename... Args>
        void registerClass(const std::string& className, Args... args) 
        {
            if constexpr (sizeof...(args) == 0)
                factories_[className] = std::make_unique<ObjectFactoryImpl<T>>();
            else
            factories_[className] = std::make_unique<ObjectFactoryWithParamImpl<T, std::decay_t<Args>...>>(std::forward<Args>(args)...);
        }

        // 根据类名创建实例
        template <typename... Args>
        std::unique_ptr<void, void(*)(void*)> createInstance(const std::string& className) const {
            auto it = factories_.find(className);
            if (it != factories_.end()) 
            {
                return it->second->create();
                // if constexpr (sizeof...(args) == 0) {
                //     return it->second->create();  // 如果没有参数，调用无参数的 create()
                // } else {
                //     auto paramFactory = dynamic_cast<ObjectFactoryWithParamImpl<std::decay_t<Args>...>*>(it->second.get());
                //     if (paramFactory) {
                //         return paramFactory->create();  // 如果有参数，调用带参数的 createWithArgs()
                //     }
                // }
            }
            return {nullptr, [](void*){}};  // 返回空指针表示失败
        }


        template <typename T>
        std::string getClassName() const;

        PropertySetterBase* getSetter(const std::string& className, const std::string& fieldName) const;

        std::unordered_map<std::string, std::any> getAllValues(const std::string& className, const void* instance) const;

        std::any getValues(const std::string& className, const std::string& fieldName, const void* instance) const;
        
        std::any invokeMethod(const std::string& className, const std::string& methodName, void* instance, const std::vector<std::any>& args) const;

        std::set<std::string> getMethodNames(const std::string& className) const;


    private:
        ReflectionRegistry();
        ReflectionRegistry(const ReflectionRegistry&) = delete;
        ReflectionRegistry& operator=(const ReflectionRegistry&) = delete;

        // 成员变量设置器的存储，键为 (类名, 变量名)
        std::unordered_map<std::pair<std::string, std::string>, std::unique_ptr<PropertySetterBase>, PairHash, PairEqual> setters_;
        // 成员函数调用器的存储，键为 (类名, 方法名)
        std::unordered_map<std::pair<std::string, std::string>, std::unique_ptr<MethodInvokerBase>, PairHash, PairEqual> methods_;
        // 存储每个类的已注册方法名
        std::unordered_map<std::string, std::set<std::string>> methodNames_;  
        // 存储每个类的已注册类名
        std::unordered_map<std::string, std::string> classNames_;

        std::unordered_map<std::string, std::unique_ptr<ObjectFactory>> factories_;
    };


    template <typename T, typename ReturnType, typename... Args>
    inline MethodInvoker<T, ReturnType, Args...>::MethodInvoker(MethodType method) : method_(method) {}

    template <typename T, typename ReturnType, typename... Args>
    std::any MethodInvoker<T, ReturnType, Args...>::invoke(void *instance, const std::vector<std::any> &args) const
    {
        if (args.size() != sizeof...(Args)) 
        {
            throw std::invalid_argument("Argument count mismatch");
        }
        T* obj = static_cast<T*>(instance);
        return invokeImpl(obj, args, std::index_sequence_for<Args...>{});
    }

    template <typename T, typename ReturnType, typename... Args>
    template <std::size_t... Indexes>
    inline std::any MethodInvoker<T, ReturnType, Args...>::invokeImpl(T *obj, const std::vector<std::any> &args, std::index_sequence<Indexes...>) const
    {
        if constexpr (std::is_void_v<ReturnType>) 
        {
            // 打印预期类型和实际类型
            // auto typeName = [](const char* name) {
            //     int status = -1;
            //     std::unique_ptr<char, void(*)(void*)> res{
            //         abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};
            //     if (status != 0) {
            //         std::cerr << "Demangling failed for: " << name << " with status: " << status << "\n";
            //     }
            //     return (status == 0) ? std::string(res.get()) : std::string(name);
            // };

            // // 使用demangleTypeName来解码类型名的代码
            // ((std::cout << "预期的参数类型: " << typeName(typeid(Args).name()) 
            //         << ", 实际的参数类型: " << typeName(args[Indexes].type().name()) << "\n"), ...);

            // 检查并调用方法
            try {
                (obj->*method_)(std::any_cast<Args>(args[Indexes])...);
            } catch (const std::bad_any_cast& e) {
                std::cerr << "bad_any_cast caught: " << e.what() << "\n";
                throw;
            }
            return {};  // Return empty std::any for void return type
        } 
        else 
        {
            // If the function returns a value, we return it as std::any
            return (obj->*method_)(std::any_cast<Args>(args[Indexes])...);
        }
    }

    template <typename T, typename FieldType>
    PropertySetter<T, FieldType>::PropertySetter(FieldType T::*field) : field_(field) {}

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::set(void* instance, const std::any& value) {
        T* obj = static_cast<T*>(instance);
        if constexpr (std::is_same_v<FieldType, int>) {
            if (value.type() == typeid(int)) {
                obj->*field_ = std::any_cast<int>(value);
            } else {
                throw std::invalid_argument("Invalid type for int field");
            }
        } else if constexpr (std::is_same_v<FieldType, std::string>) {
            if (value.type() == typeid(std::string)) {
                obj->*field_ = std::any_cast<std::string>(value);
            } else {
                throw std::invalid_argument("Invalid type for string field");
            }
        } else {
            throw std::invalid_argument("Unsupported field type");
        }
    }


    template <typename T, typename FieldType>
    std::any PropertySetter<T, FieldType>::get(const void* instance) const {
        const T* obj = static_cast<const T*>(instance);
        return obj->*field_;
    }


    template <typename T>
    void ReflectionRegistry::registerClassName(const std::string& className) {
        classNames_[typeid(T).name()] = className;
    }

    template <typename T>
    std::string ReflectionRegistry::getClassName() const {
        auto it = classNames_.find(typeid(T).name());
        return it != classNames_.end() ? it->second : "unregistered";
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string& className, const std::string& methodName, ReturnType (T::*method)(Args...)) {
        auto key = std::make_pair(className, methodName);
        methods_[key] = std::make_unique<MethodInvoker<T, ReturnType, Args...>>(method);
        methodNames_[className].insert(methodName);  // 添加方法名称到类的方法名集合中
    }

    template <typename T, typename ReturnType>
    void ReflectionRegistry::registerMethod(const std::string& className, const std::string& methodName, ReturnType (T::*method)()) {
        auto key = std::make_pair(className, methodName);
        methods_[key] = std::make_unique<MethodInvoker<T, ReturnType>>(method);
        methodNames_[className].insert(methodName);  // 添加方法名称到类的方法名集合中
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string& className, const std::string& methodName, ReturnType (T::*method)(Args...) const) {
        auto key = std::make_pair(className, methodName);
        methods_[key] = std::make_unique<MethodInvoker<T, ReturnType, Args...>>(method);
        methodNames_[className].insert(methodName);
    }


    

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string& className, const std::string& fieldName, FieldType T::*field) {
        auto key = std::make_pair(className, fieldName);
        setters_[key] = std::make_unique<PropertySetter<T, FieldType>>(field);
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string& fieldName, FieldType T::*field) {
        auto key = std::make_pair(classNames_[typeid(T).name()], fieldName);
        setters_[key] = std::make_unique<PropertySetter<T, FieldType>>(field);
    }
    
}

#endif // REFLECTION_H