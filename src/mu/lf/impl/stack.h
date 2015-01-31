// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>

#include <mu/tagged_ptr.h>

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
    /// \pre  \c std::atomic<T*>::is_lock_free() is \c true.
    stack() : head_(nullptr) { assert(head_.is_lock_free()); }
    stack(const stack&) = delete;
    stack& operator=(const stack&) = delete;
    /// \pre is \c empty()
    ~stack() { assert(empty()); }

    /// \param t a valid pointer to a \c T t.
    void push(tagged_ptr<T> t);

    /// Attempt to pop the top of the stack.
    ///
    /// \param out The top iff successful, else \c nullptr.
    /// \return \c true iff successful.
    ///
    /// \post \code out == nullptr || out->next_ == nullptr \endcode 
    bool pop(tagged_ptr<T>& out);

    bool empty() const { return !head_; }

    /// Not safe for concurrent invocation.
    void for_each(const std::function<void (T*)>& ) const;

private:
    tagged_ptr<T> head_;
};

template <typename T>
void stack<T>::push(tagged_ptr<T> e)
{
    while (true) {
        // Link the new element to a snapshot of the head. Attempt to make the
        // new element the head, or repeat if the snapshot has been invalidated.
        auto h = head_;
        e->next_ = h;
        if (head_.compare_set_strong(h, e.increment_tag()))
            break;
    }
}

template <typename T>
bool stack<T>::pop(tagged_ptr<T>& e)
{
    while (true) {
        // Snapshot head pointer before attempting to detach the head element by
        // setting the heade pointer to snapshot's next pointer.
        auto h = head_;
        if (!h)
            return false;                                       // Empty stack.
        auto n = h->next_;
        if (head_.compare_set_strong(h, n.increment_tag())) {
            e = h;
            return true;
        }
    }
}

template <typename T>
void stack<T>::for_each(const std::function<void (T*)>& f) const
{
    for (auto t = head_; t != nullptr; t = t->next_)
        f(t);
}

} // namespace impl
} // namespace lf
} // namespace mu
