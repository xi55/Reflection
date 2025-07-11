#include "Reflection.h"
#include "Any.h"
#include <stdexcept>
#include <functional>
#include <iostream>

namespace Evently {

    std::size_t PairHash::operator()(const std::pair<std::string, std::string> &pair) const {
        // 使用简单的哈希组合算法
        auto hash1 = std::hash<std::string>{}(pair.first);   // 计算第一个字符串的哈希值
        auto hash2 = std::hash<std::string>{}(pair.second);  // 计算第二个字符串的哈希值
        return hash1 ^ (hash2 << 1);  // 结合两个哈希值，避免冲突
    }

    bool PairEqual::operator()(const std::pair<std::string, std::string> &lhs, 
                              const std::pair<std::string, std::string> &rhs) const {
        // 比较两个字符串对是否完全相等
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }

    ReflectionRegistry &ReflectionRegistry::getInstance() {
        // 线程安全的单例实现（C++11保证局部静态变量的线程安全初始化）
        static ReflectionRegistry instance;
        return instance;
    }

    ReflectionRegistry::ReflectionRegistry() {
        // 显式初始化所有成员容器（C++11兼容写法）
        setters_ = std::unordered_map<std::pair<std::string, std::string>, 
                                     std::unique_ptr<PropertySetterBase>, 
                                     PairHash, 
                                     PairEqual>();
        methods_ = std::unordered_map<std::pair<std::string, std::string>, 
                                     std::unique_ptr<MethodInvokerBase>, 
                                     PairHash, 
                                     PairEqual>();
        methodNames_ = std::unordered_map<std::string, std::set<std::string>>();
        classNames_ = std::unordered_map<std::string, std::string>();
        factories_ = std::unordered_map<std::string, std::unique_ptr<ObjectFactory>>();
    }

    PropertySetterBase *ReflectionRegistry::getSetter(const std::string &className, 
                                                      const std::string &fieldName) const {
        // 构造查找键值并在设置器映射中查找
        auto key = std::make_pair(className, fieldName);
        auto it = setters_.find(key);
        return it != setters_.end() ? it->second.get() : nullptr;
    }

    std::unordered_map<std::string, Any> ReflectionRegistry::getAllValues(
        const std::string &className, const void *instance) const {
        
        std::unordered_map<std::string, Any> values;
        
        // 遍历所有已注册的属性设置器
        for (const auto &pair : setters_) {
            const auto &key = pair.first;      // (类名, 属性名)
            const auto &setter = pair.second;  // 属性设置器
            
            // 只处理指定类名的属性
            if (key.first == className) {
                values[key.second] = setter->get(instance);
            }
        }
        return values;
    }

    Any ReflectionRegistry::getValues(const std::string &className, 
                                     const std::string &fieldName, 
                                     const void *instance) const {
        // 遍历查找指定的属性
        for (const auto &pair : setters_) {
            const auto &key = pair.first;      // (类名, 属性名)
            const auto &setter = pair.second;  // 属性设置器
            
            // 找到匹配的类名和属性名
            if (key.first == className && key.second == fieldName) {
                return setter->get(instance);
            }
        }
        
        // 未找到则返回空Any对象
        return Any();
    }

    Any ReflectionRegistry::invokeMethod(const std::string &className, 
                                        const std::string &methodName, 
                                        void *instance, 
                                        const std::vector<Any> &args) const {
        // 参数验证
        if (instance == nullptr) {
            throw std::runtime_error("实例指针不能为空");
        }
        
        // 构造查找键值并在方法映射中查找
        auto key = std::make_pair(className, methodName);
        auto it = methods_.find(key);
        
        if (it != methods_.end()) {
            try {
                // 调用找到的方法
                Any result = it->second->invoke(instance, args);
                return result;
            } catch (const std::exception &e) {
                std::cerr << "方法调用失败: " << e.what() << '\n';
                throw; // 重新抛出异常以便上层处理
            }
        }
        
        // 未找到方法时抛出异常
        throw std::runtime_error("未找到方法: " + className + "::" + methodName);
    }

    std::set<std::string> ReflectionRegistry::getMethodNames(const std::string &className) const {
        // 在方法名映射中查找指定类的所有方法
        auto it = methodNames_.find(className);
        if (it != methodNames_.end()) {
            return it->second;
        }
        
        // 未找到则返回空集合
        return std::set<std::string>();
    }

} // namespace Evently
