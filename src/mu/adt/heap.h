// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014  Migrant Coder

#pragma once

#include <queue>

#include <mu/alg/heap.h>

namespace mu {
namespace adt {

/// A minimum binary heap backed by \c std::deque.
///
/// Time complexity is O(log(n)) for inserts and removal and constant time for
/// access. Space complexity is as per \c std::deque.
template <typename T>
class heap {
public:
    heap() = default;
    ~heap() = default;
    heap(const heap&) = default;
    heap(heap&& o) : heap_(std::move(o.heap_)) {}
    heap& operator=(const heap&) = default;
    heap& operator=(heap&& o) { heap_ = std::move(o.heap_); }

    /// Move \e into the instance.
    void emplace(T&& e) { mu::alg::heap::emplace(heap_, std::move(e)); }

    bool empty() const { return heap_.empty(); }

    /// Remove the minimum element.
    /// \pre \c !empty()
    void pop() { mu::alg::heap::pop(heap_); }

    void push(const T& e) { mu::alg::heap::push(heap_, e); }
    size_t size() const { return heap_.size(); }

    /// \pre \c !empty()
    /// \return a referencethe minimum element.
    const T& top() const { return mu::alg::heap::top(heap_); }

    /// \pre \c !empty()
    /// \return a referencethe minimum element.
    T& top() { return mu::alg::heap::top(heap_); }

    /// \exception \c std::logic_error if any invariants don't hold.
    /// \note Recursive and not tail call optimized.
    void validate() const;

private:
    std::deque<T> heap_;
};

} // namespace adt
} // namespace mu
