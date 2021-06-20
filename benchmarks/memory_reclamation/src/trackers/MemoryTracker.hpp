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



#ifndef MEMORY_TRACKER_HPP
#define MEMORY_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"
#include "RCUTracker.hpp"
#include "RangeTrackerNew.hpp"
#include "HazardTracker.hpp"
#include "HazardOptTracker.hpp"
// #include "ARTracker.hpp"
#include "HETracker.hpp"
#include "WFETracker.hpp"




enum TrackerType{
	//for epoch-based trackers.
	NIL = 0,
	RCU = 2,
	Interval = 4,
	Range = 6,
	Range_new = 8,
	QSBR = 10,
	Range_TP = 12,
	//for HP-like trackers.
	Hazard = 1,
	AR = 20,
	HazardOpt = 21,
	Hazard_dynamic = 3,
	HE = 5,
	WFE = 7
};

template<class T>
class MemoryTracker{
private:
	BaseTracker<T>* tracker = NULL;
	TrackerType type = NIL;
	int task_num;
	padded<int*>* slot_renamers = NULL;
	padded<std::atomic<int64_t>>* allocated_nodes;
public:
	~MemoryTracker() {
		#ifdef NO_DESTRUCT
      return;
    #endif
		for(int i = 0; i < task_num; i++)
			free(slot_renamers[i].ui);
		delete[] slot_renamers;
		delete[] allocated_nodes;
		if (type == NIL){
			delete ((BaseTracker<T>*) tracker);
		} else if (type == RCU){
			delete ((RCUTracker<T>*) tracker);
		} else if (type == Range_new){
			delete ((RangeTrackerNew<T>*) tracker);
		} else if (type == Hazard){
			delete ((HazardTracker<T>*) tracker);
		} else if (type == HazardOpt){
			delete ((HazardOptTracker<T>*) tracker);
		} else if (type == HE){
			delete ((HETracker<T>*) tracker);
    }
	}
	MemoryTracker(GlobalTestConfig* gtc, int epoch_freq, int empty_freq, int slot_num, bool collect){
	  task_num = gtc->task_num;
		std::string tracker_type = gtc->getEnv("tracker");
		if (tracker_type.empty()){
			tracker_type = "RCU";
			gtc->setEnv("tracker", "RCU");
		}

		allocated_nodes = new padded<std::atomic<int64_t>>[task_num];
		for (int i = 0; i < task_num; i++) allocated_nodes[i].ui.store(0);

		slot_renamers = new padded<int*>[task_num];
		for (int i = 0; i < task_num; i++){
			slot_renamers[i].ui = (int*) aligned_alloc(64, (((slot_num*sizeof(int))/64)+1)*64);
			// slot_renamers[i].ui = new int[slot_num];
			for (int j = 0; j < slot_num; j++){
				slot_renamers[i].ui[j] = j;
			}
		}
		if (tracker_type == "NIL"){
			tracker = new BaseTracker<T>(task_num);
			type = NIL;
		} else if (tracker_type == "RCU"){
			tracker = new RCUTracker<T>(this, task_num, epoch_freq, empty_freq, collect);
			type = RCU;
		} else if (tracker_type == "Range_new"){
			tracker = new RangeTrackerNew<T>(this, task_num, epoch_freq, empty_freq, collect);
			type = Range_new;
		} else if (tracker_type == "Hazard"){
			tracker = new HazardTracker<T>(this, task_num, slot_num, empty_freq, collect);
			type = Hazard;
		} else if (tracker_type == "HazardOpt"){
			tracker = new HazardOptTracker<T>(this, task_num, slot_num, empty_freq, collect);
			type = HazardOpt;
		// } else if (tracker_type == "AR"){
		// 	tracker = new ARTracker<T>(task_num, slot_num, empty_freq, collect);
		// 	type = AR;
		} else if (tracker_type == "HE"){
			// tracker = new HETracker<T>(task_num, slot_num, 1, collect);
			tracker = new HETracker<T>(this, task_num, slot_num, epoch_freq, empty_freq, collect);
			type = HE;
		} 
		// else if (tracker_type == "WFE"){
		// 	tracker = new WFETracker<T>(task_num, slot_num, epoch_freq, empty_freq, collect);
		// 	type = WFE;
  //   }

		else {
			errexit("constructor - tracker type error.");
		}
		
		
	}

	void* alloc(){
		return tracker->alloc();
	}

	void* alloc(int tid){
		allocated_nodes[tid].ui.store(allocated_nodes[tid].ui.load(std::memory_order_relaxed)+1, std::memory_order_release);
		return tracker->alloc(tid);
	}
	//NOTE: reclaim shall be only used to thread-local objects.
	void reclaim(T* obj){
		if(obj!=nullptr)
			tracker->reclaim(obj);
	}

	void reclaim(T* obj, int tid){
		if (obj!=nullptr) {
			allocated_nodes[tid].ui.store(allocated_nodes[tid].ui.load(std::memory_order_relaxed)-1, std::memory_order_release);
			tracker->reclaim(obj, tid);
		}
	}

	void start_op(int tid){
		//tracker->inc_opr(tid);
		tracker->start_op(tid);
	}

	void end_op(int tid){
		tracker->end_op(tid);
	}

	T* read(std::atomic<T*>& obj, int idx, int tid, T* node){
		return tracker->read(obj, slot_renamers[tid].ui[idx], tid, node);
	}

	void reserve_slot(T* ptr, int slot, int tid, T* node){
		return tracker->reserve_slot(ptr, slot, tid, node);
	}

	void copy(int src_idx, int dst_idx, int tid) {
		tracker->copy(slot_renamers[tid].ui[src_idx], slot_renamers[tid].ui[dst_idx], tid);
	}

	void transfer(int src_idx, int dst_idx, int tid){
		int tmp = slot_renamers[tid].ui[src_idx];
		slot_renamers[tid].ui[src_idx] = slot_renamers[tid].ui[dst_idx];
		slot_renamers[tid].ui[dst_idx] = tmp;
		// tracker->transfer(src_idx, dst_idx, tid);
	}

	void release(int idx, int tid){
		tracker->release(slot_renamers[tid].ui[idx], tid);
	}
	
	void clear_all(int tid){
		tracker->clear_all(tid);
	}

	void retire(T* obj, int tid){
		tracker->inc_retired(tid);
		tracker->retire(obj, tid);
	}

	int64_t get_allocated() {
		int64_t sum = 0;
		for(int i = 0; i < task_num; i++)
			sum += allocated_nodes[i].ui.load();
		return sum;
	}

	uint64_t get_retired_cnt(int tid){
		if (type){
			return tracker->get_retired_cnt(tid);
		} else {
			return 0;
		}
	}
};


#endif
