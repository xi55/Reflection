#include "Reflection.h"

#include <functional>

namespace Evently
{
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
        : frozen_(false)
    {
    }

    std::unique_lock<std::mutex> ReflectionRegistry::readLock() const
    {
        if (frozen_.load(std::memory_order_acquire))
        {
            return std::unique_lock<std::mutex>();
        }
        return std::unique_lock<std::mutex>(mutex_);
    }

    void ReflectionRegistry::ensureMutableLocked() const
    {
        if (frozen_.load(std::memory_order_acquire))
        {
            throw ReflectionError(ReflectionErrorCode::RegistryFrozen,
                                  "注册中心已冻结，不能再注册");
        }
    }

    void ReflectionRegistry::ensureClassLocked(const std::string &className)
    {
        classNamesSet_.insert(className);
    }

    void ReflectionRegistry::addClassName(const std::type_index &type,
                                          const std::string &className)
    {
        if (className.empty())
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "类名不能为空");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        ensureMutableLocked();
        auto it = classNames_.find(type);
        if (it != classNames_.end() && it->second != className)
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "该类型已注册为其他类名");
        }
        classNames_[type] = className;
        ensureClassLocked(className);
    }

    void ReflectionRegistry::addField(const std::string &className,
                                      const std::string &fieldName,
                                      std::unique_ptr<PropertySetterBase> setter)
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
        std::lock_guard<std::mutex> lock(mutex_);
        ensureMutableLocked();
        auto key = std::make_pair(className, fieldName);
        if (setters_.find(key) != setters_.end())
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "字段重复注册");
        }
        setters_[key] = std::move(setter);
        ensureClassLocked(className);
    }

    void ReflectionRegistry::addMethod(const std::string &className,
                                       const std::string &methodName,
                                       std::unique_ptr<MethodInvokerBase> invoker)
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
        std::lock_guard<std::mutex> lock(mutex_);
        ensureMutableLocked();
        auto key = std::make_pair(className, methodName);
        std::vector<std::unique_ptr<MethodInvokerBase>> &candidates = methods_[key];
        const std::vector<std::type_index> parameters = invoker->parameterTypes();
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const MethodInvokerBase &existing = *candidates[index];
            if (existing.isConst() == invoker->isConst() &&
                existing.arity() == invoker->arity() &&
                existing.parameterTypes() == parameters)
            {
                throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                      "方法签名重复注册");
            }
        }
        candidates.push_back(std::move(invoker));
        methodNames_[className].insert(methodName);
        ensureClassLocked(className);
    }

    void ReflectionRegistry::addFactory(const std::string &className,
                                        std::unique_ptr<ObjectFactory> factory)
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
        std::lock_guard<std::mutex> lock(mutex_);
        ensureMutableLocked();
        if (factories_.find(className) != factories_.end())
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "类工厂重复注册");
        }
        factories_[className] = std::move(factory);
        ensureClassLocked(className);
    }

    void ReflectionRegistry::freeze()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frozen_.store(true, std::memory_order_release);
    }

    bool ReflectionRegistry::isFrozen() const noexcept
    {
        return frozen_.load(std::memory_order_acquire);
    }

    bool ReflectionRegistry::hasClass(const std::string &className) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        return classNamesSet_.find(className) != classNamesSet_.end();
    }

    bool ReflectionRegistry::hasField(const std::string &className,
                                      const std::string &fieldName) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            return false;
        }
        return setters_.find(std::make_pair(className, fieldName)) != setters_.end();
    }

    bool ReflectionRegistry::hasMethod(const std::string &className,
                                       const std::string &methodName) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            return false;
        }
        auto it = methods_.find(std::make_pair(className, methodName));
        return it != methods_.end() && !it->second.empty();
    }

    bool ReflectionRegistry::isFieldWritable(const std::string &className,
                                             const std::string &fieldName) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = setters_.find(std::make_pair(className, fieldName));
        if (it == setters_.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        return it->second->isWritable();
    }

    ObjectHandle ReflectionRegistry::createInstance(const std::string &className) const
    {
        ObjectFactory *factory = nullptr;
        {
            std::unique_lock<std::mutex> lock = readLock();
            auto it = factories_.find(className);
            if (it == factories_.end())
            {
                throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                      "类未注册: " + className);
            }
            factory = it->second.get();
        }
        return factory->create();
    }

    PropertySetterBase *ReflectionRegistry::getSetter(const std::string &className,
                                                      const std::string &fieldName) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = setters_.find(std::make_pair(className, fieldName));
        if (it == setters_.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        if (!it->second->isWritable())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotWritable,
                                  "字段不可写: " + className + "::" + fieldName);
        }
        return it->second.get();
    }

    std::unordered_map<std::string, Any> ReflectionRegistry::getAllValues(
        const std::string &className, const void *instance) const
    {
        std::vector<std::pair<std::string, PropertySetterBase *>> fields;
        {
            std::unique_lock<std::mutex> lock = readLock();
            if (classNamesSet_.find(className) == classNamesSet_.end())
            {
                throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                      "类未注册: " + className);
            }
            if (instance == nullptr)
            {
                throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                      "实例指针不能为空");
            }
            for (auto it = setters_.begin(); it != setters_.end(); ++it)
            {
                if (it->first.first == className)
                {
                    fields.push_back(std::make_pair(it->first.second,
                                                    it->second.get()));
                }
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
        PropertySetterBase *setter = nullptr;
        {
            std::unique_lock<std::mutex> lock = readLock();
            if (classNamesSet_.find(className) == classNamesSet_.end())
            {
                throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                      "类未注册: " + className);
            }
            if (instance == nullptr)
            {
                throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                      "实例指针不能为空");
            }
            auto it = setters_.find(std::make_pair(className, fieldName));
            if (it == setters_.end())
            {
                throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                      "字段未注册: " + className + "::" + fieldName);
            }
            setter = it->second.get();
        }
        return setter->get(instance);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         void *instance,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(className, methodName, instance, false, args);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         const void *instance,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(className, methodName, instance, true, args);
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className,
                                         const std::string &methodName,
                                         std::nullptr_t,
                                         const std::vector<Any> &args) const
    {
        return invokeMethodImpl(className, methodName, nullptr, true, args);
    }

    Any ReflectionRegistry::invokeMethodImpl(const std::string &className,
                                             const std::string &methodName,
                                             const void *instance,
                                             bool instanceIsConst,
                                             const std::vector<Any> &args) const
    {
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }

        MethodInvokerBase *selected = nullptr;
        bool arityMatched = false;
        bool typeMatched = false;
        bool constMismatch = false;
        {
            std::unique_lock<std::mutex> lock = readLock();
            if (classNamesSet_.find(className) == classNamesSet_.end())
            {
                throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                      "类未注册: " + className);
            }
            auto it = methods_.find(std::make_pair(className, methodName));
            if (it == methods_.end())
            {
                throw ReflectionError(ReflectionErrorCode::MethodNotFound,
                                      "方法未注册: " + className + "::" + methodName);
            }

            const std::vector<std::unique_ptr<MethodInvokerBase>> &candidates =
                it->second;
            for (std::size_t index = 0; index < candidates.size(); ++index)
            {
                const MethodInvokerBase &candidate = *candidates[index];
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

                if (instanceIsConst && !candidate.isConst())
                {
                    constMismatch = true;
                    continue;
                }
                if (!instanceIsConst && candidate.isConst())
                {
                    if (selected == nullptr)
                    {
                        selected = candidates[index].get();
                    }
                    continue;
                }
                if (!instanceIsConst && !candidate.isConst())
                {
                    selected = candidates[index].get();
                    break;
                }
                if (instanceIsConst && candidate.isConst())
                {
                    if (selected != nullptr)
                    {
                        throw ReflectionError(ReflectionErrorCode::AmbiguousMethod,
                                              "方法重载不明确: " +
                                                  className + "::" + methodName);
                    }
                    selected = candidates[index].get();
                }
            }
        }

        if (selected == nullptr)
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
            if (constMismatch)
            {
                throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                      "const 实例不能调用非 const 方法");
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

    std::set<std::string> ReflectionRegistry::getMethodNames(
        const std::string &className) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = methodNames_.find(className);
        if (it != methodNames_.end())
        {
            return it->second;
        }
        return std::set<std::string>();
    }

    std::vector<FieldInfo> ReflectionRegistry::getFields(
        const std::string &className) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<FieldInfo> fields;
        for (auto it = setters_.begin(); it != setters_.end(); ++it)
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
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<MethodInfo> methods;
        for (auto it = methods_.begin(); it != methods_.end(); ++it)
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
                                             invoker.isConst()));
            }
        }
        return methods;
    }

    std::vector<std::string> ReflectionRegistry::getClassNames() const
    {
        std::unique_lock<std::mutex> lock = readLock();
        return std::vector<std::string>(classNamesSet_.begin(),
                                        classNamesSet_.end());
    }

    ClassInfo ReflectionRegistry::getClassInfo(const std::string &className) const
    {
        std::unique_lock<std::mutex> lock = readLock();
        if (classNamesSet_.find(className) == classNamesSet_.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }

        ClassInfo info(className);
        for (auto it = setters_.begin(); it != setters_.end(); ++it)
        {
            if (it->first.first == className)
            {
                info.fields.push_back(FieldInfo(it->first.second,
                                                it->second->type(),
                                                it->second->isWritable()));
            }
        }
        for (auto it = methods_.begin(); it != methods_.end(); ++it)
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
                                                  invoker.isConst()));
            }
        }
        return info;
    }
}
