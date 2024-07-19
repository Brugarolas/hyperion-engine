/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ATOMIC_VAR_HPP
#define HYPERION_ATOMIC_VAR_HPP

#include <core/Defines.hpp>

#include <atomic>
#include <algorithm>
#include <utility>

namespace hyperion {

enum class MemoryOrder
{
    RELAXED,
    SEQUENTIAL,
    ACQUIRE,
    RELEASE,
    ACQUIRE_RELEASE
};

namespace threading {
namespace detail {

HYP_FORCE_INLINE
static constexpr std::memory_order ToCxxMemoryOrder(MemoryOrder order)
{
    switch (order) {
    case MemoryOrder::RELAXED:
        return std::memory_order_relaxed;
    case MemoryOrder::SEQUENTIAL:
        return std::memory_order_seq_cst;
    case MemoryOrder::ACQUIRE:
        return std::memory_order_acquire;
    case MemoryOrder::RELEASE:
        return std::memory_order_release;
    case MemoryOrder::ACQUIRE_RELEASE:
        return std::memory_order_acq_rel;
    default:
        return std::memory_order_seq_cst;
    }
}
    
} // namespace detail

template <class T>
class AtomicVar
{
    static_assert(
        std::is_integral_v<T> || std::is_pointer_v<T> || std::is_standard_layout_v<T>,
        "T must be a type suitable for atomic intrinsics"
    );

    static_assert(
        sizeof(T) <= 8,
        "T must have a sizeof value of <= 8"
    );

    std::atomic<T> m_value;

public:
    AtomicVar() : m_value { T { } }
    {
    }

    AtomicVar(T value)
        : m_value { value }
    {
    }

    AtomicVar(const AtomicVar &other)                   = delete;
    AtomicVar &operator=(const AtomicVar &other)        = delete;
    AtomicVar(AtomicVar &&other) noexcept               = delete;
    AtomicVar &operator=(AtomicVar &&other) noexcept    = delete;
    ~AtomicVar()                                        = default;

    HYP_FORCE_INLINE T Get(MemoryOrder order) const
        { return m_value.load(detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE void Set(T value, MemoryOrder order)
        { m_value.store(value, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T Exchange(T new_value, MemoryOrder order)
        { return m_value.exchange(new_value, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T Increment(T amount, MemoryOrder order)
        { return m_value.fetch_add(amount, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T Decrement(T amount, MemoryOrder order)
        { return m_value.fetch_sub(amount, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T BitOr(T value, MemoryOrder order)
        { return m_value.fetch_or(value, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T BitAnd(T value, MemoryOrder order)
        { return m_value.fetch_and(value, detail::ToCxxMemoryOrder(order)); }

    HYP_FORCE_INLINE T BitXor(T value, MemoryOrder order)
        { return m_value.fetch_xor(value, detail::ToCxxMemoryOrder(order)); }
};

} // namespace threading

using threading::AtomicVar;

} // namespace hyperion

#endif
