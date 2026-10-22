#ifndef REFLECTION_H
#define REFLECTION_H
#pragma once

#include "Any.h"
#include "IndexSequence.h"
#include "ObjectHandle.h"
#include "ReflectionError.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Evently
{
    class PropertySetterBase
    {
    public:
        virtual ~PropertySetterBase() {}
        virtual void set(void *instance, const Any &value) = 0;
        virtual Any get(const void *instance) const = 0;
        virtual std::type_index type() const = 0;
        virtual bool isWritable() const = 0;
    };

    class MethodInvokerBase
    {
    public:
        virtual ~MethodInvokerBase() {}
        virtual Any invoke(const void *instance, const std::vector<Any> &args) const = 0;
        virtual std::size_t arity() const = 0;
        virtual std::vector<std::type_index> parameterTypes() const = 0;
        virtual std::type_index returnType() const = 0;
        virtual bool isConst() const = 0;
    };

    class ObjectFactory
    {
    public:
        virtual ~ObjectFactory() {}
        virtual ObjectHandle create() = 0;
    };

    struct PairHash
    {
        std::size_t operator()(const std::pair<std::string, std::string> &pair) const;
    };

    struct PairEqual
    {
        bool operator()(const std::pair<std::string, std::string> &lhs,
                        const std::pair<std::string, std::string> &rhs) const;
    };

    struct FieldInfo
    {
        FieldInfo(const std::string &fieldName, const std::type_index &fieldType,
                  bool fieldWritable)
            : name(fieldName), type(fieldType), writable(fieldWritable) {}

        std::string name;
        std::type_index type;
        bool writable;
    };

    struct MethodInfo
    {
        MethodInfo(const std::string &methodName, std::size_t methodArity,
                   const std::vector<std::type_index> &parameters,
                   const std::type_index &resultType, bool constMethod)
            : name(methodName), arity(methodArity),
              parameterTypes(parameters), returnType(resultType),
              isConst(constMethod) {}

        std::string name;
        std::size_t arity;
        std::vector<std::type_index> parameterTypes;
        std::type_index returnType;
        bool isConst;
    };

    struct ClassInfo
    {
        explicit ClassInfo(const std::string &className)
            : name(className) {}

        std::string name;
        std::vector<FieldInfo> fields;
        std::vector<MethodInfo> methods;
    };

    template <typename T, typename FieldType>
    class PropertySetter : public PropertySetterBase
    {
    public:
        explicit PropertySetter(FieldType T::*field);
        void set(void *instance, const Any &value) override;
        Any get(const void *instance) const override;
        std::type_index type() const override;
        bool isWritable() const override;

    private:
        FieldType T::*field_;

        void setImpl(void *instance, const Any &value, std::true_type);
        void setImpl(void *instance, const Any &value, std::false_type);
    };

    template <typename T, typename ReturnType, typename... Args>
    class MethodInvoker : public MethodInvokerBase
    {
    public:
        using MethodType = ReturnType (T::*)(Args...);

        explicit MethodInvoker(MethodType method);
        Any invoke(const void *instance, const std::vector<Any> &args) const override;
        std::size_t arity() const override;
        std::vector<std::type_index> parameterTypes() const override;
        std::type_index returnType() const override;
        bool isConst() const override;

    private:
        MethodType method_;

        template <std::size_t... Indexes>
        Any invokeImpl(T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>) const;
    };

    template <typename T, typename... Args>
    class MethodInvoker<T, void, Args...> : public MethodInvokerBase
    {
    public:
        using MethodType = void (T::*)(Args...);

        explicit MethodInvoker(MethodType method);
        Any invoke(const void *instance, const std::vector<Any> &args) const override;
        std::size_t arity() const override;
        std::vector<std::type_index> parameterTypes() const override;
        std::type_index returnType() const override;
        bool isConst() const override;

    private:
        MethodType method_;

        template <std::size_t... Indexes>
        Any invokeImpl(T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>) const;
    };

    template <typename T, typename ReturnType, typename... Args>
    class ConstMethodInvoker : public MethodInvokerBase
    {
    public:
        using MethodType = ReturnType (T::*)(Args...) const;

        explicit ConstMethodInvoker(MethodType method);
        Any invoke(const void *instance, const std::vector<Any> &args) const override;
        std::size_t arity() const override;
        std::vector<std::type_index> parameterTypes() const override;
        std::type_index returnType() const override;
        bool isConst() const override;

    private:
        MethodType method_;

        template <std::size_t... Indexes>
        Any invokeImpl(const T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::false_type) const;

        template <std::size_t... Indexes>
        Any invokeImpl(const T *obj, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::true_type) const;
    };

    template <typename T>
    class ObjectFactoryImpl : public ObjectFactory
    {
    public:
        ObjectHandle create() override
        {
            return ObjectHandle::create<T>();
        }
    };

    template <typename T, typename... Args>
    class ObjectFactoryWithParamImpl : public ObjectFactory
    {
    public:
        explicit ObjectFactoryWithParamImpl(Args... args)
            : args_(std::make_tuple(std::forward<Args>(args)...)) {}

        ObjectHandle create() override
        {
            return createImpl(typename index_sequence_for<Args...>::type{});
        }

    private:
        template <std::size_t... Is>
        ObjectHandle createImpl(index_sequence<Is...>)
        {
            return ObjectHandle::create<T>(std::get<Is>(args_)...);
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
            addFactory(className,
                       std::unique_ptr<ObjectFactory>(
                           new typename ObjectFactorySelector<
                               T, typename std::decay<Args>::type...>::type(
                               std::forward<Args>(args)...)));
        }

        ObjectHandle createInstance(const std::string &className) const;

        template <typename T>
        std::string getClassName() const;

        template <typename T>
        bool hasClassName() const;

        bool hasClass(const std::string &className) const;
        bool hasField(const std::string &className, const std::string &fieldName) const;
        bool hasMethod(const std::string &className, const std::string &methodName) const;
        bool isFieldWritable(const std::string &className,
                             const std::string &fieldName) const;

        PropertySetterBase *getSetter(const std::string &className,
                                      const std::string &fieldName) const;

        std::unordered_map<std::string, Any> getAllValues(const std::string &className,
                                                          const void *instance) const;

        Any getValues(const std::string &className, const std::string &fieldName,
                      const void *instance) const;

        Any invokeMethod(const std::string &className, const std::string &methodName,
                         void *instance, const std::vector<Any> &args) const;

        Any invokeMethod(const std::string &className, const std::string &methodName,
                         const void *instance, const std::vector<Any> &args) const;

        Any invokeMethod(const std::string &className, const std::string &methodName,
                         std::nullptr_t instance, const std::vector<Any> &args) const;

        std::set<std::string> getMethodNames(const std::string &className) const;

        std::vector<FieldInfo> getFields(const std::string &className) const;
        std::vector<MethodInfo> getMethods(const std::string &className) const;
        std::vector<std::string> getClassNames() const;
        ClassInfo getClassInfo(const std::string &className) const;

        void freeze();
        bool isFrozen() const noexcept;

    private:
        ReflectionRegistry();
        ReflectionRegistry(const ReflectionRegistry &) = delete;
        ReflectionRegistry &operator=(const ReflectionRegistry &) = delete;

        std::unique_lock<std::mutex> readLock() const;
        void ensureMutableLocked() const;
        void ensureClassLocked(const std::string &className);

        void addClassName(const std::type_index &type, const std::string &className);
        void addField(const std::string &className, const std::string &fieldName,
                      std::unique_ptr<PropertySetterBase> setter);
        void addMethod(const std::string &className, const std::string &methodName,
                       std::unique_ptr<MethodInvokerBase> invoker);
        void addFactory(const std::string &className,
                        std::unique_ptr<ObjectFactory> factory);

        Any invokeMethodImpl(const std::string &className,
                             const std::string &methodName,
                             const void *instance, bool instanceIsConst,
                             const std::vector<Any> &args) const;

        mutable std::mutex mutex_;
        std::atomic<bool> frozen_;

        std::unordered_map<std::pair<std::string, std::string>,
                           std::unique_ptr<PropertySetterBase>,
                           PairHash, PairEqual>
            setters_;

        std::unordered_map<std::pair<std::string, std::string>,
                           std::vector<std::unique_ptr<MethodInvokerBase>>,
                           PairHash, PairEqual>
            methods_;

        std::unordered_map<std::string, std::set<std::string>> methodNames_;
        std::unordered_map<std::type_index, std::string> classNames_;
        std::set<std::string> classNamesSet_;
        std::unordered_map<std::string, std::unique_ptr<ObjectFactory>> factories_;
    };

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

    template <typename T, typename ReturnType, typename... Args>
    inline MethodInvoker<T, ReturnType, Args...>::MethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename ReturnType, typename... Args>
    Any MethodInvoker<T, ReturnType, Args...>::invoke(
        const void *instance, const std::vector<Any> &args) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        if (args.size() != sizeof...(Args))
        {
            throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                  "参数数量不匹配");
        }
        T *obj = const_cast<T *>(static_cast<const T *>(instance));
        return invokeImpl(obj, args, typename index_sequence_for<Args...>::type{});
    }

    template <typename T, typename ReturnType, typename... Args>
    template <std::size_t... Indexes>
    inline Any MethodInvoker<T, ReturnType, Args...>::invokeImpl(
        T *obj, const std::vector<Any> &args, index_sequence<Indexes...>) const
    {
        return Any((obj->*method_)(getParam<Args>(args[Indexes])...));
    }

    template <typename T, typename ReturnType, typename... Args>
    std::size_t MethodInvoker<T, ReturnType, Args...>::arity() const
    {
        return sizeof...(Args);
    }

    template <typename T, typename ReturnType, typename... Args>
    std::vector<std::type_index> MethodInvoker<T, ReturnType, Args...>::parameterTypes() const
    {
        return std::vector<std::type_index>{std::type_index(typeid(Args))...};
    }

    template <typename T, typename ReturnType, typename... Args>
    std::type_index MethodInvoker<T, ReturnType, Args...>::returnType() const
    {
        return std::type_index(typeid(ReturnType));
    }

    template <typename T, typename ReturnType, typename... Args>
    bool MethodInvoker<T, ReturnType, Args...>::isConst() const
    {
        return false;
    }

    template <typename T, typename... Args>
    inline MethodInvoker<T, void, Args...>::MethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename... Args>
    Any MethodInvoker<T, void, Args...>::invoke(
        const void *instance, const std::vector<Any> &args) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        if (args.size() != sizeof...(Args))
        {
            throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                  "参数数量不匹配");
        }
        T *obj = const_cast<T *>(static_cast<const T *>(instance));
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

    template <typename T, typename... Args>
    std::size_t MethodInvoker<T, void, Args...>::arity() const
    {
        return sizeof...(Args);
    }

    template <typename T, typename... Args>
    std::vector<std::type_index> MethodInvoker<T, void, Args...>::parameterTypes() const
    {
        return std::vector<std::type_index>{std::type_index(typeid(Args))...};
    }

    template <typename T, typename... Args>
    std::type_index MethodInvoker<T, void, Args...>::returnType() const
    {
        return std::type_index(typeid(void));
    }

    template <typename T, typename... Args>
    bool MethodInvoker<T, void, Args...>::isConst() const
    {
        return false;
    }

    template <typename T, typename ReturnType, typename... Args>
    inline ConstMethodInvoker<T, ReturnType, Args...>::ConstMethodInvoker(MethodType method)
        : method_(method) {}

    template <typename T, typename ReturnType, typename... Args>
    Any ConstMethodInvoker<T, ReturnType, Args...>::invoke(
        const void *instance, const std::vector<Any> &args) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        if (args.size() != sizeof...(Args))
        {
            throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                  "参数数量不匹配");
        }
        const T *obj = static_cast<const T *>(instance);
        return invokeImpl(obj, args, typename index_sequence_for<Args...>::type{},
                          std::integral_constant<bool, std::is_void<ReturnType>::value>{});
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

    template <typename T, typename ReturnType, typename... Args>
    std::size_t ConstMethodInvoker<T, ReturnType, Args...>::arity() const
    {
        return sizeof...(Args);
    }

    template <typename T, typename ReturnType, typename... Args>
    std::vector<std::type_index> ConstMethodInvoker<T, ReturnType, Args...>::parameterTypes() const
    {
        return std::vector<std::type_index>{std::type_index(typeid(Args))...};
    }

    template <typename T, typename ReturnType, typename... Args>
    std::type_index ConstMethodInvoker<T, ReturnType, Args...>::returnType() const
    {
        return std::type_index(typeid(ReturnType));
    }

    template <typename T, typename ReturnType, typename... Args>
    bool ConstMethodInvoker<T, ReturnType, Args...>::isConst() const
    {
        return true;
    }

    template <typename T, typename FieldType>
    PropertySetter<T, FieldType>::PropertySetter(FieldType T::*field)
        : field_(field) {}

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::set(void *instance, const Any &value)
    {
        setImpl(instance, value,
                std::integral_constant<bool, std::is_const<FieldType>::value>{});
    }

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::setImpl(void *, const Any &, std::true_type)
    {
        throw ReflectionError(ReflectionErrorCode::FieldNotWritable,
                              "const 字段不可写");
    }

    template <typename T, typename FieldType>
    void PropertySetter<T, FieldType>::setImpl(void *instance, const Any &value,
                                               std::false_type)
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        T *obj = static_cast<T *>(instance);
        try
        {
            obj->*field_ = any_cast<FieldType>(value);
        }
        catch (const bad_any_cast &)
        {
            throw ReflectionError(ReflectionErrorCode::FieldTypeMismatch,
                                  "字段类型不匹配");
        }
    }

    template <typename T, typename FieldType>
    Any PropertySetter<T, FieldType>::get(const void *instance) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        const T *obj = static_cast<const T *>(instance);
        return Any(obj->*field_);
    }

    template <typename T, typename FieldType>
    std::type_index PropertySetter<T, FieldType>::type() const
    {
        return std::type_index(typeid(typename std::remove_cv<FieldType>::type));
    }

    template <typename T, typename FieldType>
    bool PropertySetter<T, FieldType>::isWritable() const
    {
        return !std::is_const<FieldType>::value;
    }

    template <typename T>
    void ReflectionRegistry::registerClassName(const std::string &className)
    {
        addClassName(std::type_index(typeid(T)), className);
    }

    template <typename T>
    std::string ReflectionRegistry::getClassName() const
    {
        std::unique_lock<std::mutex> lock = readLock();
        auto it = classNames_.find(std::type_index(typeid(T)));
        if (it == classNames_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册");
        }
        return it->second;
    }

    template <typename T>
    bool ReflectionRegistry::hasClassName() const
    {
        std::unique_lock<std::mutex> lock = readLock();
        return classNames_.find(std::type_index(typeid(T))) != classNames_.end();
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...))
    {
        addMethod(className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new MethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)())
    {
        addMethod(className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new MethodInvoker<T, ReturnType>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const)
    {
        addMethod(className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new ConstMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string &className,
                                           const std::string &fieldName,
                                           FieldType T::*field)
    {
        addField(className, fieldName,
                 std::unique_ptr<PropertySetterBase>(
                     new PropertySetter<T, FieldType>(field)));
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string &fieldName,
                                           FieldType T::*field)
    {
        std::string className;
        {
            std::unique_lock<std::mutex> lock = readLock();
            auto it = classNames_.find(std::type_index(typeid(T)));
            if (it == classNames_.end())
            {
                throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                      "类未注册，无法注册字段");
            }
            className = it->second;
        }
        addField(className, fieldName,
                 std::unique_ptr<PropertySetterBase>(
                     new PropertySetter<T, FieldType>(field)));
    }
}

#endif // REFLECTION_H
