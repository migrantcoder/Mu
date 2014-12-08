// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#include <atomic>
#include <cassert>
#include <cstddef>

#include <mu/lf/stack.h>
#include <mu/lf/tag.h>
#include <mu/optional.h>

namespace mu {
namespace lf {

/// An intrusive lock free FIFO queue based on "Simple, Fast, and Practical
/// Non-Blocking / and Blocking Concurrent Queue Algorithms" by Michael and Scott.
///
/// Instances are also non-blocking for reasonable schedulers.
///
/// Methods provide the strong exception guarantee.  That is if a method
/// throws an exception, the instance will be left in the pre invocation state
/// on return.  Exceptions are limited to those thrown by \c T's copy and move
/// constructors and assigment operators and destructor, as well as \c new.
///
/// Memory is allocated on construction for an initial capacity.  Allocation and
/// deallocation are not required if this capacity is not exceeded and T is
/// moveable.
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
public:
    typedef T value_type;

    constexpr static const size_t DEFAULT_INITIAL_CAPACITY = 8192;

    /// Construct with the specified initial capacity.
    ///
    /// \param initial_capacity the initial capacity in number of elements.  The
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

    /// Move an element onto the queue.
    ///
    /// \param e the value to queue.
    /// \post \c in has been moved and will empty or in a state defined by Ts
    ///       move constructor.
    void emplace(T&& e);

    /// Copy an element onto the queue.
    ///
    /// \param e the value to queue.
    /// \post \e is unchanged.
    void push(const T& e);

    /// \return \c true iff the queue has no elements available for dequeueing.
    bool empty() const;

    size_t capacity() const { return capacity_; }

    void print(std::ostream&) const;

private:
    template<typename>
    friend std::ostream& operator<<(std::ostream&, const queue&);

    /// A queue element.  Linkable for instrusive \c mu::lf::stack use.
    struct element {
        element() : value_(), next_(nullptr) {}
        T value_;
        std::atomic<element*> next_;
    };

    void destroy() noexcept;            /// Destroy the instance by freeing all resources.
    void alloc_element(element*&);      /// Get a free element, increasing capacity if necessary.
    void free_element(element*);        /// Release to pool of free elements.
    bool dequeue(T&);
    void enqueue(element* const) noexcept;

    std::atomic<size_t> capacity_;      /// Total capacity, free + used elements.
    std::atomic<element*> head_;        /// Sentinel.  head_->next_ points to first element.
    std::atomic<element*> tail_;        /// Tail.  If empty or singleton point to head_->next_.
    stack<element> free_;               /// Free element list.
};

template <typename T>
void queue<T>::destroy() noexcept
{
    assert(empty());

    while (!free_.empty()) {
        element* e;
        free_.pop(e);
        delete untag(e);
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
            free_.push(new element());
        }
        element* e = nullptr;
        alloc_element(e);
        head_ = e;
        tail_ = e;
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
void queue<T>::alloc_element(element*& e)
{
    if (!free_.pop(e)) {
        e = new element();
        ++capacity_;
    }
}


template <typename T>
void queue<T>::free_element(element* const e)
{
    assert(untag(e));
    free_.push(e);
}

template <typename T>
void queue<T>::push(T const & value)
{
    element* e = nullptr;
    alloc_element(e);
    try {
        untag(e)->value_ = value;
    } catch (...) {
        free_element(e);
        throw;
    }
    enqueue(e);
}

template <typename T>
void queue<T>::emplace(T&& value)
{
    element* e = nullptr;
    alloc_element(e);
    try {
        untag(e)->value_ = std::move(value);
    } catch (...) {
        delete e;
        throw;
    }
    enqueue(e);
}

template <typename T>
void queue<T>::enqueue(element* const e) noexcept
{
    element* t = nullptr;
    element* n = nullptr;
    untag(e)->next_ = nullptr;

    while (true) {
        t = tail_;              // The (t)ail.
        n = untag(t)->next_;    // The (n)ext element.

        // Verify read of tail_ and tail_->next_ is consistent.
        if (t != tail_)
            continue;

        if (untag(n) == nullptr) {
            // Attempt to link in the new element.
            if (untag(t)->next_.compare_exchange_strong(n, tag(e, get_tag(n) + 1)))
                break;
        } else {
            // The tail pointer has fallen behind, attempt to move it along.
            tail_.compare_exchange_strong(t, tag(n, get_tag(t) + 1));
            continue;
        }
    }

    // If this update fails, the next en/dequeue will update the tail pointer.
    tail_.compare_exchange_strong(t, tag(e, get_tag(t) + 1));
}

template <typename T> bool queue<T>::pop(T& out) { return dequeue(out); }

template <typename T>
optional<T> queue<T>::pop()
{
    using std::experimental::make_optional;

    T value;
    if (dequeue(value))
        return make_optional<T>(std::move(value));
    return optional<T>();
}

template <typename T>
bool queue<T>::dequeue(T& value)
{
    while (true) {
        // Read the state in an order allowing consistency verification.
        element* h = head_;                 // The (h)ead.
        element* t = tail_;                 // The (t)ail.
        element* n = untag(h)->next_;       // The (n)ext element.

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

        // Copy out the first element's value and dequeue it.
        value = std::move(untag(n)->value_);
        auto old = h;
        if (!head_.compare_exchange_strong(h, tag(n, get_tag(h) + 1))) {
            continue;
        }
        // Free the old head.
        free_element(old);
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
