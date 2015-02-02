// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

#include <mu/lf/stack.h>
#include <mu/optional.h>

namespace mu {
namespace lf {

/// A lock free queue.
///
/// The implementation is based on "Simple, Fast, and Practical Non-Blocking and
/// Blocking Concurrent Queue Algorithms" by Michael and Scott.  The
/// implementation uses a lock free stack for the node free list.
///
/// Memory is allocated on construction to provide initial capacity.  Allocation
/// and deallocation are not required if this capacity is not exceeded.
///
/// Mutating methods provide the strong exception safety guarantee.
///
/// Raised exceptions are limited to memory allocation exceptions and those
/// thrown by \c T's copy and move constructors and assignment operators.
///
/// \tparam T must be default constructable, assignable and copy constructable.
//          To realize maximum efficiency, T should be move assignable and
///         constructable.
///
/// \internal The implementation is based on "Simple, Fast, and Practical
///           Non-Blocking and Blocking Concurrent Queue Algorithms" by Michael
///           and Scott.  A sentinel head node is used.  Elements are enqueued
///           after the tail, and dequeued after the head.
///
/// \internal Providing strong exception safety requires protection where T
///           methods are invoked and when allocating and freeing memory.
template <typename T>
class queue {
private:
    struct node;

public:
    using value_type = T;

    constexpr static const size_t DEFAULT_INITIAL_CAPACITY = 8192;

    /// Construct with the specified initial capacity.
    ///
    /// \param initial_capacity the initial capacity in number of nodes.  The
    ///        total is roughly \code initial_capacity * (sizeof(T) +
    //         sizeof(T*)) \endcode bytes.
    queue(size_t initial_capacity);

    /// Construct with the default initial capacity.
    queue();

    /// \pre \c empty() is \c true
    ~queue();
    queue& operator=(const queue&) = delete;

    /// Remove the head of the queue.
    ///
    /// \pre \c out is an empty instance of \c T.
    /// \return \c true iff a valid, T value was assigned to \c out.
    bool pop(T& out);

    /// Remove the head of the queue.
    ///
    /// \return a valid, T value, or \c false.
    optional<T> pop();

    /// Move an node onto the queue.
    ///
    /// \param e the value to queue.
    /// \post \c in has been moved and will empty or in a state defined by Ts
    ///       move constructor.
    void emplace(T&& e);

    /// Copy an node onto the queue.
    ///
    /// \param e the value to queue.
    /// \post \e is unchanged.
    void push(const T& e);

    /// \return \c true iff the queue has no nodes available for dequeueing.
    bool empty() const;

    size_t capacity() const { return capacity_; }

    void print(std::ostream&) const;

private:
    template<typename>
    friend std::ostream& operator<<(std::ostream&, const queue&);

    /// A queue node.  Linkable for instrusive \c mu::lf::stack use.
    struct node {
        node() : value_(), next_(nullptr) {}
        T value_;
        tagged_ptr<node> next_;
    };

    void destroy() noexcept;            /// Free all instance resources.
    tagged_ptr<node> alloc_node();      /// Return a free or newly allocated node.
    void free_node(tagged_ptr<node>);   /// Release to pool of free nodes.
    bool dequeue(T&);
    void enqueue(tagged_ptr<node>) noexcept;

    std::atomic<size_t> capacity_;  /// Total capacity, free + used nodes.
    tagged_ptr<node> head_;         /// Sentinel.  head_->next_ points to first.
    tagged_ptr<node> tail_;         /// Tail.  Points head_->next_ if empty.
    stack<tagged_ptr<node>> free_;  /// Free node list.
};

template <typename T>
void queue<T>::destroy() noexcept
{
    assert(empty());

    while (!free_.empty()) {
        tagged_ptr<node> n;
        free_.pop(n);
        delete n;
    }
}

template <typename T>
queue<T>::queue(size_t const initial_capacity_count) :
        capacity_(initial_capacity_count),
        head_(),
        tail_(),
        free_()
{
    // Provision initial, free capacity.
    try {
        for (size_t i = 0; i < initial_capacity_count; ++i) {
            tagged_ptr<node> n(new node());
            free_.push(n);
        }
        tagged_ptr<node> n(alloc_node());
        head_ = n;
        tail_ = n;
    } catch (...) {
        destroy();
        throw;
    }
}

template <typename T> queue<T>::queue() : queue(DEFAULT_INITIAL_CAPACITY) {}

template <typename T> queue<T>::~queue() { destroy(); }

template <typename T>
tagged_ptr<typename queue<T>::node> queue<T>::alloc_node()
{
    tagged_ptr<node> n;
    if (!free_.pop(n)) {
        n = new node();
        ++capacity_;
    }
    return n;
}


template <typename T>
void queue<T>::free_node(tagged_ptr<node> e)
{
    free_.push(e);
}

template <typename T>
void queue<T>::push(T const & value)
{
    tagged_ptr<node> n = alloc_node();
    try {
        n->value_ = value;
    } catch (...) {
        free_node(n);
        throw;
    }
    enqueue(n);
}

template <typename T>
void queue<T>::emplace(T&& value)
{
    tagged_ptr<node> n = alloc_node();
    try {
        n->value_ = std::move(value);
    } catch (...) {
        delete n;
        throw;
    }
    enqueue(n);
}

template <typename T>
void queue<T>::enqueue(tagged_ptr<node> const n) noexcept
{
    tagged_ptr<node> tail;
    tagged_ptr<node> next;
    n->next_ = nullptr;

    while (true) {
        tail = tail_;
        next = tail->next_;

        // Verify read of tail_ and tail_->next_ is consistent.
        if (tail != tail_)
            continue;

        if (!next) {
            // Attempt to link in the new node.
            if (tail->next_.compare_set_strong(next, n.set_tag(next).increment_tag()))
                break;
        } else {
            // The tail pointer has fallen behind, attempt to move it along.
            tail_.compare_set_strong(tail, next.set_tag(tail).increment_tag());
            continue;
        }
    }

    // If this update fails, the next en/dequeue will update the tail pointer.
    tail_.compare_set_strong(tail, n.set_tag(tail).increment_tag());
}

template <typename T> bool queue<T>::pop(T& out) { return dequeue(out); }

template <typename T>
optional<T> queue<T>::pop()
{
    using std::experimental::make_optional;
    using std::move;

    T value;
    if (dequeue(value))
        return make_optional<T>(move(value));
    return optional<T>();
}

template <typename T>
bool queue<T>::dequeue(T& value)
{
    while (true) {
        // Read the state in an order allowing consistency verification.
        tagged_ptr<node> h = head_;     // The (h)ead.
        tagged_ptr<node> t = tail_;     // The (t)ail.
        tagged_ptr<node> n = h->next_;  // The (n)ext node.

        // Verify read of head_, tail_ and head_->next_ is consistent.
        if (h != head_)
            continue;

        if (h == t) {
            if (!n) {
                // The queue is empty.
                return false;
            } else {
                // The tail pointer has fallen behind, attempt to move it along.
                tail_.compare_set_strong(t, n.set_tag(t).increment_tag());
                continue;
            }
        }

        assert(n);

        // Copy out the first node's value and dequeue it.
        // If T's copy assignment operator throws, the queue state is unchanged.
        value = n->value_;
        auto old = h;
        if (!head_.compare_set_strong(h, n.set_tag(h.increment_tag())))
            continue;

        // Free the old head.
        free_node(old);
        return true;
    }
}

template <typename T>
bool queue<T>::empty() const
{
    return head_ == tail_;
}

template <typename T>
void queue<T>::print(std::ostream& os) const
{
    os << "q={";
    for (auto i = head_; i != nullptr; i = i->next_)
        os << i->value_.id_ << ", ";
    os << "}";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const queue<T>& q)
{
    q.print(os);
    return os;
}

} // namespace lf
} // namespace mu
