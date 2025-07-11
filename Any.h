#ifndef ANY_H
#define ANY_H
#pragma once

#include <typeinfo>
#include <utility>
#include <type_traits>
#include <stdexcept>
#include <memory>

namespace Evently {

/**
 * @brief 类型擦除容器类，用于替代C++17的std::any
 * 
 * 这个类可以存储任何类型的值，并在运行时保持类型信息。
 * 类似于C++17的std::any，但兼容C++11标准。
 */
class Any {
public:
    /// 默认构造函数，创建空的Any对象
    Any() : content_(nullptr) {}
    
    /**
     * @brief 模板构造函数，用于存储任意类型的值
     * @tparam T 要存储的值的类型
     * @param value 要存储的值
     */
    template<typename T>
    Any(const T& value) 
        : content_(new Holder<typename std::decay<T>::type>(value)) {}

    /// 拷贝构造函数
    Any(const Any& other)
        : content_(other.content_ ? other.content_->clone() : nullptr) {}
    
    /// 移动构造函数（C++11 noexcept）
    Any(Any&& other) noexcept : content_(other.content_) {
        other.content_ = nullptr;
    }

    /// 析构函数，释放存储的对象
    ~Any() {
        delete content_;
    }

    /// 拷贝赋值操作符
    Any& operator=(const Any& rhs) {
        Any(rhs).swap(*this);
        return *this;
    }

    /// 移动赋值操作符
    Any& operator=(Any&& rhs) noexcept {
        rhs.swap(*this);
        return *this;
    }

    /**
     * @brief 模板赋值操作符，用于赋值任意类型的值
     * @tparam T 要赋值的值的类型
     * @param value 要赋值的值
     * @return Any& 返回自身引用
     */
    template<typename T>
    Any& operator=(const T& value) {
        Any(value).swap(*this);
        return *this;
    }

    /// 交换两个Any对象的内容
    void swap(Any& rhs) noexcept {
        std::swap(content_, rhs.content_);
    }

    /// 检查Any对象是否为空
    bool empty() const noexcept {
        return !content_;
    }

    /// 获取存储值的类型信息
    const std::type_info& type() const noexcept {
        return content_ ? content_->type() : typeid(void);
    }

    /**
     * @brief 尝试将存储的值转换为指定类型的指针
     * @tparam T 目标类型
     * @return T* 如果类型匹配返回指向值的指针，否则返回nullptr
     */
    template<typename T>
    T* cast() {
        return content_ ? &static_cast<Holder<T>*>(content_)->held : nullptr;
    }

    /**
     * @brief 友元函数声明，允许any_cast访问私有成员
     */
    template<typename T>
    friend T any_cast(const Any& operand);

    template<typename T>
    friend T* any_cast(Any* operand);

private:
    /**
     * @brief 占位符基类，用于类型擦除
     * 
     * 这是一个抽象基类，用于在Any中存储不同类型的值
     * 而不需要知道具体的类型信息。
     */
    class PlaceHolder {
    public:
        virtual ~PlaceHolder() {}
        
        /// 获取存储值的类型信息
        virtual const std::type_info& type() const = 0;
        
        /// 克隆当前对象
        virtual PlaceHolder* clone() const = 0;
    };

    /**
     * @brief 具体的值持有者模板类
     * @tparam T 存储的值的类型
     * 
     * 这个类继承自PlaceHolder，用于存储具体类型的值
     */
    template<typename T>
    class Holder : public PlaceHolder {
    public:
        /// 构造函数，存储给定的值
        Holder(const T& value) : held(value) {}
        
        /// 返回存储值的类型信息
        const std::type_info& type() const override {
            return typeid(T);
        }

        /// 克隆当前Holder对象
        PlaceHolder* clone() const override {
            return new Holder(held);
        }

        T held;  ///< 实际存储的值
    };

    PlaceHolder* content_;  ///< 指向实际存储对象的指针
};

/**
 * @brief 类型转换函数，将Any对象转换为指定类型
 * @tparam T 目标类型
 * @param operand 要转换的Any对象
 * @return T 转换后的值
 * @throws std::bad_cast 如果类型不匹配
 */
template<typename T>
T any_cast(const Any& operand) {
    if (operand.type() != typeid(T)) {
        throw std::bad_cast();
    }
    return static_cast<const Any::Holder<T>*>(operand.content_)->held;
}

/**
 * @brief 指针版本的类型转换函数
 * @tparam T 目标类型
 * @param operand 指向Any对象的指针
 * @return T* 如果类型匹配返回指向值的指针，否则返回nullptr
 */
template<typename T>
T* any_cast(Any* operand) {
    return operand ? operand->cast<T>() : nullptr;
}

/**
 * @brief Any类型转换失败时抛出的异常
 * 
 * 当any_cast尝试将Any对象转换为不匹配的类型时抛出此异常
 */
class bad_any_cast : public std::bad_cast {
public:
    const char* what() const noexcept override {
        return "bad any cast";
    }
};

}  // namespace Evently

#endif // ANY_H
