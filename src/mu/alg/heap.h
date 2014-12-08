// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

namespace mu {
namespace alg {

/// Functionality to create and manipulate a minimum binary heap backed by a
/// SequenceContainer with random access, e.g. \c std::deque or \c std::vector.
namespace heap {

/// Insert an element into a heap.
///
/// \param a A sequence elements in heap order.
/// \param e The element to insert.
/// \post \c a contains \c e and is in heap order.
template <typename RandomAccess>
void push(RandomAccess& a, const typename RandomAccess::value_type& e);

/// Move an element into a heap.
/// \see \c push
template <typename RandomAccess>
void emplace(RandomAccess& a, typename RandomAccess::value_type&& e);

/// Remove the minimum element from a heap.
///
/// \param a A sequence of elements in heap order.
/// \post \c does not contain \c e and is in heap order.
template <typename RandomAccess>
void pop(RandomAccess& a);

/// \param a A non empty array of elements in heap order.
/// \return a reference to the minimum element.
template <typename RandomAccess>
typename RandomAccess::value_type& top(RandomAccess& a);

/// Bubble up the last element.
///
/// \param a An array of elements in heap order, with the possible exception of
/// the last one.
/// \post \c a is in heap order.
template <typename RandomAccess>
void bubble_last(RandomAccess& a);

/// Bubble up the last element.
///
/// \param a An array of elements in heap order, with the possible exception of
/// the last one.
/// \post \c a is in heap order.
template <typename RandomAccess>
void sift_first(RandomAccess& a);

/// Validate that the specified array's elements are in heap order.
///
/// \param a An array of elements.
/// \return \c true iff \c a is in head order.
///
/// \note Recursive and not tail-call optimized.
template <typename RandomAccess>
bool validate(const RandomAccess& a);

// Implementation specifics.
namespace impl {

// Don't handle overflow, vector::push_back will raise an exception first.
size_t left_child_index(const size_t i) { return (2 * i) + 1; }
size_t parent_index(const size_t i) { return (i - 1) / 2; }

// Don't handle overflow, vector::push_back will raise an exception first.
size_t right_child_index(const size_t i) { return (2 * i) + 2; }

// Recursive and not a tail-call.
template <typename RandomAccess>
bool validate(const RandomAccess& a, size_t i)
{
    const size_t l = left_child_index(i);
    const size_t r = right_child_index(i);
    if (l >= a.size()) {
        return true;
    }
    if (a[i] > a[l]) {
        return false;
    }
    if (!validate(a, l)) {
        return false;
    }
    if (r >= a.size()) {
        return true;
    }
    if (a[i] > a[r]) {
        return false;
    }
    return validate(a, r);
}

} // namespace impl

template <typename RandomAccess>
void push(RandomAccess& a, const typename RandomAccess::value_type& e)
{
    a.push_back(e);
    bubble_last(a);
}

template <typename RandomAccess>
void emplace(RandomAccess& a, typename RandomAccess::value_type&& e)
{
    a.emplace_back(std::move(e));
    bubble_last(a);
}

template <typename RandomAccess>
void push(RandomAccess& a, typename RandomAccess::value_type&& e)
{
    a.emplace_back(e);
    bubble_last(a);
}

template <typename RandomAccess>
void pop(RandomAccess& a)
{
    assert(!a.empty());

    if (a.size() == 1) {
        a.pop_front();
        return;
    }

    // Replace the first element with the last, breaking ordering, but
    // preserving the shape property. Sift to restore ordering.
    std::swap(a[0], a[a.size() - 1]);
    a.pop_back();
    sift_first(a);
}

template <typename RandomAccess>
void bubble_last(RandomAccess& a)
{
    using namespace impl;

    if (a.empty()) {
        return;
    }

    size_t i = a.size() - 1;
    if (i < 1) {
        return;
    }
    size_t p = parent_index(i);
    while (i > 0 && a[i] < a[p]) {
        std::swap(a[i], a[p]);
        i = p;
        p = parent_index(i);
    }
}

template <typename RandomAccess>
void sift_first(RandomAccess& a)
{
    using namespace impl;
    using namespace std;

    if (a.empty()) {
        return;
    }

    size_t i = 0;
    while (true) {
        auto& element = a[i];
        const size_t l = left_child_index(i);
        const size_t r = right_child_index(i);

        // Base cases.
        if (l >= a.size()) {
            // The end of the heap has been reached.
            break;
        }
        auto& leftchild = a[l];
        if (r >= a.size()) {
            // The left child is the last element in the heap.
            if (leftchild < element) {
                swap(leftchild, element);
            }
            break;
        }
        auto& rightchild = a[r];
        if (element < leftchild && element < rightchild) {
            // The heap invariant holds again.
            break;
        }

        // Iterate. Sift left or right depending on child priority.
        if (leftchild < rightchild) {
            swap(leftchild, element);
            i = l;
        } else {
            swap(rightchild, element);
            i = r;
        }
    }
}

template <typename RandomAccess>
typename RandomAccess::value_type& top(RandomAccess& a)
{
    assert(!a.empty());
    return a[0];
}

template <typename RandomAccess>
bool validate(const RandomAccess& a)
{
    return impl::validate(a, 0);
}

} // namespace heap
} // namespace alg
} // namespace mu
