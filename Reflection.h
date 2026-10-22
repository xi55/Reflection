#ifndef REFLECTION_H
#define REFLECTION_H
#pragma once

#include "Any.h"
#include "IndexSequence.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <memory>
#include <set>
#include <vector>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace Evently
{

    /**
     * @brief 属性设置器基类
     */
    class PropertySetterBase
    {
    public:
        virtual ~PropertySetterBase() = default;
        virtual void set(void *instance, const Any &value) = 0;
        virtual Any get(const void *instance) const = 0;
    };

    /**
     * @brief 方法调用器基类
     */
    class MethodInvokerBase
    {
    public:
        virtual ~MethodInvokerBase() = default;
        virtual Any invoke(void *instance, const std::vector<Any> &args) const = 0;
    };

    /**
     * @brief 对象工厂基类
     */
    class ObjectFactory
    {
    public:
        virtual ~ObjectFactory() = default;
        virtual std::unique_ptr<void, void (*)(void *)> create() = 0;
    };

    /**
     * @brief 字符串对哈希函数
     */
    struct PairHash
    {
        std::size_t operator()(const std::pair<std::string, std::string> &pair) const;
    };

    /**
     * @brief 字符串对相等比较函数
     */
    struct PairEqual
    {
        bool operator()(const std::pair<std::string, std::string> &lhs,
                        const std::pair<std::string, std::string> &rhs) const;
    };

    /**
     * @brief 成员变量属性设置器模板类
     */
    template <typename T, typename FieldType>
    class PropertySetter : public PropertySetterBase
    {
    public:
        PropertySetter(FieldType T::*field);
        void set(void *instance, const Any &value) override;
        Any get(const void *instance) const override;

    private:
        FieldType T::*field_;

        void setImpl(void *instance, const Any &value, std::true_type);
        void setImpl(void *instance, const Any &value, std::false_type);
    };

    /**
     * @brief 成员函数调用器模板类（非void返回类型）
     */
    template <typename T, typename ReturnType, typename... Args>
    class MethodInvoker : public MethodInvokerBase
    {
    public:
        using MethodType = ReturnType (T::*)(Args...);

        MethodInvoker(MethodType method);
        Any invoke(void *instance, const std::vector<Any> &args) const override;

    private:
        MethodType method_;

        template <std::size_t... Indexes>
        Any invokeImpl(T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>) const;
    };

    /**
     * @brief void返回类型的方法调用器特化
     */
    template <typename T, typename... Args>
    class MethodInvoker<T, void, Args...> : public MethodInvokerBase
    {
    public:
        using MethodType = void (T::*)(Args...);

        MethodInvoker(MethodType method);
        Any invoke(void *instance, const std::vector<Any> &args) const override;

    private:
        MethodType method_;

    template <std::size_t... Indexes>
    Any invokeImpl(T *obj, const std::vector<Any> &args,
               index_sequence<Indexes...>) const;
    };

    /**
     * @brief const成员函数调用器模板类
     */
    template <typename T, typename ReturnType, typename... Args>
    class ConstMethodInvoker : public MethodInvokerBase
    {
    public:
        using MethodType = ReturnType (T::*)(Args...) const;

        ConstMethodInvoker(MethodType method);
        Any invoke(void *instance, const std::vector<Any> &args) const override;

    private:
        MethodType method_;

        template <std::size_t... Indexes>
        Any invokeImpl(const T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::false_type) const;

        template <std::size_t... Indexes>
        Any invokeImpl(const T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::true_type) const;
    };

    /**
     * @brief 默认构造对象工厂实现
     */
    template <typename T>
    class ObjectFactoryImpl : public ObjectFactory
    {
    public:
        std::unique_ptr<void, void (*)(void *)> create() override
        {
            return std::unique_ptr<void, void (*)(void *)>(
                new T(),
                [](void *p)
                { delete static_cast<T *>(p); });
        }
    };

    /**
     * @brief 带参数构造对象工厂实现
     */
    template <typename T, typename... Args>
    class ObjectFactoryWithParamImpl : public ObjectFactory
    {
    public:
        ObjectFactoryWithParamImpl(Args... args)
            : args_(std::make_tuple(std::forward<Args>(args)...)) {}

        std::unique_ptr<void, void (*)(void *)> create() override
        {
            return createImpl(typename index_sequence_for<Args...>::type{});
        }

    private:
        template <std::size_t... Is>
        std::unique_ptr<void, void (*)(void *)> createImpl(index_sequence<Is...>)
        {
            T *ptr = new T(std::get<Is>(args_)...);
            return std::unique_ptr<void, void (*)(void *)>(
                ptr,
                [](void *p)
                { delete static_cast<T *>(p); });
        }

        std::tuple<Args...> args_;
    };

    template <typename T, typename... Args>
    struct ObjectFactorySelector
    {
        typedef ObjectFactoryWithParamImpl<T, Args...> type;
    };

    template <typename T>
    struct ObjectFactorySelector<T>
    {
        typedef ObjectFactoryImpl<T> type;
    };

    /**
     * @brief 反射注册表类（单例模式）
     */
    class ReflectionRegistry
    {
    public:
        static ReflectionRegistry &getInstance();

        template <typename T>
        void registerClassName(const std::string &className);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...));

        template <typename T, typename ReturnType>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)());

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) const);

        template <typename T, typename FieldType>
        void registerField(const std::string &className, const std::string &fieldName,
                           FieldType T::*field);

        template <typename T, typename FieldType>
        void registerField(const std::string &fieldName, FieldType T::*field);

        template <typename T, typename... Args>
        void registerClass(const std::string &className, Args... args)
        {
            static_assert(sizeof...(Args) != 0 || std::is_default_constructible<T>::value,
                          "default-constructed registration requires a default constructor");
            if (className.empty())
            {
                throw std::invalid_argument("类名不能为空");
            }
            factories_[className] = std::unique_ptr<ObjectFactory>(
                new typename ObjectFactorySelector<
                    T, typename std::decay<Args>::type...>::type(
                    std::forward<Args>(args)...));
        }

        template <typename... Args>
        std::unique_ptr<void, void (*)(void *)> createInstance(const std::string &className) const
        {
            auto it = factories_.find(className);
            if (it != factories_.end())
            {
                return it->second->create();
            }
            return {nullptr, [](void *) {}};
        }

        template <typename T>
        std::string getClassName() const;

        PropertySetterBase *getSetter(const std::string &className,
                                      const std::string &fieldName) const;

        std::unordered_map<std::string, Any> getAllValues(const std::string &className,
                                                          const void *instance) const;

        Any getValues(const std::string &className, const std::string &fieldName,
                      const void *instance) const;

        Any invokeMethod(const std::string &className, const std::string &methodName,
                         void *instance, const std::vector<Any> &args) const;

        std::set<std::string> getMethodNames(const std::string &className) const;

    private:
        ReflectionRegistry();
        ReflectionRegistry(const ReflectionRegistry &) = delete;
        ReflectionRegistry &operator=(const ReflectionRegistry &) = delete;

        std::unordered_map<std::pair<std::string, std::string>,
                           std::unique_ptr<PropertySetterBase>,
                           PairHash, PairEqual>
            setters_;

        std::unordered_map<std::pair<std::string, std::string>,
                           std::unique_ptr<MethodInvokerBase>,
                           PairHash, PairEqual>
            methods_;

        // 标记字段是否可写（const 字段不可写，但仍可通过 get 读取）
        std::unordered_map<std::pair<std::string, std::string>, bool, PairHash, PairEqual>
            setterWritable_;

        std::unordered_map<std::string, std::set<std::string>> methodNames_;
        std::unordered_map<std::type_index, std::string> classNames_;
        std::unordered_map<std::string, std::unique_ptr<ObjectFactory>> factories_;
    };

    // ReflectionRegistry 模板方法实现
    // 参数获取辅助函数 - 处理引用类型
    template <typename ParamType>
    static ParamType getParam(const Any &arg)
    {
        static_assert(!std::is_lvalue_reference<ParamType>::value ||
                          std::is_const<typename std::remove_reference<ParamType>::type>::value,
                      "non-const reference parameters are not supported");
        static_assert(!std::is_rvalue_reference<ParamType>::value,
                      "rvalue reference parameters are not supported");
        return any_cast<ParamType>(arg);
    }

    // 非void返回类型的实现
    template <typename T, typename ReturnType, typename... Args>
    inline MethodInvoker<T, ReturnType, Args...>::MethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename ReturnType, typename... Args>
    Any MethodInvoker<T, ReturnType, Args...>::invoke(void *instance,
                                                      const std::vector<Any> &args) const
    {
        if (args.size() != sizeof...(Args))
        {
            throw std::invalid_argument("参数数量不匹配");
        }
        T *obj = static_cast<T *>(instance);
        return invokeImpl(obj, args, typename index_sequence_for<Args...>::type{});
    }

    template <typename T, typename ReturnType, typename... Args>
    template <std::size_t... Indexes>
    inline Any MethodInvoker<T, ReturnType, Args...>::invokeImpl(
        T *obj, const std::vector<Any> &args, index_sequence<Indexes...>) const
    {
        return Any((obj->*method_)(getParam<Args>(args[Indexes])...));
    }

    // void返回类型的特化实现
    template <typename T, typename... Args>
    inline MethodInvoker<T, void, Args...>::MethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename... Args>
    Any MethodInvoker<T, void, Args...>::invoke(void *instance,
                                                const std::vector<Any> &args) const
    {
        if (args.size() != sizeof...(Args))
        {
            throw std::invalid_argument("参数数量不匹配");
        }
        T *obj = static_cast<T *>(instance);
        return invokeImpl(obj, args, typename index_sequence_for<Args...>::type{});
    }

    template <typename T, typename... Args>
    template <std::size_t... Indexes>
    inline Any MethodInvoker<T, void, Args...>::invokeImpl(
        T *obj, const std::vector<Any> &args, index_sequence<Indexes...>) const
    {
        (obj->*method_)(getParam<Args>(args[Indexes])...);
        return Any();
    }

    template <typename T, typename ReturnType, typename... Args>
    template <std::size_t... Indexes>
    inline Any ConstMethodInvoker<T, ReturnType, Args...>::invokeImpl(
        const T *obj, const std::vector<Any> &args, index_sequence<Indexes...>,
        std::false_type) const
    {
        return Any((obj->*method_)(getParam<Args>(args[Indexes])...));
    }

    template <typename T, typename ReturnType, typename... Args>
    template <std::size_t... Indexes>
    inline Any ConstMethodInvoker<T, ReturnType, Args...>::invokeImpl(
        const T *obj, const std::vector<Any> &args, index_sequence<Indexes...>,
        std::true_type) const
    {
        (obj->*method_)(getParam<Args>(args[Indexes])...);
        return Any();
    }

    // PropertySetter 实现
    template <typename T, typename FieldType>
    PropertySetter<T, FieldType>::PropertySetter(FieldType T::*field) : field_(field) {}

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::set(void *instance, const Any &value)
    {
        setImpl(instance, value,
                std::integral_constant<bool, std::is_const<FieldType>::value>{});
    }

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::setImpl(void *, const Any &, std::true_type)
    {
        throw std::invalid_argument("PropertySetter: Cannot set value of const field");
    }

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::setImpl(void *instance, const Any &value,
                                               std::false_type)
    {
        if (instance == nullptr)
        {
            throw std::invalid_argument("实例指针不能为空");
        }
        T *obj = static_cast<T *>(instance);
        try
        {
            obj->*field_ = any_cast<FieldType>(value);
        }
        catch (const bad_any_cast &)
        {
            throw std::invalid_argument("PropertySetter: Invalid type for field");
        }
    }

    template <typename T, typename FieldType>
    Any PropertySetter<T, FieldType>::get(const void *instance) const
    {
        if (instance == nullptr)
        {
            throw std::invalid_argument("实例指针不能为空");
        }
        const T *obj = static_cast<const T *>(instance);
        return Any(obj->*field_);
    }

    // ReflectionRegistry 模板方法实现
    template <typename T>
    void ReflectionRegistry::registerClassName(const std::string &className)
    {
        if (className.empty())
        {
            throw std::invalid_argument("类名不能为空");
        }
        classNames_[std::type_index(typeid(T))] = className;
    }

    template <typename T>
    std::string ReflectionRegistry::getClassName() const
    {
        auto it = classNames_.find(std::type_index(typeid(T)));
        return it != classNames_.end() ? it->second : "unregistered";
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...))
    {
        if (className.empty() || methodName.empty())
        {
            throw std::invalid_argument("类名和方法名不能为空");
        }
        auto key = std::make_pair(className, methodName);

        methods_[key] = std::unique_ptr<MethodInvokerBase>(
            new MethodInvoker<T, ReturnType, Args...>(method));
        methodNames_[className].insert(methodName);
    }

    template <typename T, typename ReturnType>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)())
    {
        if (className.empty() || methodName.empty())
        {
            throw std::invalid_argument("类名和方法名不能为空");
        }
        auto key = std::make_pair(className, methodName);
        methods_[key] = std::unique_ptr<MethodInvokerBase>(
            new MethodInvoker<T, ReturnType>(method));
        methodNames_[className].insert(methodName);
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const)
    {
        if (className.empty() || methodName.empty())
        {
            throw std::invalid_argument("类名和方法名不能为空");
        }
        auto key = std::make_pair(className, methodName);
        methods_[key] = std::unique_ptr<MethodInvokerBase>(
            new ConstMethodInvoker<T, ReturnType, Args...>(method));
        methodNames_[className].insert(methodName);
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string &className,
                                           const std::string &fieldName,
                                           FieldType T::*field)
    {
        if (className.empty() || fieldName.empty())
        {
            throw std::invalid_argument("类名和字段名不能为空");
        }
        auto key = std::make_pair(className, fieldName);
        setterWritable_[key] = !std::is_const<FieldType>::value;
        setters_[key] = std::unique_ptr<PropertySetterBase>(
            new PropertySetter<T, FieldType>(field));
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string &fieldName,
                                           FieldType T::*field)
    {
        if (fieldName.empty())
        {
            throw std::invalid_argument("字段名不能为空");
        }
        auto it = classNames_.find(std::type_index(typeid(T)));
        if (it == classNames_.end())
        {
            throw std::runtime_error("类未注册，无法注册字段: " + fieldName);
        }
        auto key = std::make_pair(it->second, fieldName);
        setterWritable_[key] = !std::is_const<FieldType>::value;
        setters_[key] = std::unique_ptr<PropertySetterBase>(
            new PropertySetter<T, FieldType>(field));
    }

    template <typename T, typename ReturnType, typename... Args>
    inline ConstMethodInvoker<T, ReturnType, Args...>::ConstMethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename ReturnType, typename... Args>
    inline Any ConstMethodInvoker<T, ReturnType, Args...>::invoke(void *instance, const std::vector<Any> &args) const
    {
        if (args.size() != sizeof...(Args))
            throw std::invalid_argument("参数数量不匹配");
        const T *obj = static_cast<const T *>(instance);
        return invokeImpl(obj, args, typename index_sequence_for<Args...>::type{},
                          std::integral_constant<bool, std::is_void<ReturnType>::value>{});
    }
}

#endif // REFLECTION_H