// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2015 Migrant Coder

#pragma once

#include <atomic>

namespace mu {

/// Select implementation for architecture.
namespace arch {
#if defined(__amd64__) || defined(__x86_64__) || defined(_M_AMD64)
    constexpr static const size_t MAX_TAG = 0xffff;
    constexpr static const uintptr_t MASK = 0xffff'0000'0000'0000;
#elif defined(__i386__) || defined(_M_IX86) || defined(i386)
    constexpr static const size_t MAX_TAG = 0x03;
    constexpr static const uintptr_t MASK = 0x03;
#else
    static_assert(false, "unsupported platform");
#endif
    template<typename T> size_t tag(T*);
    template<typename T> T* tag(T*, size_t);
    template<typename T> T* untag(T*);
}

/// A tagged pointer suitable for counting pointers for ABA protection.
///
/// Methods are provided to manipulate the tag bits and atomically compare and
/// set instance values.
///
/// Platform support: x86_64 and i386
///
/// \tparam The type of the object instances point to.
template <typename T>
class tagged_ptr {
public:
    constexpr static const size_t MAX_TAG = arch::MAX_TAG;

    tagged_ptr() : ptr_() { ptr_ = nullptr; }
    explicit tagged_ptr(T* ptr) { ptr_ =  ptr; }
    tagged_ptr(const tagged_ptr& o) { ptr_.store(o.ptr_); }
    tagged_ptr& operator=(const tagged_ptr&);
    tagged_ptr& operator=(T* ptr) { ptr_.store(ptr); return *this; }
    ~tagged_ptr() = default;

    ///\return \c true iff the atomic operations on instances are lock free.
    bool is_lock_free() const { return ptr_.is_lock_free(); }

    /// Atomically compare \c this with \c expected and iff equal set former to
    /// latter.
    ///
    /// \return \c true iff \c this was set to \c desired.
    bool compare_set_strong(tagged_ptr expected, tagged_ptr desired);

    /// \return a copy of this instance but with the tag set to \c o.get_tag().
    tagged_ptr set_tag(const tagged_ptr& o) const;

    /// \return a copy of this instance with the tag = tag + 1 mod MAX_TAG.
    tagged_ptr increment_tag() const;

    /// \return the value of the tag.
    size_t get_tag() const;

    operator bool() const { return arch::untag(ptr_.load()) != nullptr; }
    operator T*() const { return arch::untag(ptr_.load()); }
    T& operator*() { return *arch::untag(ptr_.load()); }
    const T& operator*() const { return *arch::untag(ptr_.load()); }
    T* operator->() { return arch::untag(ptr_.load()); }
    T const * operator->() const { return arch::untag(ptr_.load()); }
    bool operator==(const tagged_ptr&) const;
    bool operator!=(const tagged_ptr&) const;

private:
    std::atomic<T*> ptr_;
};

template<typename T>
bool tagged_ptr<T>::compare_set_strong(
        tagged_ptr<T> expected,
        tagged_ptr<T> desired)
{
    T* e = expected.ptr_.load();
    T* d = desired.ptr_.load();
    return ptr_.compare_exchange_strong(e, d);
}

template<typename T>
tagged_ptr<T> tagged_ptr<T>::set_tag(const tagged_ptr<T>& o) const
{
    return tagged_ptr(arch::tag(ptr_.load(), o.get_tag()));
}

template<typename T>
tagged_ptr<T> tagged_ptr<T>::increment_tag() const
{
    return tagged_ptr(arch::tag(ptr_.load(), get_tag() + 1));
}

template<typename T>
size_t tagged_ptr<T>::get_tag() const
{
    return arch::tag(ptr_.load());
}

template<typename T>
tagged_ptr<T>& tagged_ptr<T>::operator=(const tagged_ptr<T>& o)
{
    ptr_.store(o.ptr_);
    return *this;
}

template<typename T>
bool tagged_ptr<T>::operator==(const tagged_ptr<T>& o) const
{
    return ptr_.load() == o.ptr_.load();
}

template<typename T>
bool tagged_ptr<T>::operator!=(const tagged_ptr<T>& o) const
{
    return ptr_.load() != o.ptr_.load();
}

#if defined(__amd64__) || defined(__x86_64__) || defined(_M_AMD64)
namespace arch {
    template<typename T> size_t tag(T* ptr)
    {
        return reinterpret_cast<uintptr_t>(ptr) >> 48;
    }

    template<typename T> T* tag(T* ptr, size_t tag)
    {
        return reinterpret_cast<T*>(
                reinterpret_cast<uintptr_t>(untag(ptr)) | (tag << 48));
    }

    template<typename T> T* untag(T* ptr)
    {
        return reinterpret_cast<T*>(
                reinterpret_cast<uintptr_t>(ptr) & ~MASK);
    }
}
#elif defined(__i386__) || defined(_M_IX86) || defined(i386)
namespace arch {
    template<typename T> size_t tag(T* ptr)
    {
        return reinterpret_cast<uintptr_t>(ptr) & MASK;
    }

    template<typename T> T* tag(T* ptr, size_t tag)
    {
        return reinterpret_cast<T*>(
                reinterpret_cast<uintptr_t>(untag(ptr)) | (tag & MASK));
    }

    template<typename T> T* untag(T* ptr)
    {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) & ~MASK);
    }
}
#else
static_assert(false, "unsupported platform");
#endif

} // namespace mu
