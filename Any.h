#ifndef ANY_H
#define ANY_H
#pragma once

#include <cstddef>
#include <new>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace Evently
{
    class Any
    {
    public:
        Any() noexcept
            : ops_(nullptr), local_(false), ptr_(nullptr) {}

        template <
            typename T,
            typename std::enable_if<
                !std::is_same<typename std::decay<T>::type, Any>::value,
                int>::type = 0>
        Any(T &&value)
            : ops_(nullptr), local_(false), ptr_(nullptr)
        {
            construct<typename std::decay<T>::type>(std::forward<T>(value));
        }

        Any(const Any &other)
            : ops_(nullptr), local_(false), ptr_(nullptr)
        {
            if (other.ops_ != nullptr)
            {
                other.ops_->copy(other, *this);
            }
        }

        Any(Any &&other) noexcept
            : ops_(nullptr), local_(false), ptr_(nullptr)
        {
            if (other.ops_ != nullptr)
            {
                other.ops_->move(other, *this);
            }
        }

        ~Any()
        {
            reset();
        }

        Any &operator=(const Any &rhs)
        {
            Any(rhs).swap(*this);
            return *this;
        }

        Any &operator=(Any &&rhs) noexcept
        {
            if (this != &rhs)
            {
                reset();
                if (rhs.ops_ != nullptr)
                {
                    rhs.ops_->move(rhs, *this);
                }
            }
            return *this;
        }

        template <
            typename T,
            typename std::enable_if<
                !std::is_same<typename std::decay<T>::type, Any>::value,
                int>::type = 0>
        Any &operator=(T &&value)
        {
            Any(std::forward<T>(value)).swap(*this);
            return *this;
        }

        void swap(Any &rhs) noexcept
        {
            if (this == &rhs)
            {
                return;
            }
            Any temporary(std::move(rhs));
            rhs = std::move(*this);
            *this = std::move(temporary);
        }

        bool empty() const noexcept
        {
            return ops_ == nullptr;
        }

        bool has_value() const noexcept
        {
            return ops_ != nullptr;
        }

        const std::type_info &type() const noexcept
        {
            return ops_ != nullptr ? ops_->type() : typeid(void);
        }

        void reset() noexcept
        {
            if (ops_ != nullptr)
            {
                ops_->destroy(data(), local_);
                ops_ = nullptr;
                local_ = false;
                ptr_ = nullptr;
            }
        }

        template <typename T, typename... Args>
        void emplace(Args &&...args)
        {
            reset();
            construct<T>(std::forward<Args>(args)...);
        }

        template <typename T>
        friend T any_cast(const Any &operand);

        template <typename T>
        friend T any_cast(Any &operand);

        template <typename T>
        friend T *any_cast(Any *operand);

        template <typename T>
        friend const T *any_cast(const Any *operand);

    private:
        static const std::size_t BufferSize = 16;

        typedef typename std::aligned_storage<
            BufferSize, alignof(std::max_align_t)>::type Storage;

        template <typename T>
        struct FitsInline
        {
            static const bool value =
                sizeof(T) <= BufferSize &&
                alignof(T) <= alignof(Storage) &&
                std::is_nothrow_move_constructible<T>::value;
        };

        struct Ops
        {
            const std::type_info &(*type)();
            void (*destroy)(void *, bool);
            void (*copy)(const Any &, Any &);
            void (*move)(Any &, Any &);
        };

        template <typename T>
        static const std::type_info &typeImpl()
        {
            return typeid(T);
        }

        template <typename T>
        static void destroyImpl(void *object, bool local)
        {
            if (local)
            {
                static_cast<T *>(object)->~T();
            }
            else
            {
                delete static_cast<T *>(object);
            }
        }

        template <typename T>
        static void copyImpl(const Any &source, Any &target)
        {
            copyDispatch<T>(
                std::integral_constant<bool, FitsInline<T>::value>{},
                source, target);
        }

        template <typename T>
        static void copyDispatch(std::true_type, const Any &source, Any &target)
        {
            const T *value = static_cast<const T *>(source.data());
            new (&target.storage_) T(*value);
            target.local_ = true;
            target.ptr_ = nullptr;
            target.ops_ = &opsFor<T>();
        }

        template <typename T>
        static void copyDispatch(std::false_type, const Any &source, Any &target)
        {
            target.ptr_ = new T(*static_cast<const T *>(source.data()));
            target.local_ = false;
            target.ops_ = &opsFor<T>();
        }

        template <typename T>
        static void moveImpl(Any &source, Any &target)
        {
            moveDispatch<T>(
                std::integral_constant<bool, FitsInline<T>::value>{},
                source, target);
        }

        template <typename T>
        static void moveDispatch(std::true_type, Any &source, Any &target)
        {
            T *value = static_cast<T *>(source.data());
            new (&target.storage_) T(std::move(*value));
            target.local_ = true;
            target.ptr_ = nullptr;
            target.ops_ = &opsFor<T>();
            value->~T();
            source.ops_ = nullptr;
            source.local_ = false;
        }

        template <typename T>
        static void moveDispatch(std::false_type, Any &source, Any &target)
        {
            target.ptr_ = source.ptr_;
            target.local_ = false;
            target.ops_ = &opsFor<T>();
            source.ptr_ = nullptr;
            source.ops_ = nullptr;
            source.local_ = false;
        }

        template <typename T>
        static const Ops &opsFor()
        {
            static const Ops ops = {
                &typeImpl<T>,
                &destroyImpl<T>,
                &copyImpl<T>,
                &moveImpl<T>};
            return ops;
        }

        template <typename T, typename... Args>
        void construct(Args &&...args)
        {
            static_assert(std::is_copy_constructible<T>::value,
                          "Any requires copy-constructible types");
            constructDispatch<T>(
                std::integral_constant<bool, FitsInline<T>::value>{},
                *this, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        static void constructDispatch(std::true_type, Any &target,
                                      Args &&...args)
        {
            new (&target.storage_) T(std::forward<Args>(args)...);
            target.local_ = true;
            target.ptr_ = nullptr;
            target.ops_ = &opsFor<T>();
        }

        template <typename T, typename... Args>
        static void constructDispatch(std::false_type, Any &target,
                                      Args &&...args)
        {
            target.ptr_ = new T(std::forward<Args>(args)...);
            target.local_ = false;
            target.ops_ = &opsFor<T>();
        }

        void *data() noexcept
        {
            return local_ ? static_cast<void *>(&storage_) : ptr_;
        }

        const void *data() const noexcept
        {
            return local_ ? static_cast<const void *>(&storage_) : ptr_;
        }

        const Ops *ops_;
        bool local_;
        void *ptr_;
        Storage storage_;
    };

    inline void swap(Any &lhs, Any &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    class bad_any_cast : public std::bad_cast
    {
    public:
        explicit bad_any_cast(const std::string &message)
            : message_(message) {}

        const char *what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };

    template <typename T>
    T any_cast(const Any &operand)
    {
        static_assert(!std::is_lvalue_reference<T>::value ||
                          std::is_const<typename std::remove_reference<T>::type>::value,
                      "any_cast<T&>(const Any&) is not allowed");
        static_assert(!std::is_rvalue_reference<T>::value,
                      "any_cast<T&&> is not supported");
        typedef typename std::decay<T>::type U;
        if (operand.type() != typeid(U))
        {
            throw bad_any_cast("bad any cast from " +
                               std::string(operand.type().name()) + " to " +
                               std::string(typeid(U).name()));
        }
        const U *value = static_cast<const U *>(operand.data());
        return static_cast<T>(*value);
    }

    template <typename T>
    T any_cast(Any &operand)
    {
        static_assert(!std::is_rvalue_reference<T>::value,
                      "any_cast<T&&> is not supported");
        typedef typename std::decay<T>::type U;
        if (operand.type() != typeid(U))
        {
            throw bad_any_cast("bad any cast from " +
                               std::string(operand.type().name()) + " to " +
                               std::string(typeid(U).name()));
        }
        U *value = static_cast<U *>(operand.data());
        return static_cast<T>(*value);
    }

    template <typename T>
    T *any_cast(Any *operand)
    {
        static_assert(!std::is_reference<T>::value,
                      "any_cast pointer form requires a non-reference type");
        typedef typename std::remove_cv<T>::type U;
        if (operand == nullptr || operand->type() != typeid(U))
        {
            return nullptr;
        }
        return static_cast<T *>(operand->data());
    }

    template <typename T>
    const T *any_cast(const Any *operand)
    {
        static_assert(!std::is_reference<T>::value,
                      "any_cast pointer form requires a non-reference type");
        typedef typename std::remove_cv<T>::type U;
        if (operand == nullptr || operand->type() != typeid(U))
        {
            return nullptr;
        }
        return static_cast<const T *>(operand->data());
    }
}

#endif // ANY_H
