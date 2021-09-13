/*
 * File: SharedPointer.h
 * Copyright 2016-7 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#pragma once
#include "../util/atomic.h"
#include "../util/directives.h"

#include "detail/FRCManager.h"
#include "AtomicPointer.h"

namespace terrain
{
namespace frc
{

template<class T>
class PrivatePointer;

template<class T>
class SharedPointer;

template<class T>
class AtomicPointer;

/**
 * Reference counted pointer that will only be mutated by one writer at a time.
 *
 * An object of this class will prevent the object/memory pointed to from being
 * collected. Use this more or less as you would a standard smart pointer like
 * shared_pointer.
 *
 * Consider using PrivatePointer instead if you are just traversing a data structure or
 * otherwise require essentially a short-duration pinning of memory, rather than
 * a long-duration protection.
 *
 * Each setting of a SharedPointer executes a single atomic compare and swap operation,
 * and may rarely recruit the setting thread to assist in collecting freed
 * memory. The pause caused by this assist is typically very small.
 */
template<class T>
class SharedPointer
{
private:
    template<class V>
    friend class PrivatePointer;

    template<class V>
    friend class SharedPointer;

    template<class V>
    friend class AtomicPointer;

private:
    atm<T*> target; //the stored pointer

public:

    SharedPointer() noexcept
    {
        target.store(nullptr, orls);
    }

    SharedPointer(SharedPointer&& that) noexcept
    {
        target.store(that.get(), orls);
        that.target.store(nullptr, orls);
    }

    SharedPointer(SharedPointer const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    SharedPointer(SharedPointer<V> const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    SharedPointer(SharedPointer<V>& that) noexcept :
        SharedPointer((SharedPointer<V> const&)that)
    {
        ;
    }

    template<class V>
    SharedPointer(AtomicPointer<V> const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    SharedPointer(AtomicPointer<V>& that) noexcept :
        SharedPointer((AtomicPointer<V> const&)that)
    {
        ;
    }

    template<class V>
    SharedPointer(PrivatePointer<V> const& that) noexcept
    {
        target.store(that.setCountedPointer(), orls);
    }

    template<class V>
    SharedPointer(PrivatePointer<V>& that) noexcept :
        SharedPointer((PrivatePointer<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit SharedPointer(Args&& ... args)
    {
        target.store(detail::makeNewObject<T>(1, std::forward<Args>(args) ...), orls);
    }

    template<class V, class ... Args>
    explicit SharedPointer(Args&& ... args)
    {
        target.store(detail::makeNewObject<V>(1, std::forward<Args>(args) ...), orls);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args) ...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        set(detail::makeNewObject<V>(1, std::forward<Args>(args) ...));
    }

    void makeArray(sz length, bool initialize = true)
    {
        set(detail::makeNewArray<T>(1, length, initialize));
    }

    ~SharedPointer() noexcept
    {
        detail::registerDecrement(get());
    }

public:

    template<class V>
    friend void swap(SharedPointer& a, SharedPointer<V>& b) noexcept
    {
        T* tmp = a.target.load(orlx);
        a.target.store((T*)b.get(orlx), orlx);
        b.target.store(tmp, orls);
    }

  template<class V>
  void swap(SharedPointer& a, SharedPointer<V>& b) noexcept
  {
    T* tmp = a.target.load(orlx);
    a.target.store((T*)b.get(orlx), orlx);
    b.target.store(tmp, orls);
  }

    template<class V>
    void swap(V& that) noexcept
    {
        swap(*this, that);
    }

    void reset() noexcept
    {
        *this = nullptr;
    }

    template< class Y >
    void reset(Y* ptr) noexcept
    {
        *this = ptr;
    }

    size_t use_count() const noexcept
    {
        T* ptr = get();
        if(!ptr)
            return 0;
        return detail::getObjectHeader(ptr)->getCount();
    }

    bool unique() const noexcept
    {
        return use_count() == 1;
    }

    explicit operator bool() const noexcept
    {
        return get() != nullptr;
    }

public:

    T& operator*() const noexcept
    {
        return *get();
    }

    T* operator->() const noexcept
    {
        return get();
    }

    T& operator[](sz index) const noexcept
    {
        assert(index < length());
        return get()[index];
    }

    T* get(std::memory_order mo = ocon) const noexcept
    {
        return target.load(mo);
    }

    sz length() const noexcept
    {
        return detail::getObjectHeader(get())->length();
    }

public:

    bool operator==(std::nullptr_t)const noexcept
    {
        return get() == nullptr;
    }

    template<class V>
    bool operator==(V* const that)const noexcept
    {
        return get() == that;
    }

    template<class V>
    bool operator==(PrivatePointer<V> const& that) const noexcept
    {
        return that.get() == get();
    }

    template<class V>
    bool operator==(SharedPointer<V> const& that) const noexcept
    {
        return that.get() == get();
    }

    template<class V>
    bool operator==(AtomicPointer<V> const& that) const noexcept
    {
        return that.get() == get();
    }

    template<class V>
    bool operator!=(V const& that) const noexcept
    {
        return !(*this == that);
    }

public:

    SharedPointer& operator=(std::nullptr_t const&)noexcept
    {
        return set(nullptr);
    }

    template<class V>
    SharedPointer& operator=(PrivatePointer<V> const& that)
    {
        return set(that.setCountedPointer());
    }

    SharedPointer& operator=(SharedPointer const& that) noexcept
    {
        T* ptr = that.get(orlx);
        detail::registerIncrement(ptr);
        return set(ptr);
    }

    template<class V>
    SharedPointer& operator=(SharedPointer<V>const& that) noexcept
    {
        T* ptr = that.get(orlx);
        detail::registerIncrement(ptr);
        return set(ptr);
    }

    SharedPointer& operator=(SharedPointer&& that) noexcept
    {
        swap(that);
        return *this;
    }

    template<class V>
    SharedPointer& operator=(SharedPointer<V>&& that) noexcept
    {
        swap(that);
        return *this;
    }

    template<class V>
    SharedPointer& operator=(AtomicPointer<V>const& v) noexcept
    {
        PrivatePointer<V> protect(v);
        return *this = protect;
    }

private:

    SharedPointer& set(T* newValue) noexcept
    {
        T* old = target.load(orlx);
        target.store(newValue, orlx);
        detail::registerDecrement(old);
        writeFence();
        return *this;
    }
};


} /* namespace frc */
} /* namespace terrain */


namespace std
{

/**
 * std lib specialization of std::hash for SharedPointer's
 */
template <class T>
struct hash<terrain::frc::SharedPointer<T>>
{

    std::size_t operator()(const terrain::frc::SharedPointer<T>& k) const
    {
        return hash<T*>()(k.get());
    }
};
}

#include "PrivatePointer.h"
