// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>

#include <mu/lf/tag.h>

namespace mu {
namespace lf {
namespace impl {

/// Lock free stack implemented by an intrusive singly linked list.
///
/// \remark Pointer counting (tagging) is used to avoid ABA problems such as the
///         following.  Consider a pop operation on the stack \c A->B->C.
///         The thread reads \c &A and \c &B into local variables \c head and \c
///         next respectively, before performing an atomic compare and swap
///         operation to set the stack's head pointer to \c &B, i.e.
///         <tt>CAS(stack.head, head, next)</tt>.  Suppose the thread is
///         descheduled just before initiating the CAS operation, and that
///         whilst the thread sleeps, the stack is changed to \c A->C via the
///         removal of \c A and \c B and the subsequened pushing of \c A.  On
///         resuming, the thread executes the CAS successfully, \c A is still
///         the head after all, leaving the stack head pointing to \c B.  Either
///         the stack is now in the invalid state \c B->C, invalid becase \c B
///         has already been removed, or, worse still, invalid because \c B has
///         been freed by the caller and is no longer valid  memory.
///
/// \tparam T Must be linkable to another instance of T by defining a public
///         field \c std::atomic<T*> \c next_.
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

    bool empty() const { return untag(head_.load()) == nullptr; }

    /// Not safe for concurrent invocation.
    void for_each(const std::function<void (T*)>& ) const;

private:
    std::atomic<T*> head_;
};

template <typename T>
void stack<T>::push(T* const e)
{
    using mu::lf::tag;

    assert(e != nullptr);

    while (true) {
        T* h = head_;
        untag(e)->next_ = h;
        if (head_.compare_exchange_strong(h, inctag(e)))
            break;
    }
}

template <typename T>
bool stack<T>::pop(T*& e)
{
    using mu::lf::tag;
    using mu::lf::untag;

    while (true) {
        T* h = head_;
        if (untag(h) == nullptr)
            return false;
        T* n = untag(h)->next_;
        if (head_.compare_exchange_strong(h, inctag(n))) {
            e = h;
            return true;
        }
    }
}

template <typename T>
void stack<T>::for_each(const std::function<void (T*)>& f) const
{
    for (T* t = head_; t != nullptr; t = untag(t)->next_)
        f(t);
}

} // namespace impl
} // namespace lf
} // namespace mu
