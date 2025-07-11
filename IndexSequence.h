#ifndef INDEX_SEQUENCE_H
#define INDEX_SEQUENCE_H
#pragma once

#include <cstddef>

namespace Evently
{

    /**
     * @brief C++11兼容的index_sequence实现
     *
     * 这个实现提供了与C++14 std::index_sequence相同的功能，
     * 用于在编译时生成整数序列，支持参数包展开。
     */

    // index_sequence的基本定义
    template <std::size_t... Ints>
    struct index_sequence
    {
        typedef std::size_t value_type;
        static std::size_t size() { return sizeof...(Ints); }
    };

    // 辅助结构，用于递归构建index_sequence
    template <std::size_t N, std::size_t... Ints>
    struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, Ints...>
    {
    };

    // 递归终止条件
    template <std::size_t... Ints>
    struct make_index_sequence_impl<0, Ints...>
    {
        typedef index_sequence<Ints...> type;
    };

    /**
     * @brief 生成从0到N-1的index_sequence
     * @tparam N 序列长度
     */
    template <std::size_t N>
    struct make_index_sequence
    {
        typedef typename make_index_sequence_impl<N>::type type;
    };

    /**
     * @brief 根据类型包生成对应长度的index_sequence
     * @tparam T 类型包
     */
    template <typename... T>
    struct index_sequence_for
    {
        typedef typename make_index_sequence<sizeof...(T)>::type type;
    };

} // namespace Evently

#endif // INDEX_SEQUENCE_H
