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



#ifndef RCU_TRACKER_HPP
#define RCU_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

template<class T>
class MemoryTracker;

enum RCUType{type_RCU, type_QSBR};

template<class T> class RCUTracker: public BaseTracker<T>{
private:
	MemoryTracker<T>* mt;
	int task_num;
	int freq;
	int epochFreq;
	bool collect;
	RCUType type;
	
public:
	class RCUInfo{
	public:
		T* obj;
		uint64_t epoch;
		RCUInfo(T* obj, uint64_t epoch):obj(obj),epoch(epoch){}
	};
	
private:
	paddedAtomic<uint64_t>* reservations;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<RCUInfo>>* retired; 

	alignas(128) std::atomic<uint64_t> epoch;

public:
	~RCUTracker(){
		#ifdef NO_DESTRUCT
      return;
    #endif
		for(int tid = 0; tid < task_num; tid++){
			std::list<RCUInfo>* myTrash = &(retired[tid].ui);
			for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
				RCUInfo res = *iterator;
				if(res.obj->deletable()){
					iterator = myTrash->erase(iterator);
					this->reclaim(res.obj);
					// this->dec_retired(tid);
				}
				else{
					++iterator;
				}
			}			
		}
		delete[] retired;
		delete[] reservations;
		delete[] retire_counters;
		delete[] alloc_counters;
	};
	RCUTracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, RCUType type, bool collect): 
	 BaseTracker<T>(task_num),mt(mt),task_num(task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect),type(type){
		retired = new padded<std::list<RCUTracker<T>::RCUInfo>>[task_num];
		reservations = new paddedAtomic<uint64_t>[task_num];
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		for (int i = 0; i<task_num; i++){
			reservations[i].ui.store(UINT64_MAX,std::memory_order_release);
			retired[i].ui.clear();
		}
		epoch.store(0,std::memory_order_release);
	}
	RCUTracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, bool collect) : 
		RCUTracker(mt, task_num,epochFreq,emptyFreq,type_RCU,collect){}
	
	int capped_num_threads() {
		return std::min(task_num, (int) std::thread::hardware_concurrency());
	}

	void* alloc(int tid){
		alloc_counters[tid]=alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*capped_num_threads())==0){
			epoch.fetch_add(1,std::memory_order_acq_rel);
		}
		return (void*)malloc(sizeof(T));
	}
	void start_op(int tid){
		if (type == type_RCU){
			uint64_t e = epoch.load(std::memory_order_acquire);
			reservations[tid].ui.exchange(e);
		}
		
	}
	void end_op(int tid){
		if (type == type_RCU){
			reservations[tid].ui.store(UINT64_MAX,std::memory_order_release);
		} else { //if type == TYPE_QSBR
			uint64_t e = epoch.load(std::memory_order_acquire);
			reservations[tid].ui.store(e,std::memory_order_seq_cst);
		}
	}

	void clear_all(int tid) {}



	inline void incrementEpoch(){
		epoch.fetch_add(1,std::memory_order_acq_rel);
	}
	
	void __attribute__ ((deprecated)) retire(T* obj, uint64_t e, int tid){
		return retire(obj,tid);
	}
	
	void retire(T* obj, int tid){
		if(obj==NULL){return;}
		std::list<RCUInfo>* myTrash = &(retired[tid].ui);
		// for(auto it = myTrash->begin(); it!=myTrash->end(); it++){
		// 	assert(it->obj!=obj && "double retire error");
		// }
			
		uint64_t e = epoch.load(std::memory_order_acquire);
		RCUInfo info = RCUInfo(obj,e);
		myTrash->push_back(info);
		retire_counters[tid]=retire_counters[tid]+1;
		auto threshold = std::max<int>(30, freq * task_num);  // Always wait at least 30 ejects
		if(collect && retire_counters[tid]%threshold==0){
			empty(tid);
		}
	}
	
	void empty(int tid){
		uint64_t minEpoch = UINT64_MAX;
		for (int i = 0; i<task_num; i++){
			uint64_t res = reservations[i].ui.load(std::memory_order_acquire);
			if(res<minEpoch){
				minEpoch = res;
			}
		}
		
		// erase safe objects
		std::list<RCUInfo>* myTrash = &(retired[tid].ui);
		for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			RCUInfo res = *iterator;
			if(res.obj->deletable() && res.epoch<minEpoch){
				iterator = myTrash->erase(iterator);
				mt->reclaim(res.obj, tid);
				this->dec_retired(tid);
			}
			else{break;}
		}
	}
		
	bool collecting(){return collect;}
	
};


#endif
