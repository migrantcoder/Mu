// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>

namespace mu {
namespace lf {
namespace impl {

/// Lock free stack implemented by an intrusive singly linked list.
///
/// \tparam T must be linkable to another instance of T by defining a public
///         field \code std::atomic<T*> next_ \endcode.
template <typename T>
class stack {
public:
    typedef T value_type;

    /// \pre  \c std::atomic<T*>::is_lock_free() is \c true.
    stack() : head_(nullptr) { assert( head_.is_lock_free()); }
    stack(const stack&) = delete;
    stack& operator=(const stack&) = delete;
    /// \pre is \c empty()
    ~stack() { assert(empty()); }

    /// \param t a valid pointer to a \c T t.
    void push(T* t);

    /// Attempt to pop the top of the stack.
    ///
    /// \param out The top iff successful, else \c nullptr.
    /// \return \c true iff successful.
    ///
    /// \post \code out == nullptr || out->next_ == nullptr \endcode 
    bool pop(T*& out);

    bool empty() const { return head_ == nullptr; }

    /// Not safe for concurrent invocation.
    void for_each(const std::function<void (T*)>& ) const;

private:
    std::atomic<T*> head_;
};

template <typename T>
void stack<T>::push(T* const e)
{
    assert(e != nullptr);

    while (true) {
        T* h = head_;
        e->next_ = h;
        if (head_.compare_exchange_strong(h, e)) {
            break;
        }
    }
}

template <typename T>
bool stack<T>::pop(T*& e)
{
    while (true) {
        T* h = head_;
        if (h == nullptr)
            return false;
        T* n = h->next_;
        if (head_.compare_exchange_strong(h, n)) {
            e = h;
            e->next_ = nullptr;
            return true;
        }
    }
}

template <typename T>
void stack<T>::for_each(const std::function<void (T*)>& f) const
{
    for (T* t = head_; t != nullptr; t = t->next_) {
        f(t);
    }
}

} // namespace impl
} // namespace lf
} // namespace mu
