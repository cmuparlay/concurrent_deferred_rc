/*
 * File: AtomicPointer.h
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
 * Reference counted pointer.
 *
 * An object of this class will prevent the object/memory pointed to from being
 * collected. Use this more or less as you would a standard atomic smart pointer like
 * boost::atomic_shared_ptr.
 *
 * Consider using SharedPointer instead if you know only one thread will mutate the pointer
 * at a time. SharedPointer allows concurrent reads with a single writer with lower
 * performance costs than AtomicPointer.
 *
 * Consider using PrivatePointer instead if you are just traversing a data structure or
 * otherwise require essentially a short-duration pinning of memory, rather than
 * a long-duration protection.
 *
 * Each setting of a AtomicPointer executes a single atomic compare and swap operation,
 * and may rarely recruit the setting thread to assist in collecting freed
 * memory. The pause caused by this assist is typically very small.
 */
template<class T>
class AtomicPointer
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

    AtomicPointer() noexcept
    {
        target.store(nullptr, orls);
    }

    AtomicPointer(AtomicPointer&& that) noexcept
    {
        target.store(that.get(oacq), orlx);
        that.target.store(nullptr, orls);
    }

    AtomicPointer(AtomicPointer const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    AtomicPointer(AtomicPointer<V> const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    AtomicPointer(AtomicPointer<V>& that) noexcept :
        AtomicPointer((AtomicPointer<V> const&)that)
    {
        ;
    }

    template<class V>
    AtomicPointer(SharedPointer<V>& that) noexcept
    {
        PrivatePointer<T> protect(that);
        target.store(protect.setCountedPointer(), orls);
    }

    template<class V>
    AtomicPointer(PrivatePointer<V> const& that) noexcept
    {
        target.store(that.setCountedPointer(), orls);
    }

    template<class V>
    AtomicPointer(PrivatePointer<V>& that) noexcept :
        AtomicPointer((PrivatePointer<V> const&)that)
    {
        ;
    }

    template<class ... Args>
    explicit AtomicPointer(Args&& ... args)
    {
        target.store(detail::makeNewObject<T>(1, std::forward<Args>(args) ...), orls);
    }

    template<class V, class ... Args>
    explicit AtomicPointer(Args&& ... args)
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

    void makeArray(sz length)
    {
        set(detail::makeNewArray<T>(1, length));
    }

    ~AtomicPointer() noexcept
    {
        detail::registerDecrement(get());
    }

public:

    template<class V>
    friend void swap(AtomicPointer& a, AtomicPointer<V>& b) noexcept
    {
        PrivatePointer<T> protect(a);
        a = b;
        b = protect;
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

    SharedPointer<T> load() {
      return SharedPointer<T>(*this);
    }

    PrivatePointer<T> get_snapshot() {
      return PrivatePointer<T>(*this);
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

    AtomicPointer& operator=(std::nullptr_t const& that) noexcept
    {
        return set(nullptr);
    }

    template<class V>
    AtomicPointer& operator=(PrivatePointer<V> const& that)
    {
        return set(that.setCountedPointer());
    }

    template<class V>
    AtomicPointer& operator=(SharedPointer<V> const& that)
    {
        PrivatePointer<V> protect(that);
        return *this = protect;
    }

    void store(std::nullptr_t)
    {
      set(nullptr);
    }

    template<class V>
    void store(PrivatePointer<V> const& that)
    {
      set(that.setCountedPointer());
    }

    template<class V>
    void store(SharedPointer<V> const& that)
    {
      PrivatePointer<V> protect(that);
      *this = protect;
    }

    AtomicPointer& operator=(AtomicPointer const& that) noexcept
    {
        PrivatePointer<T> protect(that);
        return *this = protect;
    }

    template<class V>
    AtomicPointer& operator=(AtomicPointer<V> const& that) noexcept
    {
        PrivatePointer<V> protect(that);
        return *this = protect;
    }

    AtomicPointer& operator=(AtomicPointer&& that) const noexcept
    {
        auto ptr = that.get(oacq);
        that.target.store(nullptr, orlx);
        this->set(ptr);
        return *this;
    }

    template<class V>
    AtomicPointer& operator=(AtomicPointer<V>&& that) const noexcept
    {
        auto ptr = that.get(oacq);
        that.target.store(nullptr, orlx);
        this->set(ptr);
        return *this;
    }

    bool compare_exchange_weak(PrivatePointer<T>& expected, const SharedPointer<T>& desired) {
      PrivatePointer<T> protect(desired);
      T* expected_ptr = expected.get();
      T* desired_ptr = protect.get();
      if (target.compare_exchange_strong(expected_ptr, desired_ptr)) {
        if (desired_ptr) detail::registerIncrement(desired_ptr);
        if (expected_ptr)detail::registerDecrement(expected_ptr);
        return true;
      }
      else {
        expected = *this;
        return false;
      }
    }

    bool compare_exchange_weak(SharedPointer<T>& expected, const SharedPointer<T>& desired) {
      PrivatePointer<T> protect(desired);
      T* expected_ptr = expected.get();
      T* desired_ptr = protect.get();
      if (target.compare_exchange_strong(expected_ptr, desired_ptr)) {
        if (desired_ptr) detail::registerIncrement(desired_ptr);
        if (expected_ptr)detail::registerDecrement(expected_ptr);
        return true;
      }
      else {
        expected = *this;
        return false;
      }
    }

private:

    AtomicPointer& set(T* newValue) noexcept
    {
        T* old = target.exchange(newValue, oarl);
        detail::registerDecrement(old);
        return *this;
    }

};


} /* namespace frc */
} /* namespace terrain */


namespace std
{

/**
 * std lib specialization of std::hash for AtomicPointer's
 */
template <class T>
struct hash<terrain::frc::AtomicPointer<T>>
{

    std::size_t operator()(const terrain::frc::AtomicPointer<T>& k) const
    {
        return hash<T*>()(k.get());
    }
};
}

#include "PrivatePointer.h"
