#include "Reflection.h"
#include <stdexcept>
#include <functional>
#include <iostream>

namespace Evently
{
    
    std::size_t PairHash::operator()(const std::pair<std::string, std::string>& pair) const {
        auto hash1 = std::hash<std::string>{}(pair.first);
        auto hash2 = std::hash<std::string>{}(pair.second);
        return hash1 ^ (hash2 << 1); // 结合两个哈希值
    }

    bool PairEqual::operator()(const std::pair<std::string, std::string>& lhs, const std::pair<std::string, std::string>& rhs) const {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }

    ReflectionRegistry& ReflectionRegistry::getInstance() {
        static ReflectionRegistry instance;
        return instance;
    }

    ReflectionRegistry::ReflectionRegistry() = default;

    PropertySetterBase* ReflectionRegistry::getSetter(const std::string& className, const std::string& fieldName) const {
        auto key = std::make_pair(className, fieldName);
        auto it = setters_.find(key);
        return it != setters_.end() ? it->second.get() : nullptr;
    }

    std::unordered_map<std::string, std::any> ReflectionRegistry::getAllValues(const std::string& className, const void* instance) const {
        std::unordered_map<std::string, std::any> values;
        for (const auto& [key, setter] : setters_) {
            if (key.first == className) {
                values[key.second] = setter->get(instance);
            }
        }
        return values;
    }

    std::any ReflectionRegistry::getValues(const std::string& className, const std::string& fieldName, const void* instance) const {
        for (const auto& [key, setter] : setters_) {
            if (key.first == className && key.second == fieldName) {
                // std::cout << "ReflectionRegistry::getValues----" << key.first << ": " << key.second << std::endl;
                // std::cout << "setter->get(instance)----" << std::any_cast<std::string>(setter->get(instance)) << std::endl;
                return setter->get(instance);
            }
        }
        return "";
    }

    std::any ReflectionRegistry::invokeMethod(const std::string &className, const std::string &methodName, void *instance, const std::vector<std::any> &args) const
    {
        if (instance == nullptr) 
        {
            throw std::runtime_error("空实例指针");
        }
        auto key = std::make_pair(className, methodName);
        auto it = methods_.find(key);
        if (it != methods_.end()) 
        {
            std::any res;
            try
            {
                res = it->second->invoke(instance, args);
                return res;
            }
            catch(const std::exception& e)
            {
                std::cerr << "函数调用错误: " << e.what() << '\n';
            }
        }
        throw std::runtime_error("没有找到该方法: " + methodName);
    }

    std::set<std::string> ReflectionRegistry::getMethodNames(const std::string& className) const {
        auto it = methodNames_.find(className);
        if (it != methodNames_.end()) {
            return it->second;
        }
        return {};  // 返回空集合表示没有方法
    }
}
