// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2015 Migrant Coder

#pragma once

#include <atomic>

namespace mu {

/// A tagged pointer for ABA protection.
///
/// Methods are provided to manipulate the tag bits and atomically compare and
/// swap instances.
///
/// Platform support: x86_64
///
/// \tparam The type of the object pointer instances point to.
template <typename T>
class tagged_ptr {
public:
    constexpr static const size_t max_tag = 1 << 16;

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
    /// \return \c true iff \c this was set to \c desired.
    bool compare_set_strong(tagged_ptr expected, tagged_ptr desired);

    /// \return an instance with \code tag() == o.tag() && ptr() == o.ptr()
    /// \endcode.
    tagged_ptr set_tag(const tagged_ptr& o) const;

    /// \return an instance with \code tag() == this->tag() + 1 \endcode.
    tagged_ptr increment_tag() const;

    /// \return the value of the tag.
    size_t get_tag() const;

    operator bool() const { return untag(ptr_.load()) != nullptr; }
    operator T*() const { return untag(ptr_); }
    T& operator*() { return *untag(ptr_); }
    const T& operator*() const { return *untag(ptr_); }
    T* operator->() { return untag(ptr_); }
    T const * operator->() const { return untag(ptr_); }
    bool operator==(const tagged_ptr&) const;
    bool operator!=(const tagged_ptr&) const;

private:

    static T* tag(T* ptr, size_t tag);
    static T* untag(T* ptr);

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
    return tagged_ptr(tag(ptr_, o.get_tag()));
}

template<typename T>
tagged_ptr<T> tagged_ptr<T>::increment_tag() const
{
    return tagged_ptr(tag(ptr_, get_tag() + 1));
}

template<typename T>
size_t tagged_ptr<T>::get_tag() const
{
    return reinterpret_cast<uintptr_t>(ptr_.load()) >> 48;
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

template<typename T>
T* tagged_ptr<T>::tag(T* ptr, size_t tag)
{
    return reinterpret_cast<T*>(
            reinterpret_cast<uintptr_t>(untag(ptr)) | (tag << 48));
}

template<typename T>
T* tagged_ptr<T>::untag(T* ptr)
{
    return reinterpret_cast<T*>(
            reinterpret_cast<uintptr_t>(ptr) & 0x0000'ffff'ffff'ffff);
}

} // namespace mu
