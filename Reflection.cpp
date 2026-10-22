#include "Reflection.h"

#include <functional>

namespace Evently
{
    namespace
    {
        struct ClassData
        {
            std::unordered_map<std::string,
                               std::shared_ptr<PropertySetterBase>>
                fields;
            std::unordered_map<
                std::string,
                std::vector<std::shared_ptr<MethodInvokerBase>>>
                methods;
            std::set<std::string> methodNames;
            std::vector<std::shared_ptr<ConstructorInvokerBase>> constructors;
            std::shared_ptr<ObjectFactory> factory;
        };

        struct PluginScopeData
        {
            PluginScopeData()
                : registry(nullptr), detached(false) {}

            ReflectionRegistry *registry;
            std::string moduleName;
            std::set<std::string> classes;
            bool detached;
        };

        thread_local const ReflectionRegistry *tlsBatchOwner = nullptr;
        thread_local PluginScopeData *tlsPluginScope = nullptr;

        std::mutex &queueMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::vector<void (*)()> &queueFunctions()
        {
            static std::vector<void (*)()> functions;
            return functions;
        }

        void recordPluginClass(const std::string &className)
        {
            if (tlsPluginScope != nullptr)
            {
                tlsPluginScope->classes.insert(className);
            }
        }

        std::shared_ptr<ClassData> mutableClass(detail::RegistryData &data,
                                                const std::string &className);

        void addQueueFunction(void (*function)())
        {
            std::lock_guard<std::mutex> lock(queueMutex());
            queueFunctions().push_back(function);
        }

        void runQueue()
        {
            std::vector<void (*)()> functions;
            {
                std::lock_guard<std::mutex> lock(queueMutex());
                functions.swap(queueFunctions());
            }
            try
            {
                for (std::size_t index = 0; index < functions.size(); ++index)
                {
                    functions[index]();
                }
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(queueMutex());
                queueFunctions().insert(queueFunctions().end(),
                                       functions.begin(), functions.end());
                throw;
            }
        }
    }

    struct detail::RegistryData
    {
        std::unordered_map<std::string, std::shared_ptr<ClassData>> classes;
        std::unordered_map<std::type_index, std::string> primaryNames;
        std::unordered_map<std::string, std::type_index> classNameOwners;
    };

    struct ReflectionRegistry::Impl
    {
        Impl() : data(new detail::RegistryData()), frozen(false) {}

        std::shared_ptr<const detail::RegistryData> data;
        std::shared_ptr<detail::RegistryData> batchData;
        std::mutex writeMutex;
        std::atomic<bool> frozen;
    };

    struct RegistrationBatch::Impl
    {
        Impl() : registry(nullptr), committed(false) {}

        ReflectionRegistry *registry;
        std::unique_lock<std::mutex> lock;
        bool committed;
    };

    struct PluginRegistration::Impl
    {
        PluginScopeData scope;
    };

    namespace detail
    {
        void enqueueRegistration(void (*function)())
        {
            addQueueFunction(function);
        }

        void runRegistrationQueue()
        {
            runQueue();
        }
    }

    namespace
    {
        std::shared_ptr<ClassData> findClass(const detail::RegistryData &data,
                                             const std::string &className)
        {
            auto it = data.classes.find(className);
            if (it == data.classes.end())
            {
                return std::shared_ptr<ClassData>();
            }
            return it->second;
        }

        std::shared_ptr<ClassData> mutableClass(detail::RegistryData &data,
                                                const std::string &className)
        {
            auto it = data.classes.find(className);
            if (it == data.classes.end())
            {
                std::shared_ptr<ClassData> created(new ClassData());
                data.classes.insert(std::make_pair(className, created));
                recordPluginClass(className);
                return created;
            }
            if (it->second.use_count() == 1)
            {
                return it->second;
            }
            std::shared_ptr<ClassData> copy(new ClassData(*it->second));
            it->second = copy;
            return copy;
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
        : impl_(new Impl())
    {
    }

    ReflectionRegistry::~ReflectionRegistry()
    {
        delete impl_;
    }

    std::shared_ptr<const detail::RegistryData>
    ReflectionRegistry::snapshot() const
    {
        if (tlsBatchOwner == this && impl_->batchData)
        {
            return std::shared_ptr<const detail::RegistryData>(impl_->batchData);
        }
        return std::atomic_load(&impl_->data);
    }

    std::shared_ptr<detail::RegistryData>
    ReflectionRegistry::copyData() const
    {
        std::shared_ptr<const detail::RegistryData> current =
            std::atomic_load(&impl_->data);
        return std::shared_ptr<detail::RegistryData>(new detail::RegistryData(*current));
    }

    void ReflectionRegistry::publish(
        const std::shared_ptr<detail::RegistryData> &data)
    {
        std::atomic_store(&impl_->data,
                          std::shared_ptr<const detail::RegistryData>(data));
    }

    std::shared_ptr<detail::RegistryData>
    ReflectionRegistry::beginWrite(std::unique_lock<std::mutex> &lock,
                                   bool &batched)
    {
        if (tlsBatchOwner == this && impl_->batchData)
        {
            batched = true;
            return impl_->batchData;
        }
        lock = std::unique_lock<std::mutex>(impl_->writeMutex);
        batched = false;
        return copyData();
    }

    void ReflectionRegistry::endWrite(
        const std::shared_ptr<detail::RegistryData> &data, bool batched)
    {
        if (!batched)
        {
            publish(data);
        }
    }

    std::unique_lock<std::mutex> ReflectionRegistry::acquireBatchLock()
    {
        if (tlsBatchOwner != nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "不允许嵌套注册批处理");
        }
        return std::unique_lock<std::mutex>(impl_->writeMutex);
    }

    void ReflectionRegistry::startBatch()
    {
        if (impl_->batchData)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "注册批处理已经启动");
        }
        impl_->batchData = copyData();
        tlsBatchOwner = this;
    }

    void ReflectionRegistry::commitBatch()
    {
        if (tlsBatchOwner != this || !impl_->batchData)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "没有可提交的注册批处理");
        }
        publish(impl_->batchData);
        tlsBatchOwner = nullptr;
        impl_->batchData.reset();
    }

    void ReflectionRegistry::abortBatch()
    {
        if (tlsBatchOwner == this)
        {
            tlsBatchOwner = nullptr;
        }
        impl_->batchData.reset();
    }

    RegistrationBatch::RegistrationBatch(ReflectionRegistry &registry)
        : impl_(new Impl())
    {
        try
        {
            impl_->registry = &registry;
            impl_->lock = registry.acquireBatchLock();
            registry.startBatch();
        }
        catch (...)
        {
            delete impl_;
            impl_ = nullptr;
            throw;
        }
    }

    RegistrationBatch::~RegistrationBatch()
    {
        if (impl_ != nullptr && !impl_->committed)
        {
            impl_->registry->abortBatch();
        }
        delete impl_;
    }

    void RegistrationBatch::commit()
    {
        if (impl_ == nullptr || impl_->committed)
        {
            return;
        }
        impl_->registry->commitBatch();
        impl_->committed = true;
    }

    PluginRegistration::PluginRegistration(ReflectionRegistry &registry,
                                           const std::string &moduleName)
        : impl_(new Impl())
    {
        if (tlsPluginScope != nullptr)
        {
            delete impl_;
            impl_ = nullptr;
            throw ReflectionError(ReflectionErrorCode::InvalidArgument,
                                  "不允许嵌套插件注册作用域");
        }
        try
        {
            impl_->scope.registry = &registry;
            impl_->scope.moduleName = moduleName;
            impl_->scope.detached = false;
            tlsPluginScope = &impl_->scope;
        }
        catch (...)
        {
            delete impl_;
            impl_ = nullptr;
            throw;
        }
    }

    PluginRegistration::~PluginRegistration()
    {
        if (impl_ == nullptr)
        {
            return;
        }
        if (tlsPluginScope == &impl_->scope)
        {
            tlsPluginScope = nullptr;
        }
        if (!impl_->scope.detached && impl_->scope.registry != nullptr)
        {
            for (std::set<std::string>::const_iterator it =
                     impl_->scope.classes.begin();
                 it != impl_->scope.classes.end(); ++it)
            {
                try
                {
                    impl_->scope.registry->unregisterClass(*it);
                }
                catch (...)
                {
                }
            }
        }
        delete impl_;
    }

    void PluginRegistration::detach()
    {
        if (impl_ != nullptr)
        {
            impl_->scope.detached = true;
        }
    }

    const std::string &PluginRegistration::moduleName() const
    {
        return impl_->scope.moduleName;
    }

    void ReflectionRegistry::ensureClass(detail::RegistryData &data,
                                         const std::string &className)
    {
        auto it = data.classes.find(className);
        if (it != data.classes.end())
        {
            if (tlsPluginScope != nullptr &&
                tlsPluginScope->classes.find(className) ==
                    tlsPluginScope->classes.end())
            {
                throw ReflectionError(
                    ReflectionErrorCode::InvalidArgument,
                    "插件只能注册新类，不能修改已有类: " + className);
            }
            return;
        }
        data.classes.insert(
            std::make_pair(className, std::shared_ptr<ClassData>(new ClassData())));
        recordPluginClass(className);
    }

    void ReflectionRegistry::checkClassNameOwner(
        detail::RegistryData &data, const std::type_index &type,
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

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        auto it = next->primaryNames.find(type);
        if (it != next->primaryNames.end() && it->second != className)
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "该类型已注册为其他类名");
        }
        checkClassNameOwner(*next, type, className);
        next->primaryNames[type] = className;
        ensureClass(*next, className);
        endWrite(next, batched);
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

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        checkClassNameOwner(*next, type, className);
        ensureClass(*next, className);
        std::shared_ptr<ClassData> data = mutableClass(*next, className);
        if (data->fields.find(fieldName) != data->fields.end())
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "字段重复注册");
        }
        data->fields[fieldName] = setter;
        endWrite(next, batched);
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

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        checkClassNameOwner(*next, type, className);
        ensureClass(*next, className);
        std::shared_ptr<ClassData> data = mutableClass(*next, className);
        std::vector<std::shared_ptr<MethodInvokerBase>> &candidates =
            data->methods[methodName];
        const std::vector<std::type_index> parameters =
            invoker->parameterTypes();
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
        data->methodNames.insert(methodName);
        endWrite(next, batched);
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

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        checkClassNameOwner(*next, type, className);
        ensureClass(*next, className);
        std::shared_ptr<ClassData> data = mutableClass(*next, className);
        const std::vector<std::type_index> parameters =
            constructor->parameterTypes();
        for (std::size_t index = 0; index < data->constructors.size(); ++index)
        {
            const ConstructorInvokerBase &existing =
                *data->constructors[index];
            if (existing.arity() == constructor->arity() &&
                existing.parameterTypes() == parameters)
            {
                throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                      "构造函数重复注册");
            }
        }
        data->constructors.push_back(constructor);
        endWrite(next, batched);
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

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        checkClassNameOwner(*next, type, className);
        ensureClass(*next, className);
        std::shared_ptr<ClassData> data = mutableClass(*next, className);
        if (data->factory)
        {
            throw ReflectionError(ReflectionErrorCode::DuplicateRegistration,
                                  "类工厂重复注册");
        }
        data->factory = factory;
        endWrite(next, batched);
    }

    bool ReflectionRegistry::unregisterClass(const std::string &className)
    {
        if (className.empty())
        {
            return false;
        }

        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        if (next->classes.find(className) == next->classes.end())
        {
            return false;
        }

        next->classes.erase(className);
        next->classNameOwners.erase(className);
        for (auto it = next->primaryNames.begin();
             it != next->primaryNames.end();)
        {
            if (it->second == className)
            {
                it = next->primaryNames.erase(it);
            }
            else
            {
                ++it;
            }
        }
        endWrite(next, batched);
        return true;
    }

    void ReflectionRegistry::clear()
    {
        std::unique_lock<std::mutex> lock;
        bool batched = false;
        std::shared_ptr<detail::RegistryData> next = beginWrite(lock, batched);
        *next = detail::RegistryData();
        endWrite(next, batched);
        impl_->frozen.store(false, std::memory_order_release);
    }

    void ReflectionRegistry::registerAll()
    {
        RegistrationBatch batch(*this);
        detail::runRegistrationQueue();
        batch.commit();
    }

    void ReflectionRegistry::freeze()
    {
        impl_->frozen.store(true, std::memory_order_release);
    }

    bool ReflectionRegistry::isFrozen() const noexcept
    {
        return impl_->frozen.load(std::memory_order_acquire);
    }

    std::string ReflectionRegistry::getClassNameByType(
        const std::type_index &type) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        auto it = data->primaryNames.find(type);
        if (it == data->primaryNames.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册");
        }
        return it->second;
    }

    bool ReflectionRegistry::hasClassNameByType(
        const std::type_index &type) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        return data->primaryNames.find(type) != data->primaryNames.end();
    }

    std::string ReflectionRegistry::resolveClassNameByType(
        const std::type_index &type) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        auto it = data->primaryNames.find(type);
        if (it == data->primaryNames.end())
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册，无法注册字段");
        }
        return it->second;
    }

    bool ReflectionRegistry::hasClass(const std::string &className) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        return data->classes.find(className) != data->classes.end();
    }

    bool ReflectionRegistry::hasField(const std::string &className,
                                      const std::string &fieldName) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            return false;
        }
        return classData->fields.find(fieldName) != classData->fields.end();
    }

    bool ReflectionRegistry::hasMethod(const std::string &className,
                                       const std::string &methodName) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            return false;
        }
        auto it = classData->methods.find(methodName);
        return it != classData->methods.end() && !it->second.empty();
    }

    bool ReflectionRegistry::isFieldWritable(const std::string &className,
                                             const std::string &fieldName) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = classData->fields.find(fieldName);
        if (it == classData->fields.end())
        {
            throw ReflectionError(ReflectionErrorCode::FieldNotFound,
                                  "字段未注册: " + className + "::" + fieldName);
        }
        return it->second->isWritable();
    }

    ObjectHandle ReflectionRegistry::createInstance(
        const std::string &className) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (classData->factory)
        {
            return classData->factory->create();
        }
        for (std::size_t index = 0; index < classData->constructors.size();
             ++index)
        {
            if (classData->constructors[index]->arity() == 0)
            {
                return classData->constructors[index]->construct(
                    std::vector<Any>());
            }
        }
        throw ReflectionError(ReflectionErrorCode::ConstructorNotFound,
                              "未找到构造函数: " + className);
    }

    ObjectHandle ReflectionRegistry::createInstance(
        const std::string &className, const std::vector<Any> &args) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (classData->constructors.empty())
        {
            throw ReflectionError(ReflectionErrorCode::ConstructorNotFound,
                                  "未找到构造函数: " + className);
        }

        std::shared_ptr<ConstructorInvokerBase> selected;
        bool arityMatched = false;
        bool typeMatched = false;
        for (std::size_t index = 0; index < classData->constructors.size();
             ++index)
        {
            const ConstructorInvokerBase &candidate =
                *classData->constructors[index];
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
            selected = classData->constructors[index];
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
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = classData->fields.find(fieldName);
        if (it == classData->fields.end())
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
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }

        std::unordered_map<std::string, Any> values;
        for (auto it = classData->fields.begin(); it != classData->fields.end();
             ++it)
        {
            values[it->first] = it->second->get(instance);
        }
        return values;
    }

    Any ReflectionRegistry::getValues(const std::string &className,
                                      const std::string &fieldName,
                                      const void *instance) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        if (instance == nullptr)
        {
            throw ReflectionError(ReflectionErrorCode::InvalidInstance,
                                  "实例指针不能为空");
        }
        auto it = classData->fields.find(fieldName);
        if (it == classData->fields.end())
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

        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = classData->methods.find(methodName);
        if (it == classData->methods.end())
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
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        auto it = classData->methods.find(functionName);
        if (it == classData->methods.end())
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
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        return classData->methodNames;
    }

    std::vector<FieldInfo> ReflectionRegistry::getFields(
        const std::string &className) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<FieldInfo> fields;
        for (auto it = classData->fields.begin(); it != classData->fields.end();
             ++it)
        {
            fields.push_back(FieldInfo(it->first, it->second->type(),
                                       it->second->isWritable()));
        }
        return fields;
    }

    std::vector<MethodInfo> ReflectionRegistry::getMethods(
        const std::string &className) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<MethodInfo> methods;
        for (auto it = classData->methods.begin();
             it != classData->methods.end(); ++it)
        {
            for (std::size_t index = 0; index < it->second.size(); ++index)
            {
                const MethodInvokerBase &invoker = *it->second[index];
                methods.push_back(MethodInfo(it->first,
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
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }
        std::vector<ConstructorInfo> constructors;
        for (std::size_t index = 0; index < classData->constructors.size();
             ++index)
        {
            const ConstructorInvokerBase &constructor =
                *classData->constructors[index];
            constructors.push_back(ConstructorInfo(
                constructor.arity(), constructor.parameterTypes()));
        }
        return constructors;
    }

    std::vector<std::string> ReflectionRegistry::getClassNames() const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::vector<std::string> names;
        names.reserve(data->classes.size());
        for (auto it = data->classes.begin(); it != data->classes.end(); ++it)
        {
            names.push_back(it->first);
        }
        return names;
    }

    ClassInfo ReflectionRegistry::getClassInfo(
        const std::string &className) const
    {
        std::shared_ptr<const detail::RegistryData> data = snapshot();
        std::shared_ptr<ClassData> classData = findClass(*data, className);
        if (!classData)
        {
            throw ReflectionError(ReflectionErrorCode::ClassNotFound,
                                  "类未注册: " + className);
        }

        ClassInfo info(className);
        for (auto it = classData->fields.begin(); it != classData->fields.end();
             ++it)
        {
            info.fields.push_back(FieldInfo(it->first, it->second->type(),
                                            it->second->isWritable()));
        }
        for (auto it = classData->methods.begin();
             it != classData->methods.end(); ++it)
        {
            for (std::size_t index = 0; index < it->second.size(); ++index)
            {
                const MethodInvokerBase &invoker = *it->second[index];
                info.methods.push_back(MethodInfo(it->first,
                                                  invoker.arity(),
                                                  invoker.parameterTypes(),
                                                  invoker.returnType(),
                                                  invoker.isConst(),
                                                  invoker.isStatic(),
                                                  invoker.refQualifier()));
            }
        }
        for (std::size_t index = 0; index < classData->constructors.size();
             ++index)
        {
            const ConstructorInvokerBase &constructor =
                *classData->constructors[index];
            info.constructors.push_back(ConstructorInfo(
                constructor.arity(), constructor.parameterTypes()));
        }
        return info;
    }
}
