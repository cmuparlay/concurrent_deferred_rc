/*
 * File: PrivatePointer.h
 * Copyright 2016-2018 Terrain Data, Inc.
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

#include "detail/FRCConstants.h"
#include "AtomicPointer.h"
#include "SharedPointer.h"

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
 * Skinny and fast protected set pointer. Good for most applications.
 *
 * An object of this class will prevent the object/memory pointed to from being
 * collected.
 *
 * The main use for protected set pointers is as stack-resident pointers
 * when traversing data structures. This prevents the objects being
 * read from begin reclaimed out from under the reader.
 *
 * There is a limit (currently 1024) to the number of pins allowed at one
 * time for a particular thread. For performance reasons,
 * there is no safeguard in place to prevent over-allocations of pins beyond
 * this limit. Typical applications will only pin a handful of things.
 *
 * T is the type of the object being pointed to.
 */
template<class T>
class PrivatePointer
{
private:
    template<class V>
    friend
    class PrivatePointer;

    template<class V>
    friend
    class SharedPointer;

    template<class V>
    friend
    class AtomicPointer;

private:

    /**
     * A pointer to the pin pointer.
     * The pin pointer holds the actual pointer, so that **pin is a reference
     * to the pointed-to-value.
     *
     * The pins reside in a thread-local array as defined in /detail/PinSet.h
     */
    atm<void*>* pin;

public:

    PrivatePointer() noexcept :
        pin(detail::PinSet::acquire())
    {
        pin->store(nullptr, orls);
    }

    PrivatePointer(PrivatePointer&& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        std::swap(*this, that);
    }

    PrivatePointer(PrivatePointer const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        copyFrom(that);
    }

    template<class V>
    PrivatePointer(PrivatePointer<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointer(PrivatePointer<V>& that) noexcept :
        PrivatePointer((PrivatePointer<V> const&) that)
    {
        ;
    }

    template<class V>
    PrivatePointer(SharedPointer<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointer(SharedPointer<V>& that) noexcept :
        PrivatePointer((SharedPointer<V> const&) that)
    {
        ;
    }

    template<class V>
    PrivatePointer(AtomicPointer<V> const& that) noexcept :
        pin(detail::PinSet::acquire())
    {
        init(that);
    }

    template<class V>
    PrivatePointer(AtomicPointer<V>& that) noexcept :
        PrivatePointer((AtomicPointer<V> const&) that)
    {
        ;
    }

    template<class ... Args>
    explicit PrivatePointer(Args&& ... args) :
        pin(detail::PinSet::acquire())
    {
        make<T>(std::forward<Args>(args) ...);
    }

    template<class V, class ... Args>
    explicit PrivatePointer(Args&& ... args) :
        pin(detail::PinSet::acquire())
    {
        make<V>(std::forward<Args>(args) ...);
    }

    template<class ... Args>
    void make(Args&& ... args)
    {
        makeType<T>(std::forward<Args>(args) ...);
    }

    template<class V, class ... Args>
    void makeType(Args&& ... args)
    {
        doEmplace(detail::makeNewObject<V>(1, std::forward<Args>(args) ...));
    }

    void makeArray(sz length, bool initialize = true)
    {
        doEmplace(detail::makeNewArray<T>(1, length, initialize));
    }


public:

    ~PrivatePointer() noexcept
    {
        while(detail::ThreadData::isScanning())
            ;
        detail::PinSet::release(pin);
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

    T* get() const noexcept
    {
        return reinterpret_cast<T*>(pin->load(orlx));
    }

    sz length() const noexcept
    {
        return detail::getObjectHeader(get())->length();
    }

public:

    PrivatePointer& operator=(std::nullptr_t const&)noexcept
    {
        return set((T*) nullptr);
    }

    template<class V>
    PrivatePointer& operator=(PrivatePointer<V> const& that) noexcept
    {
        return set(that);
    }

    PrivatePointer& operator=(PrivatePointer const& that) noexcept
    {
        return copyFrom(that);
    }

    PrivatePointer& operator=(PrivatePointer&& that) noexcept
    {
        swap(that);
        return *this;
    }

    template<class V>
    PrivatePointer& operator=(PrivatePointer<V>&& that) noexcept
    {
        swap(that);
        return *this;
    }

    template<class V>
    PrivatePointer& operator=(SharedPointer<V> const& that) noexcept
    {
        return set(that);
    }

    template<class V>
    PrivatePointer& operator=(AtomicPointer<V> const& that) noexcept
    {
        return set(that);
    }

public:

    bool operator==(std::nullptr_t) const noexcept
    {
        return get() == nullptr;
    }

    template<class V>
    bool operator==(V* const that) const noexcept
    {
        return get() == that;
    }

    template<class V>
    bool operator==(PrivatePointer<V> const& that) const noexcept
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

    friend void swap(PrivatePointer& a, PrivatePointer& b) noexcept
    {
        a.swap(b);
    }

    void swap(PrivatePointer& that) noexcept
    {
        std::swap(pin, that.pin);
    }

    void reset() noexcept
    {
        *this = nullptr;
    }

    size_t use_count() const noexcept
    {
        T* ptr = get();
        if(!ptr)
            return 0;
        return detail::getObjectHeader(ptr)->getCount();
    }

    explicit operator bool() const noexcept
    {
        return get() != nullptr;
    }

private:

    PrivatePointer& set(std::nullptr_t) noexcept
    {
        pin->store(nullptr, orlx);
        return *this;
    }

    //    template<class V>
    //    PrivatePointer &set(V *ptr) noexcept
    //    {
    //        pin->store(ptr, orls);
    //        return *this;
    //    }

    template<class V>
    PrivatePointer& copyFrom(PrivatePointer<V> const& that) noexcept
    {
        auto ptr = that.get();
        detail::registerIncrement(ptr);
        pin->store(ptr, orls);
        detail::registerDecrement(ptr);
        return *this;
    }

    template<class V>
    PrivatePointer& init(PrivatePointer<V> const& that) noexcept
    {
        pin->store(that.get(), orls);
        return *this;
    }

    template<class V>
    PrivatePointer& set(PrivatePointer<V> const& that) noexcept
    {
        //detail::ThreadData::waitForScan();
        return init(that);
    }


    template<class V>
    PrivatePointer& init(V const& that) noexcept
    {
        /* We block the scan phase here with a specialized spin lock:
         * The busySignal is a special value which causes the scaning thread to wait until it has been cleared
         * when reading the thread's pins.
         *
         * This prevents a race condition where the source object is destructed after being read but before
         * being added to the pin set.
         */

        pin->store((void*) detail::FRCConstants::busySignal, orls);
        auto ptr = that.get(oacq);
        pin->store(ptr, orls);
        return *this;
    }

    template<class V>
    PrivatePointer& set(V const& that) noexcept
    {
        //detail::ThreadData::waitForScan();
        return init(that);
    }

    /**
     * Strategies with the first increment:
     * + Create object with count = 1
     *  - Must register a decrement or put in ZCT to check if set to a shared ptr
     * + Create object with count = 0 **using this one**
     *  - Must register an increment when set to a shared ptr
     * + Create object with count = 1 and decrement if not set
     *  + minimizes work done in the common case
     *  - must keep additional state information about private vs shared status
     */
    void doEmplace(T* ptr) noexcept
    {
        pin->store(ptr, orls);
        detail::registerDecrement(ptr);
    }

    T* setCountedPointer() const noexcept
    {
        T* ptr = get();
        detail::registerIncrement(ptr);
        return ptr;
    }
};

} /* namespace frc */
} /* namespace terrain */

namespace std
{

/**
 * std lib specialization of std::hash for SharedPointer's
 */
template<class T>
struct hash<terrain::frc::PrivatePointer<T>>
{

    std::size_t operator()(const terrain::frc::PrivatePointer<T>& k) const
    {
        return hash<T*>()(k.get());
    }
};
}
