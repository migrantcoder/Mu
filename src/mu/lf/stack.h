// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

#include <mu/optional.h>
#include <mu/lf/impl/stack.h>

namespace mu {
namespace lf {

/// A lock-free unbounded stack.
///
/// Memory is allocated on construction to provide initial capacity.  Allocation and
/// deallocation are not required if this capacity is not exceeded.
///
/// The \c emplace(T&&) and \code option<t> pop() \endcode methods provide the
/// strong exception safety guarantee.
///
/// Raised exceptions are limited to memory allocation exceptions and those
/// thrown by \c T's copy and move constructors and assignment operators.
///
/// \tparam T The stack element value type.  Must be copy constructable and
///           assignable.  Should be move constructable and assignable.
template <typename T>
class stack {
public:
    using value_type = T;
    constexpr static const size_t DEFAULT_INITIAL_CAPACITY = 8192;

    stack() : stack(DEFAULT_INITIAL_CAPACITY)  {}
    stack(size_t initial_capacity);
    stack(const stack&) = delete;
    ~stack();
    stack& operator=(const stack&) = delete;

    /// Copy a value onto the top of the stack.
    ///
    /// \param v The value to copy. 
    void push(const T& v);

    /// Move a value to the top of the stack.
    ///
    /// \param v The value to move.
    void emplace(T&& v);

    /// Attempt to pop the top of the stack.
    ///
    /// \param out The top value iff successful.
    /// \return \c true on success.
    bool pop(T& out);

    /// Attempt to pop the top of the stack.
    ///
    /// \return engaged optional iff successful.
    optional<T> pop();

    /// \return \c true iff the stack is not empty.
    bool empty() const { return stack_.empty(); }

    /// Not safe for concurrent invocation.
    void for_each(const std::function<void (T&)>& ) const;

private:
    struct node {
        node() = default;
        node(T const & value) : next_(nullptr), value_(value) {}
        node(T&& value) : next_(nullptr), value_(std::move(value)) {}
        node(node const &) = delete;
        node& operator=(node const &) = delete;
        ~node() = default;
        tagged_ptr<node> next_;
        T value_;
    };

    void destroy();

    impl::stack<node> free_;       // List of free nodes.
    impl::stack<node> stack_;      // The stack implementation.
};

template <typename T>
stack<T>::stack(size_t initial_capacity)
{
    try {
        for (size_t i = 0; i < initial_capacity; ++i) {
            tagged_ptr<node> n(new node());
            free_.push(n);
        }
    } catch (...) {
        destroy();
    }
}

template <typename T> stack<T>::~stack() { destroy(); }

template <typename T>
void stack<T>::destroy()
{
    assert(empty());

    tagged_ptr<node> n;
    while (free_.pop(n)) {
        delete n;
    }
}

template <typename T>
void stack<T>::push(T const& v)
{
    tagged_ptr<node> n;
    if (!free_.pop(n))
        n = new node(v);

    n->value_ = v;
    stack_.push(n);
}

template <typename T>
void stack<T>::emplace(T&& v)
{
    tagged_ptr<node> n;
    bool allocated = false;
    if (!free_.pop(n)) {
        n = new node(v);
        allocated = true;
    }

    try {
        n->value_ = std::move(v);
        stack_.push(n);
    } catch (...) {
        if (allocated) {
            delete n;
        } else {
            free_.push(n);
        }
    }
}

template <typename T>
bool stack<T>::pop(T& out)
{
    tagged_ptr<node> n;
    if (stack_.pop(n)) {
        out = n->value_;
        n->value_ = T();
        return true;
    }
    return false;
}

template <typename T>
optional<T> stack<T>::pop()
{
    using std::experimental::make_optional;
    using std::move;

    tagged_ptr<node> n;
    optional<T> t;
    if (stack_.pop(n)) {
        t = make_optional<T>(move(n->value_));
        n->value_ = move(T());
    }
    return t;
}

template <typename T>
void stack<T>::for_each(const std::function<void (T&)>& f) const
{
    stack_.for_each([&f] (node* n) { f(n->value_); });
}

} // namespace lf
} // namespace mu
