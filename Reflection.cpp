#include "Reflection.h"

#include <functional>

namespace Evently
{
    namespace detail
    {
        RegistrationQueue &RegistrationQueue::getInstance()
        {
            static RegistrationQueue instance;
            return instance;
        }

        void RegistrationQueue::add(void (*function)())
        {
            std::lock_guard<std::mutex> lock(mutex_);
            functions_.push_back(function);
        }

        void RegistrationQueue::runAll()
        {
            std::vector<void (*)()> functions;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                functions.swap(functions_);
            }
            for (std::size_t index = 0; index < functions.size(); ++index)
            {
                functions[index]();
            }
        }
    }

    std::size_t PairHash::operator()(const std::pair<std::string, std::string> &pair) const
    {
        auto hash1 = std::hash<std::string>{}(pair.first);
        auto hash2 = std::hash<std::string>{}(pair.second);
        return hash1 ^ (hash2 << 1);
    }

    bool PairEqual::operator()(const std::pair<std::string, std::string> &lhs,
                               const std::pair<std::string, std::string> &rhs) const
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }

    ReflectionRegistry &ReflectionRegistry::getInstance()
    {
        static ReflectionRegistry instance;
        return instance;
    }

    ReflectionRegistry::ReflectionRegistry()
        : data_(new RegistryData()), frozen_(false)
    {
    }

    std::shared_ptr<const ReflectionRegistry::RegistryData>
    ReflectionRegistry::snapshot() const
    {
        return std::atomic_load(&data_);
    }

    std::shared_ptr<ReflectionRegistry::RegistryData>
    ReflectionRegistry::copyData() const
    {
        std::shared_ptr<const RegistryData> current = std::atomic_load(&data_);
        return std::shared_ptr<RegistryData>(new RegistryData(*current));
    }

    void ReflectionRegistry::publish(const std::shared_ptr<RegistryData> &data)
    {
        std::atomic_store(&data_, std::shared_ptr<const RegistryData>(data));
    }

    void ReflectionRegistry::ensureClass(RegistryData &data,
                                         const std::string &className)
    {
        data.classNamesSet.insert(className);
    }

    void ReflectionRegistry::checkClassNameOwner(
        RegistryData &data, const std::type_index &type,
        const std::string &className)
    {
        auto owner = data.classNameOwners.find(className);
        if (owner != data.classNameOwners.end() && owner->second != type)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNameConflict,
                                  "类名已属于其他类型: " + className);
        }
        if (owner == data.classNameOwners.end())
        {
            data.classNameOwners.insert(std::make_pair(className, type));
        }
    }

    void ReflectionRegistry::addClassName(const std::type_index &type,
                                          const std::string &className)
    {
        if (className.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名不能为空");
        }
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::shared_ptr<RegistryData> next = copyData();
        auto it = next->classNames.find(type);
        if (it != next->classNames.end() && it->second != className)
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "该类型已注册为其他类名");
        }
        checkClassNameOwner(*next, type, className);
        next->classNames[type] = className;
        ensureClass(*next, className);
        publish(next);
    }

    void ReflectionRegistry::addField(
        const std::type_index &type, const std::string &className,
        const std::string &fieldName,
        std::shared_ptr<PropertySetterBase> setter)
    {
        if (className.empty() || fieldName.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名和字段名不能为空");
        }
        if (!setter)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "字段设置器不能为空");
        }
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::shared_ptr<RegistryData> next = copyData();
        checkClassNameOwner(*next, type, className);
        auto key = std::make_pair(className, fieldName);
        if (next->setters.find(key) != next->setters.end())
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "字段重复注册");
        }
        next->setters[key] = setter;
        ensureClass(*next, className);
        publish(next);
    }

    void ReflectionRegistry::addMethod(
        const std::type_index &type, const std::string &className,
        const std::string &methodName,
        std::shared_ptr<MethodInvokerBase> invoker)
    {
        if (className.empty() || methodName.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名和方法名不能为空");
        }
        if (!invoker)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "方法调用器不能为空");
        }
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::shared_ptr<RegistryData> next = copyData();
        checkClassNameOwner(*next, type, className);
        auto key = std::make_pair(className, methodName);
        std::vector<std::shared_ptr<MethodInvokerBase>> &candidates =
            next->methods[key];
        const std::vector<std::type_index> parameters = invoker->parameterTypes();
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const MethodInvokerBase &existing = *candidates[index];
            if (existing.isConst() == invoker->isConst() &&
                existing.isStatic() == invoker->isStatic() &&
                existing.refQualifier() == invoker->refQualifier() &&
                existing.arity() == invoker->arity() &&
                existing.parameterTypes() == parameters)
            {
                throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                      "方法签名重复注册");
            }
        }
        candidates.push_back(invoker);
        next->methodNames[className].insert(methodName);
        ensureClass(*next, className);
        publish(next);
    }

    void ReflectionRegistry::addConstructor(
        const std::type_index &type, const std::string &className,
        std::shared_ptr<ConstructorInvokerBase> constructor)
    {
        if (className.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名不能为空");
        }
        if (!constructor)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "构造函数不能为空");
        }
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::shared_ptr<RegistryData> next = copyData();
        checkClassNameOwner(*next, type, className);
        std::vector<std::shared_ptr<ConstructorInvokerBase>> &candidates =
            next->constructors[className];
        const std::vector<std::type_index> parameters =
            constructor->parameterTypes();
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const ConstructorInvokerBase &existing = *candidates[index];
            if (existing.arity() == constructor->arity() &&
                existing.parameterTypes() == parameters)
            {
                throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                      "构造函数重复注册");
            }
        }
        candidates.push_back(constructor);
        ensureClass(*next, className);
        publish(next);
    }

    void ReflectionRegistry::addFactory(
        const std::type_index &type, const std::string &className,
        std::shared_ptr<ObjectFactory> factory)
    {
        if (className.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名不能为空");
        }
        if (!factory)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "对象工厂不能为空");
        }
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::shared_ptr<RegistryData> next = copyData();
        checkClassNameOwner(*next, type, className);
        if (next->factories.find(className) != next->factories.end())
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "类工厂重复注册");
        }
        next->factories[className] = factory;
        ensureClass(*next, className);
        publish(next);
    }

    void ReflectionRegistry::registerAll()
    {
        detail::RegistrationQueue::getInstance().runAll();
    }

    void ReflectionRegistry::freeze()
    {
        frozen_.store(true, std::memory_order_release);
    }

    bool ReflectionRegistry::isFrozen() const noexcept
    {
        return frozen_.load(std::memory_order_acquire);
    }

    bool ReflectionRegistry::hasClass(const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        return data->classNamesSet.find(className) != data->classNamesSet.end();
    }

    bool ReflectionRegistry::hasField(const std::string &className,
                                      const std::string &fieldName) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            return false;
        }
        return data->setters.find(std::make_pair(className, fieldName)) !=
               data->setters.end();
    }

    bool ReflectionRegistry::hasMethod(const std::string &className,
                                       const std::string &methodName) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            return false;
        }
        auto it = data->methods.find(std::make_pair(className, methodName));
        return it != data->methods.end() && !it->second.empty();
    }

    bool ReflectionRegistry::isFieldWritable(const std::string &className,
                                             const std::string &fieldName) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->setters.find(std::make_pair(className, fieldName));
        if (it == data->setters.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        return it->second->isWritable();
    }

    ObjectHandle ReflectionRegistry::createInstance(
        const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }

        auto factoryIt = data->factories.find(className);
        if (factoryIt != data->factories.end())
        {
            return factoryIt->second->create();
        }

        auto constructorIt = data->constructors.find(className);
        if (constructorIt != data->constructors.end())
        {
            for (std::size_t index = 0; index < constructorIt->second.size();
                 ++index)
            {
                if (constructorIt->second[index]->arity() == 0)
                {
                    return constructorIt->second[index]->construct(
                        std::vector<Any>());
                }
            }
        }
        throw ReflectionError(ReflectionErrorCode::ConstructorNotFound,
                              "未找到构造函数: " + className);
    }

    ObjectHandle ReflectionRegistry::createInstance(
        const std::string &className, const std::vector<Any> &args) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->constructors.find(className);
        if (it == data->constructors.end())
        {
            throw ReflectionError(ReflectionErrorCode::ConstructorNotFound,
                                  "未找到构造函数: " + className);
        }

        std::shared_ptr<ConstructorInvokerBase> selected;
        bool arityMatched = false;
        bool typeMatched = false;
        for (std::size_t index = 0; index < it->second.size(); ++index)
        {
            const ConstructorInvokerBase &candidate = *it->second[index];
            if (candidate.arity() != args.size())
            {
                continue;
            }
            arityMatched = true;

            const std::vector<std::type_index> parameters =
                candidate.parameterTypes();
            bool matches = true;
            for (std::size_t argIndex = 0; argIndex < args.size(); ++argIndex)
            {
                if (parameters[argIndex] !=
                    std::type_index(args[argIndex].type()))
                {
                    matches = false;
                    break;
                }
            }
            if (!matches)
            {
                continue;
            }
            typeMatched = true;
            if (selected)
            {
                throw ReflectionError(ReflectionErrorCode::AmbiguousConstructor,
                                      "构造函数重载不明确: " + className);
            }
            selected = it->second[index];
        }

        if (!selected)
        {
            if (!arityMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                      "构造参数数量不匹配");
            }
            if (!typeMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentTypeMismatch,
                                      "构造参数类型不匹配");
            }
            throw ReflectionError(ReflectionErrorCode::ConstructorNotFound,
                                  "未找到构造函数: " + className);
        }
        return selected->construct(args);
    }

    std::shared_ptr<PropertySetterBase> ReflectionRegistry::getSetter(
        const std::string &className, const std::string &fieldName) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->setters.find(std::make_pair(className, fieldName));
        if (it == data->setters.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        if (!it->second->isWritable())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotWritable,
                                  "字段不可写: " + className + "::" + fieldName);
        }
        return it->second;
    }

    std::unordered_map<std::string, Any> ReflectionRegistry::getAllValues(
        const std::string &className, const void *instance) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }

        std::vector<std::pair<std::string, std::shared_ptr<PropertySetterBase>>>
            fields;
        for (auto it = data->setters.begin(); it != data->setters.end(); ++it)
        {
            if (it->first.first == className)
            {
                fields.push_back(std::make_pair(it->first.second, it->second));
            }
        }

        std::unordered_map<std::string, Any> values;
        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            values[fields[index].first] = fields[index].second->get(instance);
        }
        return values;
    }

    Any ReflectionRegistry::getValues(const std::string &className,
                                      const std::string &fieldName,
                                      const void *instance) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        auto it = data->setters.find(std::make_pair(className, fieldName));
        if (it == data->setters.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        return it->second->get(instance);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         void *instance,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(className, methodName, instance,
                                InstanceKind::MutableLValue, args);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         const void *instance,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(className, methodName, instance,
                                InstanceKind::ConstLValue, args);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         RvalueInstance instance,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(
            className, methodName, instance.get(),
            instance.isConst() ? InstanceKind::ConstRValue
                               : InstanceKind::MutableRValue,
            args);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         std::nullptr_t,
                                         const std::vector<Any> &args) const
    {
        (void)className;
        (void)methodName;
        (void)args;
        throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                              "实例指针不能为空");
    }

    Any ReflectionRegistry::invokeMethodImpl(const std::string &className,
                                             const std::string &methodName,
                                             const void *instance,
                                             InstanceKind kind,
                                             const std::vector<Any> &args) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }

        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->methods.find(std::make_pair(className, methodName));
        if (it == data->methods.end())
        {
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "方法未注册: " + className + "::" + methodName);
        }

        bool hasNonStatic = false;
        for (std::size_t index = 0; index < it->second.size(); ++index)
        {
            if (!it->second[index]->isStatic())
            {
                hasNonStatic = true;
                break;
            }
        }
        if (!hasNonStatic)
        {
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "未找到成员方法: " + className + "::" +
                                      methodName);
        }

        std::shared_ptr<MethodInvokerBase> selected;
        int bestRank = 0;
        bool arityMatched = false;
        bool typeMatched = false;
        bool qualifierMismatch = false;
        bool ambiguous = false;

        for (std::size_t index = 0; index < it->second.size(); ++index)
        {
            const MethodInvokerBase &candidate = *it->second[index];
            if (candidate.isStatic())
            {
                continue;
            }
            if (candidate.arity() != args.size())
            {
                continue;
            }
            arityMatched = true;

            const std::vector<std::type_index> parameters =
                candidate.parameterTypes();
            bool matches = true;
            for (std::size_t argIndex = 0; argIndex < args.size(); ++argIndex)
            {
                if (parameters[argIndex] !=
                    std::type_index(args[argIndex].type()))
                {
                    matches = false;
                    break;
                }
            }
            if (!matches)
            {
                continue;
            }
            typeMatched = true;

            const bool isConstMethod = candidate.isConst();
            const RefQualifier qualifier = candidate.refQualifier();
            int rank = -1;
            switch (kind)
            {
            case InstanceKind::MutableLValue:
                if (qualifier == RefQualifier::LValue && !isConstMethod)
                {
                    rank = 0;
                }
                else if (qualifier == RefQualifier::None && !isConstMethod)
                {
                    rank = 1;
                }
                else if (qualifier == RefQualifier::ConstLValue &&
                         isConstMethod)
                {
                    rank = 2;
                }
                else if (qualifier == RefQualifier::None && isConstMethod)
                {
                    rank = 3;
                }
                break;
            case InstanceKind::ConstLValue:
                if (qualifier == RefQualifier::ConstLValue && isConstMethod)
                {
                    rank = 0;
                }
                else if (qualifier == RefQualifier::None && isConstMethod)
                {
                    rank = 1;
                }
                break;
            case InstanceKind::MutableRValue:
                if (qualifier == RefQualifier::RValue && !isConstMethod)
                {
                    rank = 0;
                }
                else if (qualifier == RefQualifier::None && !isConstMethod)
                {
                    rank = 1;
                }
                else if (qualifier == RefQualifier::ConstRValue &&
                         isConstMethod)
                {
                    rank = 2;
                }
                else if (qualifier == RefQualifier::None && isConstMethod)
                {
                    rank = 3;
                }
                else if (qualifier == RefQualifier::ConstLValue &&
                         isConstMethod)
                {
                    rank = 4;
                }
                break;
            case InstanceKind::ConstRValue:
                if (qualifier == RefQualifier::ConstRValue && isConstMethod)
                {
                    rank = 0;
                }
                else if (qualifier == RefQualifier::None && isConstMethod)
                {
                    rank = 1;
                }
                else if (qualifier == RefQualifier::ConstLValue &&
                         isConstMethod)
                {
                    rank = 2;
                }
                break;
            }

            if (rank < 0)
            {
                qualifierMismatch = true;
                continue;
            }
            if (!selected || rank < bestRank)
            {
                selected = it->second[index];
                bestRank = rank;
            }
            else if (rank == bestRank)
            {
                ambiguous = true;
            }
        }

        if (ambiguous)
        {
            throw ReflectionError(ReflectionErrorCode::AmbiguousMethod,
                                  "方法重载不明确: " + className + "::" +
                                      methodName);
        }
        if (!selected)
        {
            if (!arityMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                      "参数数量不匹配");
            }
            if (!typeMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentTypeMismatch,
                                      "参数类型不匹配");
            }
            if (qualifierMismatch)
            {
                throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                      "实例的 const/值类别与成员函数限定不匹配");
            }
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "方法未注册: " + className + "::" + methodName);
        }

        try
        {
            return selected->invoke(instance, args);
        }
        catch (const bad_any_cast &error)
        {
            throw ReflectionError(ReflectionErrorCode::ArgumentTypeMismatch,
                                  error.what());
        }
    }

    Any ReflectionRegistry::invokeFunction(
        const std::string &className, const std::string &functionName,
        const std::vector<Any> &args) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->methods.find(std::make_pair(className, functionName));
        if (it == data->methods.end())
        {
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "函数未注册: " + className + "::" +
                                      functionName);
        }

        bool hasStatic = false;
        for (std::size_t index = 0; index < it->second.size(); ++index)
        {
            if (it->second[index]->isStatic())
            {
                hasStatic = true;
                break;
            }
        }
        if (!hasStatic)
        {
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "未找到静态函数: " + className + "::" +
                                      functionName);
        }

        std::shared_ptr<MethodInvokerBase> selected;
        bool arityMatched = false;
        bool typeMatched = false;
        for (std::size_t index = 0; index < it->second.size(); ++index)
        {
            const MethodInvokerBase &candidate = *it->second[index];
            if (!candidate.isStatic())
            {
                continue;
            }
            if (candidate.arity() != args.size())
            {
                continue;
            }
            arityMatched = true;

            const std::vector<std::type_index> parameters =
                candidate.parameterTypes();
            bool matches = true;
            for (std::size_t argIndex = 0; argIndex < args.size(); ++argIndex)
            {
                if (parameters[argIndex] !=
                    std::type_index(args[argIndex].type()))
                {
                    matches = false;
                    break;
                }
            }
            if (!matches)
            {
                continue;
            }
            typeMatched = true;
            if (selected)
            {
                throw ReflectionError(ReflectionErrorCode::AmbiguousMethod,
                                      "函数重载不明确: " + className + "::" +
                                          functionName);
            }
            selected = it->second[index];
        }

        if (!selected)
        {
            if (!arityMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentCountMismatch,
                                      "参数数量不匹配");
            }
            if (!typeMatched)
            {
                throw ReflectionError(ReflectionErrorCode::ArgumentTypeMismatch,
                                      "参数类型不匹配");
            }
            throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                  "未找到静态函数: " + className + "::" +
                                      functionName);
        }

        try
        {
            return selected->invoke(nullptr, args);
        }
        catch (const bad_any_cast &error)
        {
            throw ReflectionError(ReflectionErrorCode::ArgumentTypeMismatch,
                                  error.what());
        }
    }

    std::set<std::string> ReflectionRegistry::getMethodNames(
        const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = data->methodNames.find(className);
        if (it != data->methodNames.end())
        {
            return it->second;
        }
        return std::set<std::string>();
    }

    std::vector<FieldInfo> ReflectionRegistry::getFields(
        const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<FieldInfo> fields;
        for (auto it = data->setters.begin(); it != data->setters.end(); ++it)
        {
            if (it->first.first == className)
            {
                fields.push_back(FieldInfo(it->first.second,
                                           it->second->type(),
                                           it->second->isWritable()));
            }
        }
        return fields;
    }

    std::vector<MethodInfo> ReflectionRegistry::getMethods(
        const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<MethodInfo> methods;
        for (auto it = data->methods.begin(); it != data->methods.end(); ++it)
        {
            if (it->first.first != className)
            {
                continue;
            }
            for (std::size_t index = 0; index < it->second.size(); ++index)
            {
                const MethodInvokerBase &invoker = *it->second[index];
                methods.push_back(MethodInfo(it->first.second,
                                             invoker.arity(),
                                             invoker.parameterTypes(),
                                             invoker.returnType(),
                                             invoker.isConst(),
                                             invoker.isStatic(),
                                             invoker.refQualifier()));
            }
        }
        return methods;
    }

    std::vector<ConstructorInfo> ReflectionRegistry::getConstructors(
        const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<ConstructorInfo> constructors;
        auto it = data->constructors.find(className);
        if (it != data->constructors.end())
        {
            for (std::size_t index = 0; index < it->second.size(); ++index)
            {
                const ConstructorInvokerBase &constructor = *it->second[index];
                constructors.push_back(ConstructorInfo(
                    constructor.arity(), constructor.parameterTypes()));
            }
        }
        return constructors;
    }

    std::vector<std::string> ReflectionRegistry::getClassNames() const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        return std::vector<std::string>(data->classNamesSet.begin(),
                                        data->classNamesSet.end());
    }

    ClassInfo ReflectionRegistry::getClassInfo(const std::string &className) const
    {
        std::shared_ptr<const RegistryData> data = snapshot();
        if (data->classNamesSet.find(className) == data->classNamesSet.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }

        ClassInfo info(className);
        for (auto it = data->setters.begin(); it != data->setters.end(); ++it)
        {
            if (it->first.first == className)
            {
                info.fields.push_back(FieldInfo(it->first.second,
                                                it->second->type(),
                                                it->second->isWritable()));
            }
        }
        for (auto it = data->methods.begin(); it != data->methods.end(); ++it)
        {
            if (it->first.first != className)
            {
                continue;
            }
            for (std::size_t index = 0; index < it->second.size(); ++index)
            {
                const MethodInvokerBase &invoker = *it->second[index];
                info.methods.push_back(MethodInfo(it->first.second,
                                                  invoker.arity(),
                                                  invoker.parameterTypes(),
                                                  invoker.returnType(),
                                                  invoker.isConst(),
                                                  invoker.isStatic(),
                                                  invoker.refQualifier()));
            }
        }
        auto constructorIt = data->constructors.find(className);
        if (constructorIt != data->constructors.end())
        {
            for (std::size_t index = 0; index < constructorIt->second.size();
                 ++index)
            {
                const ConstructorInvokerBase &constructor =
                    *constructorIt->second[index];
                info.constructors.push_back(ConstructorInfo(
                    constructor.arity(), constructor.parameterTypes()));
            }
        }
        return info;
    }
}
