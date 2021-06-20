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



#ifndef HE_TRACKER_HPP
#define HE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

template<class T> class HETracker: public BaseTracker<T>{
private:
  int task_num;
  int he_num;
  int epochFreq;
  int freq;
  bool collect;

  
public:
  class HEInfo{
  public:
    T* obj;
    uint64_t birth_epoch;
    uint64_t retire_epoch;
    HEInfo(T* obj, uint64_t b_epoch, uint64_t r_epoch):
      obj(obj), birth_epoch(b_epoch), retire_epoch(r_epoch){}
  };
  
private:
  padded<std::atomic<uint64_t>*>* reservations;
  padded<uint64_t>* retire_counters;
  padded<uint64_t>* alloc_counters;
  padded<std::list<HEInfo>>* retired; 

  std::atomic<uint64_t> epoch;

public:
  ~HETracker(){};
  HETracker(int task_num, int he_num, int epochFreq, int emptyFreq, bool collect): 
   BaseTracker<T>(task_num),task_num(task_num),he_num(he_num),epochFreq(epochFreq),freq(emptyFreq),collect(collect){
    retired = new padded<std::list<HETracker<T>::HEInfo>>[task_num];
    reservations = new padded<std::atomic<uint64_t>*>[task_num];
    for (int i = 0; i<task_num; i++){
      reservations[i].ui = new std::atomic<uint64_t>[he_num];
      for (int j = 0; j<he_num; j++){
        reservations[i].ui[j].store(0, std::memory_order_release);
      }
    }
    retire_counters = new padded<uint64_t>[task_num];
    alloc_counters = new padded<uint64_t>[task_num];
    epoch.store(0, std::memory_order_release);
  }
  HETracker(int task_num, int emptyFreq) : HETracker(task_num,emptyFreq,true){}

  void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
    return reserve(tid);
  }
  uint64_t getEpoch(){
    return epoch.load(std::memory_order_acquire);
  }

  void* alloc(int tid){
    alloc_counters[tid] = alloc_counters[tid]+1;
    if(alloc_counters[tid]%(epochFreq*task_num)==0){
      epoch.fetch_add(1,std::memory_order_acq_rel);
    }
    char* block = (char*) malloc(sizeof(uint64_t) + sizeof(T));
    uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
    *birth_epoch = getEpoch();
    return (void*)block;
  }

  uint64_t read_birth(T* obj){
    uint64_t* birth_epoch = (uint64_t*)((char*)obj + sizeof(T));
    return *birth_epoch;
  }

  void reclaim(T* obj){
    obj->~T();
    free ((char*)obj);
  }

  T* read(std::atomic<T*>& obj, int index, int tid){
    uint64_t prev_epoch = reservations[tid].ui[index].load(std::memory_order_acquire);
    while(true){
      T* ptr = obj.load(std::memory_order_acquire);
      uint64_t curr_epoch = getEpoch();
      if (curr_epoch == prev_epoch){
        return ptr;
      } else {
        // reservations[tid].ui[index].store(curr_epoch, std::memory_order_release);
        reservations[tid].ui[index].store(curr_epoch, std::memory_order_seq_cst);
        prev_epoch = curr_epoch;
      }
    }
  }

  void reserve_slot(T* obj, int index, int tid){
    uint64_t prev_epoch = reservations[tid].ui[index].load(std::memory_order_acquire);
    while(true){
      uint64_t curr_epoch = getEpoch();
      if (curr_epoch == prev_epoch){
        return;
      } else {
        reservations[tid].ui[index].store(curr_epoch, std::memory_order_seq_cst);
        prev_epoch = curr_epoch;
      }
    }
  }
  

  void clear(int tid){
    //reservations[tid].ui.store(UINT64_MAX,std::memory_order_release);
    for (int i = 0; i < he_num; i++){
      reservations[tid].ui[i].store(0, std::memory_order_seq_cst);
    }
  }

  inline void incrementEpoch(){
    epoch.fetch_add(1,std::memory_order_acq_rel);
  }
  
  void retire(T* obj, int tid){
    retire(obj, read_birth(obj), tid);
  }

  void retire(T* obj, uint64_t birth_epoch, int tid){
    if(obj==NULL){return;}
    std::list<HEInfo>* myTrash = &(retired[tid].ui);
    // for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
    //  assert(it->obj!=obj && "double retire error");
    // }
      
    uint64_t retire_epoch = epoch.load(std::memory_order_acquire);
    myTrash->push_back(HEInfo(obj, birth_epoch, retire_epoch));
    retire_counters[tid]=retire_counters[tid]+1;
    if(collect && retire_counters[tid]%freq==0){
      empty(tid);
    }
  }
  
  bool can_delete(uint64_t birth_epoch, uint64_t retire_epoch){
    for (int i = 0; i < task_num; i++){
      for (int j = 0; j < he_num; j++){
        const uint64_t epo = reservations[i].ui[j].load(std::memory_order_acquire);
        if (epo < birth_epoch || epo > retire_epoch || epo == 0){
          continue;
        } else {
          return false;
        }
      }
    }
    return true;
  }

  void empty(int tid){
    // erase safe objects
    std::list<HEInfo>* myTrash = &(retired[tid].ui);
    for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
      HEInfo res = *iterator;
      if(can_delete(res.birth_epoch, res.retire_epoch)){
        reclaim(res.obj);
        iterator = myTrash->erase(iterator);
        this->dec_retired(tid);
      }
      else{++iterator;}
    }
  }
    
  bool collecting(){return collect;}
  
};


#endif