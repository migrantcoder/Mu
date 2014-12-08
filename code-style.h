// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once    // Preferred to #ifndef ...

#include <algorithm>

/// Header file declaring and defining classes, namespaces and non-member
/// functions.
///
/// This file would normally be named mu/n/c.h as dictated by the namespaces and
/// class it declares.

// Namespaces are declared one per line and are not indented.
namespace mu {  
namespace n {

/// Class \c c comment.
///
/// Class definitions are not indented from their parent namespace to save
/// horizontal space.
///
/// Class and type names are snake case.
///
/// Doxygen comments are preceded by "///". Don't use "/*...*/" or "//!", and
/// keywords are preceded by a '\' not a '@'.
///
/// \tparam Int template parameters are the only names that are camel case.
template <typename Int>
class c {
public:
    c() : m_(start_value) {}        // One-liners may be defined inline.
    c(const Int& m) : m_(m) {}
    c(const c& lhs) = default;      // Explicit generated methods.
    c(c&& lhs);
    ~c() = default;

    /// Comments (Doxygen or otherwise) may precede a member ...
    c& operator=(const c&) = default;   
    c& operator=(c&&);              /// ... or follow it.

    Int m() const { return m_; }    // No "get" or "set" prefix for accessors.
    Int m(int m) { m_ = m; }

    c get_next() const;             // May use "get" prefix when work is done.
    c get_previous() const;         // Functions are snake case.

private:
    // Variable and constant names are snake case.
    constexpr static const Int start_value = 0;

    Int m_;                         // Member data has a trailing underscore.
};

// One liners may be on one line ...
template <typename Int>
c<Int>::c(c<Int>&& lhs) { std::swap(m_, lhs.m_); }

// ... but are often easier to read on multiple lines.
template <typename Int>
c<Int> c<Int>::get_next() const
{ 
    return c<Int>(m_ + 1);
}

// A function's opening braces are on a new line.
template <typename Int>
c<Int> c<Int>::get_previous() const
{ 
    return c<Int>(m_ + 1);
}

template <typename Int>
c<Int>& c<Int>::operator=(c&& lhs)
{
    std::swap(m_, lhs.m_);
    return *this;
}

} // namespace n
} // namespace mu
