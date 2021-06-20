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



#ifndef RANGE_TRACKER_NEW_HPP
#define RANGE_TRACKER_NEW_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

template<class T>
class MemoryTracker;

template<class T> 
class RangeTrackerNew:public BaseTracker<T>{
private:
	int task_num;
	int freq;
	int epochFreq;
	bool collect;
public:
	class IntervalInfo{
	public:
		T* obj;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
		IntervalInfo(T* obj, uint64_t b_epoch, uint64_t r_epoch):
			obj(obj), birth_epoch(b_epoch), retire_epoch(r_epoch){}
	};
	
private:
	MemoryTracker<T>* mt;
	paddedAtomic<uint64_t>* upper_reservs;
	paddedAtomic<uint64_t>* lower_reservs;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<IntervalInfo>>* retired; 

	std::atomic<uint64_t> epoch;

public:
	~RangeTrackerNew(){
		#ifdef NO_DESTRUCT
      return;
    #endif
		for(int i = 0; i < task_num; i++) clear(i);
		for(int i = 0; i < task_num; i++) empty(i, false);
		delete[] retired;
		delete[] upper_reservs;
		delete[] lower_reservs;
		delete[] retire_counters;
		delete[] alloc_counters;
	};
	RangeTrackerNew(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, bool collect): 
	 BaseTracker<T>(task_num),mt(mt),task_num(task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect){
		retired = new padded<std::list<RangeTrackerNew<T>::IntervalInfo>>[task_num];
		upper_reservs = new paddedAtomic<uint64_t>[task_num];
		lower_reservs = new paddedAtomic<uint64_t>[task_num];
		for (int i = 0; i < task_num; i++){
			upper_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
			lower_reservs[i].ui.store(UINT64_MAX, std::memory_order_release);
		}
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		epoch.store(0,std::memory_order_release);
	}
	RangeTrackerNew(int task_num, int epochFreq, int emptyFreq) : RangeTrackerNew(task_num,epochFreq,emptyFreq,true){}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return reserve(tid);
	}
	uint64_t get_epoch(){
		return epoch.load(std::memory_order_acquire);
	}

	void* alloc(int tid){
		alloc_counters[tid] = alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			epoch.fetch_add(1,std::memory_order_acq_rel);
		}
		char* block = (char*) malloc(sizeof(uint64_t) + sizeof(T));
		uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
		*birth_epoch = get_epoch();
		return (void*)block;
	}

	static uint64_t read_birth(T* obj){
		uint64_t* birth_epoch = (uint64_t*)((char*)obj + sizeof(T));
		return *birth_epoch;
	}

	void reclaim(T* obj){
		obj->~T();
		free ((char*)obj);
	}

	T* read(std::atomic<T*>& obj, int idx, int tid, T* node){
		return read(obj, tid);
	}
    T* read(std::atomic<T*>& obj, int tid){
        uint64_t prev_epoch = upper_reservs[tid].ui.load(std::memory_order_acquire);
		while(true){
			T* ptr = obj.load(std::memory_order_acquire);
			uint64_t curr_epoch = get_epoch();
			if (curr_epoch == prev_epoch){
				return ptr;
			} else {
				// upper_reservs[tid].ui.store(curr_epoch, std::memory_order_release);
				upper_reservs[tid].ui.store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		}
    }

    void reserve_slot(T* ptr, int idx, int tid, T* node){
        uint64_t prev_epoch = upper_reservs[tid].ui.load(std::memory_order_acquire);
		while(true){
			uint64_t curr_epoch = get_epoch();
			if (curr_epoch == prev_epoch){
				return;
			} else {
				// upper_reservs[tid].ui.store(curr_epoch, std::memory_order_release);
				upper_reservs[tid].ui.store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		}
    }

	void start_op(int tid){
		uint64_t e = epoch.load(std::memory_order_acquire);
		lower_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		upper_reservs[tid].ui.store(e,std::memory_order_seq_cst);
		// lower_reservs[tid].ui.store(e,std::memory_order_release);
		// upper_reservs[tid].ui.store(e,std::memory_order_release);
	}
	void end_op(int tid){
		upper_reservs[tid].ui.store(UINT64_MAX,std::memory_order_release);
		lower_reservs[tid].ui.store(UINT64_MAX,std::memory_order_release);
	}
	void reserve(int tid){
		start_op(tid);
	}
	void clear(int tid){
		end_op(tid);
	}

	
	inline void incrementEpoch(){
		epoch.fetch_add(1,std::memory_order_acq_rel);
	}
	
	void retire(T* obj, uint64_t birth_epoch, int tid){
		if(obj==NULL){return;}
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(it->obj!=obj && "double retire error");
		// }
			
		uint64_t retire_epoch = epoch.load(std::memory_order_acquire);
		myTrash->push_back(IntervalInfo(obj, birth_epoch, retire_epoch));
		retire_counters[tid]=retire_counters[tid]+1;
		if(collect && retire_counters[tid]%freq==0){
			empty(tid);
		}
	}

	void retire(T* obj, int tid){
		retire(obj, read_birth(obj), tid);
	}
	
	bool conflict(uint64_t* lower_epochs, uint64_t* upper_epochs, uint64_t birth_epoch, uint64_t retire_epoch){
		for (int i = 0; i < task_num; i++){
			if (upper_epochs[i] >= birth_epoch && lower_epochs[i] <= retire_epoch){
				return true;
			}
		}
		return false;
	}

	void empty(int tid, bool use_mt=true){
		//read all epochs
		uint64_t upper_epochs_arr[task_num];
		uint64_t lower_epochs_arr[task_num];
		for (int i = 0; i < task_num; i++){
			//sequence matters.
			lower_epochs_arr[i] = lower_reservs[i].ui.load(std::memory_order_acquire);
			upper_epochs_arr[i] = upper_reservs[i].ui.load(std::memory_order_acquire);
		}

		// erase safe objects
		std::list<IntervalInfo>* myTrash = &(retired[tid].ui);
		for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			IntervalInfo res = *iterator;
			if(res.obj->deletable() && !conflict(lower_epochs_arr, upper_epochs_arr, res.birth_epoch, res.retire_epoch)){
				if(use_mt)
					mt->reclaim(res.obj, tid);
				else
					this->reclaim(res.obj);
				this->dec_retired(tid);
				iterator = myTrash->erase(iterator);
			}
			else{++iterator;}
		}
	}

	bool collecting(){return collect;}
};


#endif
