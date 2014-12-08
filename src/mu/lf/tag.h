// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <algorithm>

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

/// Remove tag from pointer \c p.
/// \post \code p & TAG_MASK == 0 \endcode.
template <typename T> T* untag(const T*);

/// \param p a pointer.
/// \return the tag from pointer \c p.
template <typename T> tag_t get_tag(const T* const);

/// Tag or increment the tag of the pointer \c p.
template <typename T>
T* tag(const T* const p)
{
    // Get the current tag and increment it.
    uintptr_t tag = reinterpret_cast<uintptr_t>(p) >> TAG_SHIFT_COUNT;
    ++tag;
    tag <<= TAG_SHIFT_COUNT;

    auto r = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(untag(p)) | tag);
    assert(untag(r)->next_ || !untag(r)->next_);
    return r;
}

/// \param p a pointer.
/// \param tag a tag.
/// \return \p tagged with \c tag.
template <typename T>
T* tag(const T* const p, const tag_t tag)
{
    auto r =
            reinterpret_cast<T*>(
                    reinterpret_cast<uintptr_t>(untag(p)) | (static_cast<uintptr_t>(tag) << TAG_SHIFT_COUNT));
    assert(untag(r) == nullptr || untag(r)->value_.id_ < 0xfffffffff);
    return r;
}

/// \return the tag from pointer \c p.
template <typename T>
tag_t get_tag(const T* const p)
{
    return static_cast<tag_t>(reinterpret_cast<uintptr_t>(p) >> TAG_SHIFT_COUNT);
}

template <typename T>
T* untag(const T* const p)
{
    auto r = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) & ~TAG_MASK);
    if (r == nullptr) {
        return r;
    }
    assert(r == nullptr || r->value_.id_ < 0xfffffffff);
    return r;
}

} // namespace lf
} // namespace mu
