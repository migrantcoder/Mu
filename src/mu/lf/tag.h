// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <algorithm>

/// \file
///
/// Pointer counting is implemented by tagging the unused high (most
/// significant) bits of pointers.  On x86-64 where only the low 48 bits of 64
/// bit pointer types are used, the tags are 16 bits wide allowing for counts of
/// up to 65535.

namespace mu {
namespace lf {

typedef uint16_t tag_t;
constexpr const size_t PTR_SIZE = sizeof(void *);
constexpr const size_t TAG_SIZE = sizeof(tag_t);
constexpr const size_t TAG_SHIFT_COUNT = (PTR_SIZE - TAG_SIZE) * 8;

static_assert(PTR_SIZE == 8, "void* size not 64 bits");
static_assert(TAG_SIZE == 2, "tag_t size not 16 bits");
static_assert(TAG_SHIFT_COUNT == 48, "tag shif not 48 bits");

/// Mask the bits that can be used tagging on x86_64.
constexpr const uintptr_t TAG_MASK = std::numeric_limits<uintptr_t>::max() << TAG_SHIFT_COUNT;

/// Clear the pointer's tag.
///
/// \param p
/// \post \code p & TAG_MASK == 0 \endcode.
template <typename T> T* untag(const T* p);

/// Get the pointer's current tag.
///
/// \param p
/// \return the tag from pointer \c p.
template <typename T> tag_t get_tag(const T* const);

/// Increment pointer \c p's tag.
template <typename T>
T* inctag(const T* const p)
{
    return tag(p, get_tag(p) + 1);
}

/// Set a pointer's tag with the specified tag.
///
/// \param p A pointer.
/// \param tag A tag.
/// \return \c p tagged with \c tag.
template <typename T>
T* tag(const T* const p, const tag_t t)
{
    const uintptr_t up = reinterpret_cast<uintptr_t>(untag(p));
    const uintptr_t ut = static_cast<uintptr_t>(t) << TAG_SHIFT_COUNT; 
    return reinterpret_cast<T*>(up | ut);
}

/// \return pointer \c p's tag.
template <typename T>
tag_t get_tag(const T* const p)
{
    return static_cast<tag_t>(reinterpret_cast<uintptr_t>(p) >> TAG_SHIFT_COUNT);
}

template <typename T>
T* untag(const T* const p)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) & ~TAG_MASK);
}

} // namespace lf
} // namespace mu
