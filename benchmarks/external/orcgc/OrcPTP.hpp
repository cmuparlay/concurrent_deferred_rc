/*
 * Copyright 2016-2020
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once

#include <atomic>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include "ThreadRegistry.hpp"


/*
 * <h1> ORC GC - Pass-The-Pointer </h1>
 *
 * TODO: explain orcgc
 *
 * This header contains 4 important classes:
 * 1. PassThePointerORC: The core of the manual reclamation scheme
 * 2. orc_ptr: A wrapper like std::shared_ptr to keep track of the pointer's lifetime
 * 3. orc_base: A base class where the _orc counter is stored
 * 4. orc_atomic: A replacement to std::atomic
 *
 * The idea of using ORCs is not new, it has been used in CX and described as early as
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.149.7259&rep=rep1&type=pdf
 *
 * Advantages of ORC GC over manual lock-free memory reclamation like Hazard Pointers or Hazard Eras:
 * 1. No need for the user to determine the maximum number of hazardous
 *    pointers in the algorithm. This is determined automatically by ORC GC
 *    though it may not be the minimum;
 * 2. No need to pass thread-id, though each thread must register itself with
 *    the methods thread_register()/thread_unregister();
 * 3. No need to call retire() or to determine when is it safe to call it.
 *    Unlinking of nodes that are no longer accessible is however required (for now);
 *
 * TODO:
 * - Add the find . trick to the makefile dependencies for graphs
 * - Add GAS wait-free stack https://arxiv.org/pdf/1510.00116.pdf
 * - Add HTM casincdec() function and measure 100% in set-ll-1k and in queues bench. Don't call if expected or desired are nullptr
 */

namespace orcgc_ptp {


// TODO define this stuff in inner methods of orc_base or hp_obj/he_obj
#ifndef ORC_CONSTANTS
#define ORC_CONSTANTS
static const uint64_t ORC_SEQ  = (1ULL << 24);
static const uint64_t BRETIRED = (1ULL << 23);
static const uint64_t ORC_ZERO = (1ULL << 22);
static const uint64_t ORC_CNT_MASK = ORC_SEQ-1;
static const uint64_t ORC_SEQ_MASK = ~(ORC_SEQ-1);
static const int      MAX_RETCNT = 1000;
// TODO make these inline functions to not polute the namespace
#define oseq(x) (ORC_SEQ_MASK & (x))
#define ocnt(x) (ORC_CNT_MASK & (x))
#define hasBitRetire(x)   ((x) & BRETIRED)
#define isCounterZero(x)  (((x) & (~BRETIRED) & ORC_CNT_MASK) == ORC_ZERO)
#endif


/*
 * Base type which all tracked objects must extend
 */
struct orc_base {
    std::atomic<uint64_t>   _orc {ORC_ZERO};        // Counts the number of object references (hard links)
    void                  (*_deleter)(void*){nullptr};
};



// Hazard Pointers class made specifically to be used by OrcGC
class PassThePointerOrcGC  {

private:
    static const int      MAX_HAZ = 64;        // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(uintptr_t);
    bool                  inDestructor = false;
    // Stuff that is thread specific and is therefore indexed by thread id in tl[]
    struct TLInfo {
        bool                    retireStarted {false};
        std::vector<orc_base*>  recursiveList;
        int                     usedHaz[MAX_HAZ];  // Which hp indexes are being used by the thread.
        int                     retcnt {0};
        uint8_t                 pad[128];
        TLInfo() {
            for (int ihe = 0; ihe < MAX_HAZ; ihe++) usedHaz[ihe] = 0;
            recursiveList.reserve(REGISTRY_MAX_THREADS*MAX_HAZ);
        }
    };

    // Class members
    alignas(128) std::atomic<orc_base*>   hp[REGISTRY_MAX_THREADS][MAX_HAZ];
    alignas(128) std::atomic<orc_base*>   handovers[REGISTRY_MAX_THREADS][MAX_HAZ];
    alignas(128) std::atomic<uint64_t>    maxHPs {0};                 // Optimization so that retire() doesn't have to scan MAX_HAZ everytime
    alignas(128) TLInfo                   tl[REGISTRY_MAX_THREADS];   // Thread-local stuff. One entry per thread

public:
    PassThePointerOrcGC() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            for (int ihp = 0; ihp < MAX_HAZ; ihp++) {
                hp[it][ihp].store(nullptr, std::memory_order_relaxed);
                handovers[it][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    // Delete the objects from handover list.
    // Unlike in HP, there is no need ofr a loop here because no further objects will be placed in handovers[] from calling _deleter()
    ~PassThePointerOrcGC() {
        inDestructor = true;
        // Now delete whatever is on the handovers array, triggering further deletions as needed
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        const int tid = ThreadRegistry::getTID();
        const int lmaxHPs = maxHPs.load(std::memory_order_acquire);
        for (int it = 0; it < maxThreads; it++) {
            for (int ihp = 0; ihp < lmaxHPs; ihp++) {
                orc_base* obj = handovers[it][ihp].load();
                if (obj == nullptr) continue;
                uint64_t lorc = obj->_orc.load();
                handovers[it][ihp].store(nullptr, std::memory_order_relaxed);
                retire(obj,tid);

            }
        }
        //printf("maxHPs = %ld\n", maxHPs.load());
    }

    inline int addRetcnt(int tid) {
        return ++tl[tid].retcnt;
    }

    void resetRetcnt(int tid) {
        tl[tid].retcnt = 0;
    }

    // Returns the next available he index for this thread, and updates maxHEs if needed
    // TODO: consider optimizing by looking up if the ptr already exists and if yes, return the index (and increment the usedHaz)
    int getNewIdx(const int tid, int start_idx=1) {
        for (int idx = start_idx; idx < MAX_HAZ; idx++) {
            if (tl[tid].usedHaz[idx] != 0) continue;
            tl[tid].usedHaz[idx]++;
            // Increase the current maximum to cover the new hp index
            uint64_t curMax = maxHPs.load(std::memory_order_relaxed);
            while (curMax <= idx) {
                maxHPs.compare_exchange_strong(curMax, idx+1);
            }
            return idx;
        }
        std::cout << "ERROR: MAX_HAZ is not enough for all the hazardous pointers in this algorithm\n";
        assert(false);
    }

    /**
     * Marks the index as being used by another orc_ptr
     */
    inline void usingIdx(const int idx, const int tid) {
        if (idx == 0) return;
        tl[tid].usedHaz[idx]++;
    }

    inline int cleanIdx(const int idx, const int tid) {
    	if (idx == 0) return -1;
        return --tl[tid].usedHaz[idx];
    }

    /**
     * Progress Condition: ???
     * 'T' is typically 'Node*'
     * This is on the hot-path because it is called from orc_atomic<T>::load()
     */
    template<typename T> inline void clear(T ptr, const int idx, const int tid, const bool linked, const bool reuse) {
        if (!reuse && cleanIdx(idx, tid) != 0) return;
        // Some algorithms (like the Kogan-Petrank queue for OpDescs) may not always
        // insert a newly created object into the data structure.
        // This if() takes care of those algorithms.
        if (linked) return;
        if (ptr != nullptr) {
            ptr = getUnmarked(ptr);
            uint64_t lorc = ptr->_orc.load(std::memory_order_acquire);
            if (ocnt(lorc) == ORC_ZERO) {
                if (ptr->_orc.compare_exchange_strong(lorc, lorc+BRETIRED)) retire(ptr, tid);
            }
        }
    }

    inline int getUsedHaz(const int idx, const int tid) {
        return tl[tid].usedHaz[idx];
    }

    // Progress Condition: lock-free
    template<typename T> inline T get_protected(int index, std::atomic<T>* addr, const int tid) {
        T pub, ptr = nullptr;
        while ((pub=addr->load()) != ptr) {
#ifdef ALWAYS_USE_EXCHANGE
            hp[tid][index].exchange(getUnmarked(pub));
#else
            hp[tid][index].store(getUnmarked(pub));
#endif
            ptr = pub;
        }
        return pub;
    }

    // Protect an existing pointer. Only called when the ptr comes from an already published pointer in a lower index.
    // Notice that the store here is done with memory_order_release, while on get_protected() it is done with memory_order_seq_cst or equivalent.
    // Progress Condition: wait-free population-oblivious
    inline void protect_ptr(orc_base* ptr, const int tid, int index) {
        hp[tid][index].store(getUnmarked(ptr), std::memory_order_release);
    }

    /**
     * Retire an object (node)
     * Progress Condition: wait-free bounded
     */
    void retire(orc_base* ptr) {
        const int tid = ThreadRegistry::getTID();
        retire(ptr, tid);
    }

    void retire(orc_base* ptr, int tid) {
        //std::cout << "retiring ptr = " << ptr << std::endl;
        if (ptr == nullptr) return;
        auto& rlist = tl[tid].recursiveList;
        // We don't want to blow up the program's stack, therefore, if this is being called recursively,
        // just add the ptr to the recursiveList and return.
        if (tl[tid].retireStarted) {
            rlist.push_back(ptr);
            return;
        }
        // If this is being called from the destructor ~PassThePointerOrcGC(), clear out the handovers so we don't leak anything
        if (!inDestructor) {
            const int lmaxHPs = maxHPs.load(std::memory_order_acquire);
            for (int i=0;i<lmaxHPs;i++){
                // there is at least one hp with ptr published
                if (hp[tid][i].load(std::memory_order_relaxed) == ptr) {
                    ptr = handovers[tid][i].exchange(ptr);
                    break;
                }
            }
        }
        int i=0;
        tl[tid].retireStarted = true;
      //std::cout << "starting retire = " << ptr << std::endl;
        while (true) {
            while (ptr != nullptr){
                auto lorc = ptr->_orc.load();
                if (!isCounterZero(lorc)){
                	if((lorc = clearBitRetired(ptr,tid))==0) break;
                }
                if (tryHandover(ptr)) continue;
                uint64_t lorc2 = ptr->_orc.load(std::memory_order_acquire);
                if (lorc2 != lorc) {
                    if(!isCounterZero(lorc2)){
                    	if(clearBitRetired(ptr,tid)==0) break;
                    }
                    continue;
                }
                //std::cout << "DELETING??? " << ptr << std::endl;
                (*(ptr->_deleter))(ptr);  // This calls "delete obj" with the appropriate type information
                break;
            }
            if(rlist.size()==i) break;
            ptr = rlist[i];
            i++;
        }
        assert(i== rlist.size());
        rlist.clear();
        tl[tid].retireStarted = false;
    }

    uint64_t clearBitRetired(orc_base* ptr, int tid) {
    	hp[tid][0].store(static_cast<orc_base*>(ptr), std::memory_order_release);
    	uint64_t lorc = ptr->_orc.fetch_add(-BRETIRED)-BRETIRED;
		if(ocnt(lorc) == ORC_ZERO && ptr->_orc.compare_exchange_strong(lorc, lorc+BRETIRED)){
			hp[tid][0].store(nullptr, std::memory_order_relaxed);
			return lorc+BRETIRED;// counter is zero, we can proceed to check HPs
		}else{
			hp[tid][0].store(nullptr, std::memory_order_relaxed);
			return 0;
		}
    }


    // Search for _one_ object to retire
    // Called only from decrementOrc(). Must be 'public'.
    void retireOne(int tid) {
        const int lmaxHPs = maxHPs.load(std::memory_order_acquire);
        for (int idx = 0; idx < lmaxHPs; idx++) {
            // Find an obj to delete in my handovers list
            orc_base* obj = handovers[tid][idx].load(std::memory_order_relaxed);
            if (obj != nullptr && obj != hp[tid][idx].load(std::memory_order_relaxed)){
                obj = handovers[tid][idx].exchange(nullptr);
                retire(obj,tid);
                return;
            }
        }
        // No object in my own handover, therefore, scan every other thread's handover
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        for (int id = 0; id < maxThreads; id++) {
            if (id == tid) continue; // Already scanned my own list
            for (int idx = 0; idx < lmaxHPs; idx++) {
                orc_base* obj = handovers[id][idx].load(std::memory_order_acquire);
                if (obj != nullptr && obj != hp[id][idx].load(std::memory_order_acquire)) {
                    obj = handovers[id][idx].exchange(nullptr);
                    retire(obj,tid);
                    return;
                }
            }
        }
    }


private:

    // Called only from retire()
    inline bool tryHandover(orc_base*& ptr) {
        if (inDestructor) return false;
        const int maxThreads = (int)ThreadRegistry::getMaxThreads();
        const int lmaxHPs = maxHPs.load(std::memory_order_acquire);
        for (int tid = 0; tid < maxThreads; tid++) {
            for (int idx = 0; idx < lmaxHPs; idx++) {
                if (ptr == hp[tid][idx].load(std::memory_order_acquire)) {
                    ptr = handovers[tid][idx].exchange(ptr);
                    return true;
                }
            }
        }
        return false;
    }

public:
    // Needed by Harris Linked List (orc_ptr)
    template<typename T> T getUnmarked(T ptr) { return (T)(((size_t)ptr) & (~0x3ULL)); }
};

// The global/singleton instance of PassThePointer
PassThePointerOrcGC g_ptp {};



// Temporary object that comes from a load().
// Do NOT use in user code. This is meant to be used internally only.
template<typename T>
struct orc_unsafe_internal_ptr {
    T ptr;

    orc_unsafe_internal_ptr(T ptr) : ptr{ptr} {}

    // Used by Natarajan and maybe Harris
    inline T getUnmarked() const { return (T)(((size_t)ptr) & (~3ULL)); }
    inline T getMarked() const { return (T)(((size_t)ptr) | (3ULL)); }
    inline bool isMarked() const { return getFlag(); }
    inline bool getFlag() const { return (bool)((size_t)ptr & 1); }
    inline bool getTag() const { return (bool)((size_t)ptr & 2); }

    // Equality/Inequality operators
    bool operator == (const orc_unsafe_internal_ptr<T> &rhs) { return ptr == rhs.ptr; }
    bool operator == (const T &rhs) { return ptr == rhs; }
    bool operator != (const orc_unsafe_internal_ptr<T> &rhs) { return ptr != rhs.ptr; }
    bool operator != (const T &rhs) { return ptr != rhs; }
};



// A load() or exchange() on an atomic_orc<T> will return an orc_ptr<T>
// orc_ptr<T*> is an RAII, with semantics similar to std::shared_ptr<T>:
//   https://en.cppreference.com/w/cpp/memory/shared_ptr
// Notice the missing star on shared_ptr, where it's assumed to be a pointer type.
//
// Do not pass an orc_ptr from one thread to another. Similarly to shared_ptr, this is meant for a single thread's use.
// 'T' is typically 'Node*'.
// There are four cases that we need to deal with, where 'next' is an orc_ptr and load() returns an orc_unsafe_internal_ptr<Node*>:
// Scenario 1:
//   orc_ptr<Node*> curr {nullptr};
//   curr = next;
// Calls: assignemnt operator (orc-to-orc)
//
// Scenario 2:
//   orc_ptr<Node*> curr {nullptr};
//   curr = atom->load();
// Calls: copy constructor (internal-to-internal), move assignment (internal-to-orc)
//
// Scenario 3:
//   orc_ptr<Node*> curr = next;
// Calls: copy constructor (orc-to-orc)
//
// Scenario 4
//   orc_ptr<Node*> curr = atom->load();
// Calls: copy constructor (internal-to-internal), copy constructor (internal-to-orc)
//

template<typename T> struct orc_ptr {
    T       ptr;
    int16_t tid;
    int8_t  idx;
    int8_t  lnk;  // This represents 'linked' or not and is set to false on make_orc<T>, true when coming from orc_atomic<T>::load()


    orc_ptr(T ptr, const int16_t tid, const int8_t idx, const int8_t linked) : ptr{ptr}, tid{tid}, idx{idx}, lnk{linked} {}

    // Default constructor. ptr will be nullptr
    orc_ptr() : lnk{true} {
        tid = ThreadRegistry::getTID();
        idx = g_ptp.getNewIdx(tid);
        ptr = nullptr;
    }

    ~orc_ptr() {
        g_ptp.clear(ptr, idx, tid, lnk, false);
    }

    // Used by Natarajan and maybe Harris
    inline T getUnmarked() const { return (T)(((size_t)ptr) & (~3ULL)); }
    inline T getMarked() const { return (T)(((size_t)ptr) | (3ULL)); }
    inline bool isMarked() const { return getFlag(); }
    inline bool getFlag() const { return (bool)((size_t)ptr & 1); }
    inline bool getTag() const { return (bool)((size_t)ptr & 2); }
    inline void unmark() { ptr = getUnmarked(); }
    inline void swapPtrs(orc_ptr<T>& other) {
        T tmp_ptr = ptr;
        int8_t tmp_idx = idx;
        ptr = other.ptr;
        idx = other.idx;
        other.ptr = tmp_ptr;
        other.idx = tmp_idx;
    }

    // Equality/Inequality operators
    bool operator == (const orc_unsafe_internal_ptr<T> &rhs) { return ptr == rhs.ptr; }
    bool operator != (const orc_unsafe_internal_ptr<T> &rhs) { return ptr != rhs.ptr; }
    bool operator == (const T &rhs) { return ptr == rhs; }
    bool operator != (const T &rhs) { return ptr != rhs; }

    // Operator ->  de-references using the unmarked pointer
    T operator->() const noexcept { return getUnmarked(); }

    // Operator *   de-references using the unmarked pointer
    T& operator*() const noexcept { return getUnmarked(); }

    // Casting: yields the raw pointer, with markings
    operator T() { return ptr; }

    // Similar to std::shared_ptr::get(): https://en.cppreference.com/w/cpp/memory/shared_ptr/get
    // Return the unsafe/raw pointer. Use this at your own peril (DO NOT USE)
    //T get() const noexcept { return ptr; }

    // Copy constructor (orc-to-orc)
    orc_ptr(const orc_ptr& other) {
        PassThePointerOrcGC* ptp = &g_ptp;
        tid = other.tid;
        idx = other.idx;
        ptr = other.ptr;
        lnk = other.lnk;
        if (idx == 0) {
            idx = ptp->getNewIdx(tid);
            ptp->protect_ptr(ptr, tid, idx);
        } else {
            ptp->usingIdx(idx, tid);
        }
    }

    // Copy constructor (internal-to-orc)
    orc_ptr(const orc_unsafe_internal_ptr<T>& other) {
        tid = ThreadRegistry::getTID();
        idx = g_ptp.getNewIdx(tid);
        ptr = other.ptr;
        lnk = true;
        g_ptp.protect_ptr(ptr, tid, idx);
    }

    // Copy constructor with move semantics (orc-to-orc)
    orc_ptr(orc_ptr&& other) {
        //printf("orc_ptr constructor with move semantics from %p increment on idx=%d\n", other.ptr, other.idx);
        tid = other.tid;
        idx = other.idx;
        ptr = other.ptr;
        lnk = other.lnk;
        if (idx == 0) {
            idx = g_ptp.getNewIdx(tid);
            g_ptp.protect_ptr(ptr, tid, idx);
        } else {
            // other.idx is always 0, it should never enter this branch
            other.idx = 0;
        }
    }

    // Assignment operator
    // We decrement the counter (and clear the hp if there is no other orc_ptr
    // with the same idx) and increment the counter for the other orc_ptr.
    inline orc_ptr& operator=(const orc_ptr& other) {
        PassThePointerOrcGC* ptp = &g_ptp;
        bool reuseIdx = ((other.idx < idx) && (ptp->getUsedHaz(idx, tid) == 1));
        ptp->clear(ptr, idx, tid, lnk, reuseIdx);
        if (other.idx < idx) {
            if (!reuseIdx) idx = ptp->getNewIdx(tid, other.idx+1);
            ptp->protect_ptr(other.ptr, tid, idx);
        } else {
            ptp->usingIdx(other.idx, tid);
            idx = other.idx;
        }
        ptr = other.ptr;
        lnk = other.lnk;
        return *this;
    }

    // Move assignment operator (orc-to-orc)
    inline orc_ptr& operator=(orc_ptr&& other) {
        PassThePointerOrcGC* ptp = &g_ptp;
        bool reuseIdx = ((other.idx < idx) && (ptp->getUsedHaz(idx, tid) == 1));
        ptp->clear(ptr, idx, tid, lnk, reuseIdx);
        if (other.idx < idx) {
            if (!reuseIdx) idx = ptp->getNewIdx(tid, other.idx+1);
            ptp->protect_ptr(other.ptr, tid, idx);
        } else {
            // Steal the other's reference
            idx = other.idx;
            other.idx = 0;
        }
        ptr = other.ptr;
        lnk = other.lnk;
        return *this;
    }

    // Move assignment (internal-to-orc)
    //other comes always from a load and other.idx is 0
    inline orc_ptr& operator=(orc_unsafe_internal_ptr<T>&& other) {
        // This may be called once or twice. If called twice, 'other' is the just-moved-from orc_ptr hp
        //printf("orc_ptr 'move' from %p to %p increment on idx=%d\n", ptr, other.ptr, other.idx);
        bool reuseIdx = (g_ptp.getUsedHaz(idx, tid) == 1);
        g_ptp.clear(ptr, idx, tid, lnk, reuseIdx);
        if (!reuseIdx) idx = g_ptp.getNewIdx(tid);
        g_ptp.protect_ptr(other.ptr, tid, idx);
        ptr = other.ptr;
        lnk = true;
        return *this;
    }

    // Used by Harris Original and Maged-Harris
    // TODO: change this to return orc_unsafe_internal_ptr<T> instead.
    T setUnmarked(orc_ptr<T>& other) {
        PassThePointerOrcGC* ptp = &g_ptp;
        bool reuseIdx = ((other.idx < idx) && (ptp->getUsedHaz(idx, tid) == 1));
        ptp->clear(ptr, idx, tid, lnk, reuseIdx);
        if(other.idx<idx){
            if (!reuseIdx) idx = ptp->getNewIdx(tid, other.idx+1);
            ptp->protect_ptr(other.ptr, tid, idx);
        }else{
            ptp->usingIdx(other.idx, tid);
            idx = other.idx;
        }
        ptr = g_ptp.getUnmarked(other.ptr);
        lnk = other.lnk;
        return ptr;
    }

    // Used by Harris Original and Maged-Harris
    // TODO: change this to return orc_unsafe_internal_ptr<T> instead.
    T setUnmarked(orc_unsafe_internal_ptr<T>&& other) {
        PassThePointerOrcGC* ptp = &g_ptp;
        bool reuseIdx = (ptp->getUsedHaz(idx, tid) == 1);
        if (!reuseIdx) {
        	ptp->clear(ptr, idx, tid, lnk, reuseIdx);
        	idx = ptp->getNewIdx(tid, 1);
        }
        ptp->protect_ptr(other.ptr, tid, idx);
        ptr = g_ptp.getUnmarked(other.ptr);
        lnk = true;
        return ptr;
    }

};

// Use inheritance to pad class types
template<typename T>
struct alignas(128) Padded : public T {

};

struct allocation_counter {
  static inline std::vector<Padded<std::atomic<int>>> num_allocated{REGISTRY_MAX_THREADS};

  static void decrement_allocations() {
    auto id = ThreadRegistry::getTID();
    num_allocated[id].store(num_allocated[id].load() - 1);
  }

  static void increment_allocations() {
    auto id = ThreadRegistry::getTID();
    num_allocated[id].store(num_allocated[id].load() + 1);
  }

  static size_t currently_allocated() {
    size_t total = 0;
    auto num_threads = ThreadRegistry::getMaxThreads();
    for (size_t t = 0; t < num_threads; t++) {
      total += num_allocated[t].load(std::memory_order_acquire);
    }
    return total;
  }
};


/*
 * make_orc<T> is similar to make_shared<T>
 */
template <typename T, typename... Args>
orc_ptr<T*> make_orc(Args&&... args) {
    const int tid = ThreadRegistry::getTID();
    allocation_counter::increment_allocations();
    T* ptr = new T(std::forward<Args>(args)...);
    ptr->_deleter = [](void* obj) {
      allocation_counter::decrement_allocations();
      delete static_cast<T*>(obj);
    };
    g_ptp.protect_ptr(ptr, tid, 0);
    // If the orc_ptr was created by the user, then it is not linked
    return std::move(orc_ptr<T*>(ptr, tid, 0, false));
}



// Just some variable to make a unique pointer
intptr_t g_poisoned = 0;

template<typename T> bool is_poisoned(T ptr) { return (void*)ptr.getUnmarked() == (void*)&g_poisoned; }



// 'T' is typically 'Node*'
template<typename T>
class orc_atomic : public std::atomic<T> {
private:
    static const bool enablePoison = true;  // set to false to disable poisoning


    // Needed by Harris Linked List, Natarajan tree and possibly others
    static T getUnmarked(T ptr) { return (T)(((size_t)ptr) & (~3ULL)); }

    // Progress condition: wait-free population oblivious
    inline void incrementOrc(T ptr) {
        ptr = getUnmarked(ptr);
        if (ptr == nullptr || ptr == (T)&g_poisoned) return;
        uint64_t lorc = ptr->_orc.fetch_add(1) + 1;
        if (ocnt(lorc) != ORC_ZERO) return;
        // No need to increment sequence: the faa has done it already
        if (ptr->_orc.compare_exchange_strong(lorc, lorc + BRETIRED)) g_ptp.retire(ptr);
    }

    /*
     * There are three places from which this function is called: CAS(), store() and ~orc_atomic()
     * The CAS() and store() execute an inc() on the new object and then dec() on the old one.
     * ~orc_atomic() executes a dec() on the old object.
     * Progress condition: wait-free
     */
    inline void decrementOrc(T ptr) {
      //std::cout << "INTERNAL decrement for ptr = " << ptr << std::endl;
        ptr = getUnmarked(ptr);
        if (ptr == nullptr || ptr == (T)&g_poisoned) return;
        const int tid = ThreadRegistry::getTID();
        g_ptp.protect_ptr(ptr, tid, 0);
        //std::cout << "protected ptr = " << ptr << std::endl;
        uint64_t lorc = ptr->_orc.fetch_add(ORC_SEQ-1) + ORC_SEQ - 1;
      //std::cout << "fetch add done" << ptr << std::endl;
        if (g_ptp.addRetcnt(tid) == MAX_RETCNT) {
            g_ptp.retireOne(tid);
            g_ptp.resetRetcnt(tid);
        }
      //std::cout << "idunno done " << ptr << std::endl;
        if (ocnt(lorc) != ORC_ZERO) return;
        // No need to increment sequence: the faa has done it already
      //std::cout << "some last CAS? " << ptr << std::endl;
        if (ptr->_orc.compare_exchange_strong(lorc, lorc + BRETIRED)) g_ptp.retire(ptr, tid);
      //std::cout << "decrement done for " << ptr << std::endl;
    }

public:
    orc_atomic() {
        std::atomic<T>::store(nullptr, std::memory_order_relaxed);
    }

    orc_atomic(T ptr) {
        incrementOrc(ptr);
        std::atomic<T>::store(ptr, std::memory_order_relaxed);
    }

    ~orc_atomic() {
        T ptr = std::atomic<T>::load(std::memory_order_relaxed);
        if (ptr == nullptr || ptr == (T)&g_poisoned) return;
        // No need to protect before decrementing because this object is being
        // destroyed by the current thread and therefore, there is a positive counter
        // on "ptr._orc"
        decrementOrc(ptr);
    }

    // Operator arrow.  Let's us do 'ptr = oatom_a->oatom_b->otaom_c' instead of 'ptr = oatom_.load()->oatom_b.load()->oatom_c.load'
    orc_ptr<T> operator->() { return load(); }

    // Casting operator. Let's us do 'ptr = oatom' instead of 'ptr = oatom.load()'
    operator orc_ptr<T>() { return load(); }

    // Assignment operator from a desired value. Let's us do 'oatom = ptr' instead of 'oatm.store(ptr)'
    orc_atomic<T>& operator=(T desired) {
        store(desired);
        return *this;
    }

    // Assignment operator from another orc_atomic. Let's us do 'oatom = atom' instead of 'oatm.store(atom.load())'
    orc_atomic<T>& operator=(orc_atomic<T>& atom) {
        // TODO: can we optimize this?
        orc_ptr<T> newval = atom.load();
        store(newval);
        return *this;
    }

    // One of the invariants of OrcGC is that an object whose orc is not yet incremented, to adjust for the fact
    // that it has one extra connection to it, must have at least one hazardous pointer to protect the object.
    // We take advantage of this invariant to ensure that incrementOrc() does not need yet another hp to protect it and
    // that even if the counter drops to zero before our call to decrementOrc(), there will be a hp protecting the object.
    // It's ok to do the increment before the exchange() because the exchange() never fails.
    // Progress: Wait-free (population oblivious)
    inline void store(T newval, std::memory_order order = std::memory_order_seq_cst) {
        incrementOrc(newval);
        T old = std::atomic<T>::exchange(newval, order);
        decrementOrc(old);
    }

    // This is currently not being used by any data structure, but we implemented it anyways
    // Progress: Wait-free (population oblivious)
    inline orc_unsafe_internal_ptr<T> exchange(T newval) {
        incrementOrc(newval);
        T old = std::atomic<T>::exchange(newval);
        decrementOrc(old);
        const int tid = ThreadRegistry::getTID();
        return std::move(orc_unsafe_internal_ptr<T>{old});
    }

    // Warning: unlike std::atomic<T>::cas() the param 'expected' will not be updated
    // Progress: Wait-free (population oblivious)
    inline bool compare_exchange_strong(T expected, T desired) {
        if (!std::atomic<T>::compare_exchange_strong(expected,desired)) return false;
        incrementOrc(desired);
        decrementOrc(expected);
        return true;
    }

    // Warning: unlike std::atomic<T>::cas() the param 'expected' will not be updated
    // Progress: Wait-free (population oblivious)
    inline bool compare_exchange_weak(T expected, T desired) {
      //std::cout << "INTERNAL CAS. expected = " << expected << ", desired = " << desired << std::endl;
        if (!std::atomic<T>::compare_exchange_weak(expected,desired)) return false;
        //std::cout << "INteRNAL CAS succeeded." << std::endl;
        incrementOrc(desired);
        //std::cout << "Increment success" << std::endl;
        decrementOrc(expected);
        //std::cout << "Decrement success" << std::endl;
        return true;
    }

    // Progress: Lock-Free
    inline orc_unsafe_internal_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) {
        const int tid = ThreadRegistry::getTID();
        auto ptr = static_cast<T>(g_ptp.get_protected(0, this, tid));
        // If it's coming from an orc_atomic<T>::load() then it must be linked=true and temp=true
        return std::move(orc_unsafe_internal_ptr<T>{ptr});
    }

    // Same as above, but creates an orc_ptr<T> without the marked bit. Used by Sundel-Tsigas
    inline orc_unsafe_internal_ptr<T> loadUnmarked(std::memory_order order = std::memory_order_seq_cst) {
        const int tid = ThreadRegistry::getTID();
        auto ptr = static_cast<T>(g_ptp.get_protected(0, this, tid));
        // If it's coming from an orc_atomic<T>::load() then it must be linked=true and temp=true
        return std::move(orc_unsafe_internal_ptr<T>{getUnmarked(ptr)});
    }

    // This assumes no other thread will change the value after poisoned
    inline void poison() {
        if (enablePoison) {
            T old = std::atomic<T>::load(std::memory_order_relaxed);
            std::atomic<T>::store(getMarked((T)&g_poisoned), std::memory_order_relaxed);
            decrementOrc(old);
        }
    }
    T getMarked(T ptr) { return (T)(((size_t)ptr) | (3ULL)); }

    static inline bool is_poisoned(T val) { return getUnmarked(val) == (T)&g_poisoned; }

    static inline size_t currently_allocated() {
      return allocation_counter::currently_allocated();
    }
};

} // end of namespace orcgc

