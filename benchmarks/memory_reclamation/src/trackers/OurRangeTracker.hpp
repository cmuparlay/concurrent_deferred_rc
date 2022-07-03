/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/



#ifndef OUR_RANGE_TRACKER_HPP
#define OUR_RANGE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <algorithm>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

#include <cdrc/marked_arc_ptr.h>

template<typename T, typename Sfinae = void>
struct Aligned;

// Use user-defined conversions to pad primitive types
template<typename T>
struct alignas(128) Aligned<T, typename std::enable_if<std::is_fundamental<T>::value>::type> {
  Aligned() = default;
  Aligned(T _x) : x(_x) { }
  operator T() { return x; }
  T x;
};

// Use inheritance to pad class types
template<typename T>
struct alignas(128) Aligned<T, typename std::enable_if<std::is_class<T>::value>::type> : public T {

};

template<class T>
class MemoryTracker;
template<class T> 
class OurRangeTracker;

const int BLOCK_SIZE = 256;

template<class T>
using atomic_rc_ptr = cdrc::atomic_rc_ptr<T>;

template<class T>
using rc_ptr = cdrc::rc_ptr<T>;

template<class T>
using snapshot_ptr = cdrc::snapshot_ptr<T>;

template<class V> struct ConcurrentQueue {
private:
  struct Node {
    V value;
    atomic_rc_ptr<Node> next;
    Node(V value) : value(std::move(value)), next(nullptr) {}
  };
  atomic_rc_ptr<Node> head;
  int padding[64];
  atomic_rc_ptr<Node> tail;
  int padding2[64];

public:
  ConcurrentQueue() : head(new Node(V())), tail(head.load()) {}

  void enq(V value) {
    rc_ptr<Node> node = rc_ptr<Node>::make_shared(std::move(value));
    while (true) {
      snapshot_ptr<Node> last = tail.get_snapshot();
      snapshot_ptr<Node> next = last->next.get_snapshot();
      if (last == tail.get_snapshot()) {
        //not necessary but checks again before try
        if (next == nullptr) {
          if (last->next.compare_exchange_strong(next, node)){
            tail.compare_exchange_strong(last, node);
            return;
          }
        } else {
          tail.compare_exchange_strong(last, next);
        }
      }
    }
  }

  //---------------------------------
  V deq(){
    while (true) {
      Node* first = head;
      Node* last = tail;
      Node* next = first->next;
      if (first == head) {
        if (first == last) {
          if (next == NULL) {
            return V();
            //throw EmptyException();
          }
          tail.compare_exchange_strong(last, next);
        } else {
          if (head.compare_exchange_strong(first, next)){
            return std::move(next->value);
          }
        }
      }
    }
  }
};

template<class T> struct RangeTracker {
  struct Range {
    T* t; int low; int high;
  };

  int task_num;
  std::vector<Aligned<std::vector<Range>>> LDPool;
  MemoryTracker<T>* mt;
  ConcurrentQueue<std::vector<Range>> Q;

  RangeTracker(MemoryTracker<T>* mt, int task_num) : mt(mt),
                                                     task_num(task_num),
                                                     LDPool(task_num) {}

  ~RangeTracker() {
    for(int i = 0; i < task_num; i++)
      for(Range const& r : LDPool[i])
        mt->reclaim(r.t, i);
  }

  // merge l2 into l1 and clear l2
  void merge(std::vector<Range> &l1, std::vector<Range> &l2) {
    Range merged[BLOCK_SIZE*2];

    int i = 0, j = 0, k = 0;
    while(i < l1.size() && j < l2.size()) {
      if(l1[i].high < l2[j].high) {
        merged[k] = l1[i];
        i++;
      } else {
        merged[k] = l2[j];
        j++;
      } 
      k++;
    }

    for(; i < l1.size(); i++) {
      merged[k] = l1[i];
      k++;
    }

    for(; j < l2.size(); j++) {
      merged[k] = l2[j];
      k++;
    }
    
    l1.resize(k);
    for(i = 0; i < k; i++) {
      l1[i] = merged[i];
    }
    l2.clear();
  }



  void intersect(std::vector<Range>& MQ, const std::vector<uint64_t>& ar, int tid) {
    int i = 0;
    Range needed[BLOCK_SIZE*2];
    int k = 0;
    for(Range const& r : MQ) {
      while(i < ar.size() && ar[i] < r.high) i++;
      if(i == 0 || ar[i-1] < r.low) mt->reclaim(r.t, tid);
      else {
        needed[k] = r;
        k++;
      }
    }
    MQ.resize(k);
    for(i = 0; i < k; i++)
      MQ[i] = needed[i];
  }

  void split(std::vector<Range> &l1, std::vector<Range> &l2) {
    int new_size = l1.size()/2;
    for(int i = 0; i < l1.size()-new_size; i++)
      l2.push_back(l1[new_size+i]);
    l1.resize(new_size);
  }

  void retire(OurRangeTracker<T>* ort, T* o, int low, int high, int tid) {
    LDPool[tid].push_back((Range) {o, low, high});
    if(LDPool[tid].size() == BLOCK_SIZE/2) {
      std::vector<Range> l1 = Q.deq();
      if(l1.size() > 0) {
        std::vector<Range> l2 = Q.deq();
        merge(l1, l2);
        std::vector<uint64_t> ar = ort->sortAnnouncements(); // NOTE: This can be optimized to avoid malloc
        intersect(l1, ar, tid); 
        if(l1.size() > BLOCK_SIZE) {
          split(l1, l2); // split l1, push into l2, which is now cleared 
          Q.enq(std::move(l1));
          Q.enq(std::move(l2));
        } else if(l1.size() > BLOCK_SIZE/2) {
          Q.enq(std::move(l1));
        } else {
          merge(LDPool[tid], l1);
        }
      }
      Q.enq(LDPool[tid]);
      LDPool[tid].clear();
    }
  }
};

template<class T> class OurRangeTracker: public BaseTracker<T>{
private:
  int task_num;
  int he_num;
  int epochFreq;
  int freq;
  bool collect;
  
private:
  MemoryTracker<T>* mt;
  padded<padded<std::atomic<uint64_t>>*>* reservations;
  padded<uint64_t>* alloc_counters;
  paddedAtomic<uint64_t> epoch;
  RangeTracker<T> rt;

public:
  ~OurRangeTracker(){
    #ifdef NO_DESTRUCT
      return;
    #endif
    for(int i = 0; i < task_num; i++) clear_all(i);
    for(int i = 0; i < task_num; i++) {
      delete[] reservations[i].ui;
    }
    delete[] reservations;
    delete[] alloc_counters;
  };
  OurRangeTracker(MemoryTracker<T>* mt, int task_num, int he_num, int epochFreq, int emptyFreq, bool collect): 
  // Unlike EBR/IBR, Hazard Eras also increment epoch in retire() and
  // therefore emptyFreq is somewhat different. Use epochFreq+emptyFreq
  // for retire()'s frequency for now.
   BaseTracker<T>(task_num),mt(mt),task_num(task_num),he_num(he_num),epochFreq(epochFreq),freq(emptyFreq),collect(collect), rt(mt, task_num){
    reservations = new padded<padded<std::atomic<uint64_t>>*>[task_num];
    for (int i = 0; i<task_num; i++){
      reservations[i].ui = new padded<std::atomic<uint64_t>>[he_num];
      for (int j = 0; j<he_num; j++){
        reservations[i].ui[j].ui.store(UINT64_MAX, std::memory_order_release);
      }
    }
    alloc_counters = new padded<uint64_t>[task_num];
    epoch.ui.store(0, std::memory_order_release); // use 0 as infinity
  }
  OurRangeTracker(int task_num, int emptyFreq) : OurRangeTracker(task_num,emptyFreq,true){}

  void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
    return reserve(tid);
  }
  uint64_t getEpoch(){
    return epoch.ui.load(std::memory_order_acquire);
  }

  void copy(int src_idx, int dst_idx, int tid) {
    reservations[tid].ui[dst_idx].ui.store(reservations[tid].ui[src_idx].ui, std::memory_order_release);
  }

  void* alloc(int tid){
    alloc_counters[tid] = alloc_counters[tid]+1;
    if(alloc_counters[tid]%(epochFreq*task_num)==0){
      epoch.ui.fetch_add(1,std::memory_order_acq_rel);
    }
    char* block = (char*) malloc(sizeof(uint64_t) + sizeof(T));
    uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
    *birth_epoch = getEpoch();
    return (void*)block;
  }

  void reclaim(T* obj){
    obj->~T();
    free ((char*)obj);
  }

  T* read(std::atomic<T*>& obj, int index, int tid, T* node){
    uint64_t prev_epoch = reservations[tid].ui[index].ui.load(std::memory_order_acquire);
    while(true){
      T* ptr = obj.load(std::memory_order_acquire);
      uint64_t curr_epoch = getEpoch();
      if (curr_epoch == prev_epoch){
        return ptr;
      } else {
        // reservations[tid].ui[index].store(curr_epoch, std::memory_order_release);
        reservations[tid].ui[index].ui.store(curr_epoch, std::memory_order_seq_cst);
        prev_epoch = curr_epoch;
      }
    }
  }

  void reserve_slot(T* obj, int index, int tid, T* node){
    uint64_t prev_epoch = reservations[tid].ui[index].ui.load(std::memory_order_acquire);
    while(true){
      uint64_t curr_epoch = getEpoch();
      if (curr_epoch == prev_epoch){
        return;
      } else {
        reservations[tid].ui[index].ui.store(curr_epoch, std::memory_order_seq_cst);
        prev_epoch = curr_epoch;
      }
    }
  }
  

  void clear_all(int tid){
    //reservations[tid].ui.store(UINT64_MAX,std::memory_order_release);
    for (int i = 0; i < he_num; i++){
      reservations[tid].ui[i].ui.store(UINT64_MAX, std::memory_order_seq_cst);
    }
  }

  inline void incrementEpoch(){
    epoch.ui.fetch_add(1,std::memory_order_acq_rel);
  }
  
  void retire(T* obj, int tid){
    if(obj==NULL){return;}
    // for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
    //  assert(it->obj!=obj && "double retire error");
    // }
      
    uint64_t retire_epoch = epoch.ui.load(std::memory_order_acquire);
    uint64_t birth_epoch = *(uint64_t*)((char*)obj + sizeof(T));

    rt.retire(this, obj, birth_epoch, retire_epoch, tid);
  }
  
  std::vector<uint64_t> sortAnnouncements() {
    std::vector<uint64_t> ann;
    ann.reserve(task_num*he_num);
    for (int i = 0; i < task_num; i++)
      for (int j = 0; j < he_num; j++)
        ann.push_back(reservations[i].ui[j].ui.load(std::memory_order_acquire));
    std::sort(ann.begin(), ann.end());
    return ann;
  }
    
  bool collecting(){return collect;}
  
};


#endif
