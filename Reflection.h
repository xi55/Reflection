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

#if defined(__cpp_noexcept_function_type) || \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE 1
#else
#define EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE 0
#endif

namespace Evently
{
    enum class RefQualifier
    {
        None,
        LValue,
        ConstLValue,
        RValue,
        ConstRValue
    };

    class RvalueInstance
    {
    public:
        explicit RvalueInstance(void *instance) noexcept
            : instance_(instance), isConst_(false) {}

        explicit RvalueInstance(const void *instance) noexcept
            : instance_(instance), isConst_(true) {}

        explicit RvalueInstance(std::nullptr_t) noexcept
            : instance_(nullptr), isConst_(true) {}

        const void *get() const noexcept
        {
            return instance_;
        }

        bool isConst() const noexcept
        {
            return isConst_;
        }

    private:
        const void *instance_;
        bool isConst_;
    };

    template <typename ParamType>
    static ParamType getParam(const Any &arg);

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
        virtual bool isStatic() const = 0;
        virtual RefQualifier refQualifier() const = 0;
    };

    class ConstructorInvokerBase
    {
    public:
        virtual ~ConstructorInvokerBase() {}
        virtual ObjectHandle construct(const std::vector<Any> &args) const = 0;
        virtual std::size_t arity() const = 0;
        virtual std::vector<std::type_index> parameterTypes() const = 0;
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
                   const std::type_index &resultType, bool constMethod,
                   bool staticMethod, RefQualifier qualifier)
            : name(methodName), arity(methodArity),
              parameterTypes(parameters), returnType(resultType),
              isConst(constMethod), isStatic(staticMethod),
              refQualifier(qualifier) {}

        std::string name;
        std::size_t arity;
        std::vector<std::type_index> parameterTypes;
        std::type_index returnType;
        bool isConst;
        bool isStatic;
        RefQualifier refQualifier;
    };

    struct ConstructorInfo
    {
        ConstructorInfo(std::size_t constructorArity,
                        const std::vector<std::type_index> &parameters)
            : arity(constructorArity), parameterTypes(parameters) {}

        std::size_t arity;
        std::vector<std::type_index> parameterTypes;
    };

    struct ClassInfo
    {
        explicit ClassInfo(const std::string &className)
            : name(className) {}

        std::string name;
        std::vector<FieldInfo> fields;
        std::vector<MethodInfo> methods;
        std::vector<ConstructorInfo> constructors;
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

    template <typename T, typename ReturnType, typename MethodType,
              bool IsConst, RefQualifier Qualifier, typename... Args>
    class MethodInvokerGeneric : public MethodInvokerBase
    {
    public:
        explicit MethodInvokerGeneric(MethodType method)
            : method_(method) {}

        Any invoke(const void *instance, const std::vector<Any> &args) const override
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
            return invokeImpl(instance, args,
                              typename index_sequence_for<Args...>::type{},
                              std::integral_constant<bool,
                                                     std::is_void<ReturnType>::value>{});
        }

        std::size_t arity() const override
        {
            return sizeof...(Args);
        }

        std::vector<std::type_index> parameterTypes() const override
        {
            return std::vector<std::type_index>{std::type_index(typeid(Args))...};
        }

        std::type_index returnType() const override
        {
            return std::type_index(typeid(ReturnType));
        }

        bool isConst() const override
        {
            return IsConst;
        }

        bool isStatic() const override
        {
            return false;
        }

        RefQualifier refQualifier() const override
        {
            return Qualifier;
        }

    private:
        template <std::size_t... Indexes>
        Any invokeImpl(const void *instance, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::false_type) const
        {
            return Any(call(instance, getParam<Args>(args[Indexes])...));
        }

        template <std::size_t... Indexes>
        Any invokeImpl(const void *instance, const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::true_type) const
        {
            call(instance, getParam<Args>(args[Indexes])...);
            return Any();
        }

        template <typename... CallArgs>
        ReturnType call(const void *instance, CallArgs &&...callArgs) const
        {
            return callImpl(instance,
                            std::integral_constant<RefQualifier, Qualifier>{},
                            std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callImpl(
            const void *instance,
            std::integral_constant<RefQualifier, RefQualifier::None>,
            CallArgs &&...callArgs) const
        {
            return callNone(instance,
                            std::integral_constant<bool, IsConst>{},
                            std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callNone(const void *instance, std::false_type,
                            CallArgs &&...callArgs) const
        {
            T *obj = const_cast<T *>(static_cast<const T *>(instance));
            return (obj->*method_)(std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callNone(const void *instance, std::true_type,
                            CallArgs &&...callArgs) const
        {
            const T *obj = static_cast<const T *>(instance);
            return (obj->*method_)(std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callImpl(
            const void *instance,
            std::integral_constant<RefQualifier, RefQualifier::LValue>,
            CallArgs &&...callArgs) const
        {
            T *obj = const_cast<T *>(static_cast<const T *>(instance));
            return (obj->*method_)(std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callImpl(
            const void *instance,
            std::integral_constant<RefQualifier, RefQualifier::ConstLValue>,
            CallArgs &&...callArgs) const
        {
            const T *obj = static_cast<const T *>(instance);
            return (obj->*method_)(std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callImpl(
            const void *instance,
            std::integral_constant<RefQualifier, RefQualifier::RValue>,
            CallArgs &&...callArgs) const
        {
            T *obj = const_cast<T *>(static_cast<const T *>(instance));
            return (std::move(*obj).*method_)(std::forward<CallArgs>(callArgs)...);
        }

        template <typename... CallArgs>
        ReturnType callImpl(
            const void *instance,
            std::integral_constant<RefQualifier, RefQualifier::ConstRValue>,
            CallArgs &&...callArgs) const
        {
            const T *obj = static_cast<const T *>(instance);
            return (std::move(*obj).*method_)(std::forward<CallArgs>(callArgs)...);
        }

        MethodType method_;
    };

    template <typename T, typename ReturnType, typename... Args>
    using MethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...), false,
        RefQualifier::None, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using ConstMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) const, true,
        RefQualifier::None, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using LValueMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) &, false,
        RefQualifier::LValue, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using ConstLValueMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) const &, true,
        RefQualifier::ConstLValue, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using RValueMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) &&, false,
        RefQualifier::RValue, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using ConstRValueMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) const &&, true,
        RefQualifier::ConstRValue, Args...>;

#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
    template <typename T, typename ReturnType, typename... Args>
    using NoexceptMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) noexcept, false,
        RefQualifier::None, Args...>;

    template <typename T, typename ReturnType, typename... Args>
    using ConstNoexceptMethodInvoker = MethodInvokerGeneric<
        T, ReturnType, ReturnType (T::*)(Args...) const noexcept, true,
        RefQualifier::None, Args...>;
#endif

    template <typename ReturnType, typename... Args>
    class FunctionInvoker : public MethodInvokerBase
    {
    public:
        typedef ReturnType (*FunctionType)(Args...);

        explicit FunctionInvoker(FunctionType function)
            : function_(function) {}

        Any invoke(const void *, const std::vector<Any> &args) const override
        {
            if (args.size() != sizeof...(Args))
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                      "参数数量不匹配");
            }
            return invokeImpl(args, typename index_sequence_for<Args...>::type{},
                              std::integral_constant<bool,
                                                     std::is_void<ReturnType>::value>{});
        }

        std::size_t arity() const override
        {
            return sizeof...(Args);
        }

        std::vector<std::type_index> parameterTypes() const override
        {
            return std::vector<std::type_index>{std::type_index(typeid(Args))...};
        }

        std::type_index returnType() const override
        {
            return std::type_index(typeid(ReturnType));
        }

        bool isConst() const override
        {
            return false;
        }

        bool isStatic() const override
        {
            return true;
        }

        RefQualifier refQualifier() const override
        {
            return RefQualifier::None;
        }

    private:
        template <std::size_t... Indexes>
        Any invokeImpl(const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::false_type) const
        {
            return Any(function_(getParam<Args>(args[Indexes])...));
        }

        template <std::size_t... Indexes>
        Any invokeImpl(const std::vector<Any> &args,
                       index_sequence<Indexes...>, std::true_type) const
        {
            function_(getParam<Args>(args[Indexes])...);
            return Any();
        }

        FunctionType function_;
    };

    template <typename T, typename... Args>
    class ConstructorInvoker : public ConstructorInvokerBase
    {
    public:
        ObjectHandle construct(const std::vector<Any> &args) const override
        {
            if (args.size() != sizeof...(Args))
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                      "构造参数数量不匹配");
            }
            return constructImpl(args, typename index_sequence_for<Args...>::type{});
        }

        std::size_t arity() const override
        {
            return sizeof...(Args);
        }

        std::vector<std::type_index> parameterTypes() const override
        {
            return std::vector<std::type_index>{std::type_index(typeid(Args))...};
        }

    private:
        template <std::size_t... Indexes>
        ObjectHandle constructImpl(const std::vector<Any> &args,
                                   index_sequence<Indexes...>) const
        {
            return ObjectHandle::create<T>(getParam<Args>(args[Indexes])...);
        }
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

    namespace detail
    {
        struct FreeFunctionTag
        {
        };
    }

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

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) &);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) const &);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) &&);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) const &&);

#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) noexcept);

        template <typename T, typename ReturnType, typename... Args>
        void registerMethod(const std::string &className, const std::string &methodName,
                            ReturnType (T::*method)(Args...) const noexcept);
#endif

        template <typename ReturnType, typename... Args>
        void registerFunction(const std::string &className,
                              const std::string &functionName,
                              ReturnType (*function)(Args...));

        template <typename T, typename ReturnType, typename... Args>
        void registerStaticMethod(const std::string &className,
                                  const std::string &methodName,
                                  ReturnType (*method)(Args...));

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
            addFactory(std::type_index(typeid(T)), className,
                       std::unique_ptr<ObjectFactory>(
                           new typename ObjectFactorySelector<
                               T, typename std::decay<Args>::type...>::type(
                               std::forward<Args>(args)...)));
        }

        template <typename T, typename... Args>
        void registerConstructor(const std::string &className)
        {
            addConstructor(std::type_index(typeid(T)), className,
                           std::unique_ptr<ConstructorInvokerBase>(
                               new ConstructorInvoker<T, Args...>()));
        }

        ObjectHandle createInstance(const std::string &className) const;

        ObjectHandle createInstance(const std::string &className,
                                    const std::vector<Any> &args) const;

        template <typename... Args>
        ObjectHandle createInstanceWithArgs(const std::string &className,
                                            Args &&...args) const
        {
            std::vector<Any> values{Any(std::forward<Args>(args))...};
            return createInstance(className, values);
        }

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
                         RvalueInstance instance, const std::vector<Any> &args) const;

        Any invokeMethod(const std::string &className, const std::string &methodName,
                         std::nullptr_t instance, const std::vector<Any> &args) const;

        Any invokeFunction(const std::string &className,
                           const std::string &functionName,
                           const std::vector<Any> &args) const;

        std::set<std::string> getMethodNames(const std::string &className) const;

        std::vector<FieldInfo> getFields(const std::string &className) const;
        std::vector<MethodInfo> getMethods(const std::string &className) const;
        std::vector<ConstructorInfo> getConstructors(const std::string &className) const;
        std::vector<std::string> getClassNames() const;
        ClassInfo getClassInfo(const std::string &className) const;

        void freeze();
        bool isFrozen() const noexcept;

    private:
        enum class InstanceKind
        {
            MutableLValue,
            ConstLValue,
            MutableRValue,
            ConstRValue
        };

        ReflectionRegistry();
        ReflectionRegistry(const ReflectionRegistry &) = delete;
        ReflectionRegistry &operator=(const ReflectionRegistry &) = delete;

        std::unique_lock<std::mutex> readLock() const;
        void ensureMutableLocked() const;
        void ensureClassLocked(const std::string &className);
        void checkClassNameOwnerLocked(const std::type_index &type,
                                       const std::string &className);

        void addClassName(const std::type_index &type, const std::string &className);
        void addField(const std::type_index &type, const std::string &className,
                      const std::string &fieldName,
                      std::unique_ptr<PropertySetterBase> setter);
        void addMethod(const std::type_index &type, const std::string &className,
                       const std::string &methodName,
                       std::unique_ptr<MethodInvokerBase> invoker);
        void addConstructor(const std::type_index &type, const std::string &className,
                            std::unique_ptr<ConstructorInvokerBase> constructor);
        void addFactory(const std::type_index &type, const std::string &className,
                        std::unique_ptr<ObjectFactory> factory);

        Any invokeMethodImpl(const std::string &className,
                             const std::string &methodName,
                             const void *instance, InstanceKind kind,
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

        std::unordered_map<std::string,
                           std::vector<std::unique_ptr<ConstructorInvokerBase>>>
            constructors_;

        std::unordered_map<std::string, std::set<std::string>> methodNames_;
        std::unordered_map<std::type_index, std::string> classNames_;
        std::unordered_map<std::string, std::type_index> classNameOwners_;
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
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new MethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)())
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new MethodInvoker<T, ReturnType>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new ConstMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) &)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new LValueMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const &)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new ConstLValueMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) &&)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new RValueMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const &&)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new ConstRValueMethodInvoker<T, ReturnType, Args...>(method)));
    }

#if EVENTLY_HAS_NOEXCEPT_FUNCTION_TYPE
    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) noexcept)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new NoexceptMethodInvoker<T, ReturnType, Args...>(method)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerMethod(const std::string &className,
                                            const std::string &methodName,
                                            ReturnType (T::*method)(Args...) const noexcept)
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new ConstNoexceptMethodInvoker<T, ReturnType, Args...>(method)));
    }
#endif

    template <typename ReturnType, typename... Args>
    void ReflectionRegistry::registerFunction(const std::string &className,
                                              const std::string &functionName,
                                              ReturnType (*function)(Args...))
    {
        addMethod(std::type_index(typeid(detail::FreeFunctionTag)),
                  className, functionName,
                  std::unique_ptr<MethodInvokerBase>(
                      new FunctionInvoker<ReturnType, Args...>(function)));
    }

    template <typename T, typename ReturnType, typename... Args>
    void ReflectionRegistry::registerStaticMethod(const std::string &className,
                                                  const std::string &methodName,
                                                  ReturnType (*method)(Args...))
    {
        addMethod(std::type_index(typeid(T)), className, methodName,
                  std::unique_ptr<MethodInvokerBase>(
                      new FunctionInvoker<ReturnType, Args...>(method)));
    }

    template <typename T, typename FieldType>
    void ReflectionRegistry::registerField(const std::string &className,
                                           const std::string &fieldName,
                                           FieldType T::*field)
    {
        addField(std::type_index(typeid(T)), className, fieldName,
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
        addField(std::type_index(typeid(T)), className, fieldName,
                 std::unique_ptr<PropertySetterBase>(
                     new PropertySetter<T, FieldType>(field)));
    }
}

#endif // REFLECTION_H
