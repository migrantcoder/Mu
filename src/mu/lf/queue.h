// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

#include <mu/lf/stack.h>
#include <mu/lf/tag.h>
#include <mu/optional.h>

namespace mu {
namespace lf {

/// A lock free queue.
///
/// The implementation is based on "Simple, Fast, and Practical Non-Blocking and
/// Blocking Concurrent Queue Algorithms" by Michael and Scott.  The
/// implementation uses a lock free stack for the node free list.
///
/// Memory is allocated on construction to provide initial capacity.  Allocation and
/// deallocation are not required if this capacity is not exceeded.
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
    typedef T value_type;

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
        std::atomic<node*> next_;
    };

    void destroy() noexcept;        /// Destroy the instance by freeing all resources.
    node* alloc_node();             /// Get a free node, increasing capacity if necessary.
    void free_node(node*);          /// Release to pool of free nodes.
    bool dequeue(T&);
    void enqueue(node* const) noexcept;

    std::atomic<size_t> capacity_;  /// Total capacity, free + used nodes.
    std::atomic<node*> head_;       /// Sentinel.  head_->next_ points to first node.
    std::atomic<node*> tail_;       /// Tail.  If empty or singleton point to head_->next_.
    stack<node*> free_;             /// Free node list.
};

template <typename T>
void queue<T>::destroy() noexcept
{
    assert(empty());

    while (!free_.empty()) {
        node* n;
        free_.pop(n);
        delete untag(n);
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
            free_.push(new node());
        }
        node* n = alloc_node();
        head_ = n;
        tail_ = n;
    } catch (...) {
        destroy();
        throw;
    }

    assert(untag(head_.load())->next_ == nullptr);
    assert(untag(tail_.load())->next_ == nullptr);
    assert(untag(head_.load()) == untag(tail_.load()));
}

template <typename T> queue<T>::queue() : queue(DEFAULT_INITIAL_CAPACITY) {}

template <typename T> queue<T>::~queue() { destroy(); }

template <typename T>
typename queue<T>::node* queue<T>::alloc_node()
{
    node* n = nullptr;
    if (!free_.pop(n)) {
        n = new node();
        ++capacity_;
    }
    return n;
}


template <typename T>
void queue<T>::free_node(node* const e)
{
    assert(untag(e));
    free_.push(e);
}

template <typename T>
void queue<T>::push(T const & value)
{
    node* n = alloc_node();
    try {
        untag(n)->value_ = value;
    } catch (...) {
        free_node(n);
        throw;
    }
    enqueue(n);
}

template <typename T>
void queue<T>::emplace(T&& value)
{
    node* n = alloc_node();
    try {
        untag(n)->value_ = std::move(value);
    } catch (...) {
        delete n;
        throw;
    }
    enqueue(n);
}

template <typename T>
void queue<T>::enqueue(node* const n) noexcept
{
    node* tail = nullptr;
    node* next = nullptr;
    untag(n)->next_ = nullptr;

    while (true) {
        tail = tail_;               // The (t)ail.
        next = untag(tail)->next_;  // The (n)ext node.

        // Verify read of tail_ and tail_->next_ is consistent.
        if (tail != tail_)
            continue;

        if (untag(next) == nullptr) {
            // Attempt to link in the new node.
            auto tagged_node = tag(n, get_tag(next) + 1);
            if (untag(tail)->next_.compare_exchange_strong(next, tagged_node))
                break;
        } else {
            // The tail pointer has fallen behind, attempt to move it along.
            tail_.compare_exchange_strong(tail, tag(next, get_tag(tail) + 1));
            continue;
        }
    }

    // If this update fails, the next en/dequeue will update the tail pointer.
    tail_.compare_exchange_strong(tail, tag(n, get_tag(tail) + 1));
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
        node* h = head_;                 // The (h)ead.
        node* t = tail_;                 // The (t)ail.
        node* n = untag(h)->next_;       // The (n)ext node.

        // Verify read of head_, tail_ and head_->next_ is consistent.
        if (h != head_)
            continue;

        if (h == t) {
            if (untag(n) == nullptr) {
                // The queue is empty.
                return false;
            } else {
                // The tail pointer has fallen behind, attempt to move it along.
                tail_.compare_exchange_strong(t, tag(n, get_tag(t) + 1));
                continue;
            }
        }

        // Copy out the first node's value and dequeue it.
        // If T's copy assignment operator throws, the queue state is unchanged.
        value = untag(n)->value_;
        auto old = h;
        if (!head_.compare_exchange_strong(h, tag(n, get_tag(h) + 1))) {
            continue;
        }
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
    for (auto i = head_.load(); i != nullptr; i = untag(i)->next_) {
        os << untag(i)->value_.id_ << ", ";
    }
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
