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



#ifndef HAZARD_OPT_TRACKER_HPP
#define HAZARD_OPT_TRACKER_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include <unordered_set>

#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

template<class T>
class MemoryTracker;

template<class T>
class HazardOptTracker: public BaseTracker<T>{
private:
	int task_num;
	int slotsPerThread;
	int actualSlotsPerThread;
	int freq;
	bool collect;

	MemoryTracker<T>* mt;
	RAllocator* mem;

	std::atomic<T*>* slots;
	padded<int>* cntrs;
	padded<std::list<T*>>* retired; // TODO: use different structure to prevent malloc locking....

  // Apply the function f to every currently announced handle
  template<typename F>
  void scan_slots(F&& f) {
		for (int i = 0; i<task_num*slotsPerThread; i++){
			T* ann = slots[i].load(std::memory_order_seq_cst);
			if(ann != nullptr) f(ann);
		}
  }

	void empty(int tid, bool use_mt=true){
	  std::unordered_set<T*> announced;
    scan_slots([&](auto reserved) { announced.insert(reserved); });

		std::list<T*>* myTrash = &(retired[tid].ui);
		for (typename std::list<T*>::iterator iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			auto ptr = *iterator;
			bool danger = !ptr->deletable();
			if(announced.find(ptr) == announced.end()){
				if(use_mt) mt->reclaim(ptr, tid);
				else this->reclaim(ptr);
				this->dec_retired(tid);
				iterator = myTrash->erase(iterator);
			}
			else{++iterator;}
		}
		return;
	}

public:
	~HazardOptTracker(){
		#ifdef NO_DESTRUCT
      return;
    #endif
		for (int i = 0; i<task_num*slotsPerThread; i++){
			slots[i].store(nullptr);
		}
		for (int i = 0; i<task_num; i++){
			empty(i, false);
		}
		delete[] slots;
		delete[] retired;
		delete[] cntrs;
	};
	HazardOptTracker(MemoryTracker<T>* mt, int task_num, int slotsPerThread, int emptyFreq, bool collect): BaseTracker<T>(task_num), mt(mt){
		this->task_num = task_num;
		this->slotsPerThread = slotsPerThread;
		this->actualSlotsPerThread = std::max(16, slotsPerThread); // for padding
		this->freq = emptyFreq;
		slots = new std::atomic<T*>[task_num*actualSlotsPerThread];
		for (int i = 0; i<task_num*actualSlotsPerThread; i++){
			slots[i]=NULL;
		}
		retired = new padded<std::list<T*>>[task_num];
		cntrs = new padded<int>[task_num];
		for (int i = 0; i<task_num; i++){
			cntrs[i]=0;
			retired[i].ui = std::list<T*>();
		}
		this->collect = collect;
	}
	HazardOptTracker(int task_num, int slotsPerThread, int emptyFreq): 
		HazardOptTracker(task_num, slotsPerThread, emptyFreq, true){}

	void copy(int src_idx, int dst_idx, int tid) {
		int src = tid*actualSlotsPerThread+src_idx;
		int dst = tid*actualSlotsPerThread+dst_idx;
		slots[dst].store(slots[src], std::memory_order_release);
	}

	T* read(std::atomic<T*>& obj, int idx, int tid, T* node){
		T* ret;
		T* realptr;
		while(true){
			ret = obj.load(std::memory_order_acquire);
			realptr = (T*)((size_t)ret & 0xfffffffffffffffc);
			__builtin_prefetch((const void*)(realptr),0,0);
			if(realptr == nullptr) {
				clearSlot(idx, tid);
				return ret;
			}
			reserve_slot(realptr, idx, tid);
			if(ret == obj.load(std::memory_order_acquire)){
				return ret;
			}
		}
	}

	void reserve_slot(T* ptr, int slot, int tid){
		slots[tid*actualSlotsPerThread+slot].exchange(ptr);
	}
	void reserve_slot(T* ptr, int slot, int tid, T* node){
		slots[tid*actualSlotsPerThread+slot].exchange(ptr);
	}
	void clearSlot(int slot, int tid){
		slots[tid*actualSlotsPerThread+slot].store(NULL, std::memory_order_release);
	}
	void clearAll(int tid){
		for(int i = 0; i<slotsPerThread; i++){
			slots[tid*actualSlotsPerThread+i].store(NULL, std::memory_order_release);
		}
	}
	void clear_all(int tid){
		clearAll(tid);
	}

	void retire(T* ptr, int tid){
		if(ptr==NULL){return;}
		std::list<T*>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(*it !=ptr && "double retire error");
		// }
		myTrash->push_back(ptr);	
		if(collect && cntrs[tid]==freq*task_num){
			cntrs[tid]=0;
			empty(tid);
		}
		cntrs[tid].ui++;
	}
	

	bool collecting(){return collect;}
	
};


#endif
