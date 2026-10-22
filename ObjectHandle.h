#ifndef EVENTLY_OBJECT_HANDLE_H
#define EVENTLY_OBJECT_HANDLE_H
#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace Evently
{
    class ObjectHandle
    {
    public:
        ObjectHandle() noexcept
            : object_(), type_(typeid(void)) {}

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(object_);
        }

        bool empty() const noexcept
        {
            return !object_;
        }

        const std::type_index &type() const noexcept
        {
            return type_;
        }

        template <typename T>
        T *get() const
        {
            static_assert(!std::is_reference<T>::value,
                          "ObjectHandle::get<T> requires a non-reference type");
            static_assert(!std::is_void<T>::value,
                          "ObjectHandle::get<T> requires a non-void type");
            typedef typename std::remove_cv<T>::type U;
            if (type_ != std::type_index(typeid(U)))
            {
                return nullptr;
            }
            return static_cast<T *>(object_.get());
        }

        void reset() noexcept
        {
            object_.reset();
            type_ = std::type_index(typeid(void));
        }

        void swap(ObjectHandle &other) noexcept
        {
            object_.swap(other.object_);
            std::swap(type_, other.type_);
        }

        template <typename T, typename... Args>
        static ObjectHandle create(Args &&... args)
        {
            static_assert(!std::is_const<T>::value,
                          "ObjectHandle::create<T> requires a non-const type");
            static_assert(!std::is_reference<T>::value,
                          "ObjectHandle::create<T> requires a non-reference type");
            std::shared_ptr<T> object(new T(std::forward<Args>(args)...));
            return ObjectHandle(std::shared_ptr<void>(object),
                                std::type_index(typeid(T)));
        }

    private:
        ObjectHandle(std::shared_ptr<void> object, const std::type_index &type)
            : object_(std::move(object)), type_(type) {}

        std::shared_ptr<void> object_;
        std::type_index type_;
    };

    inline void swap(ObjectHandle &lhs, ObjectHandle &rhs) noexcept
    {
        lhs.swap(rhs);
    }
}

#endif // EVENTLY_OBJECT_HANDLE_H
