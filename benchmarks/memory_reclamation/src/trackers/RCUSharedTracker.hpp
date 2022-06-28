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



#ifndef RCU_SHARED_TRACKER_HPP
#define RCU_SHARED_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"
#include "RCUTracker.hpp"

#include <cdrc/atomic_rc_ptr.h>

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
  ConcurrentQueue() : head(rc_ptr<Node>::make_shared(V())), tail(head.load()) {}

  void enq(V value) {
    rc_ptr<Node> node = rc_ptr<Node>::make_shared(std::move(value));
    while (true) {
      snapshot_ptr<Node> last = tail.get_snapshot();
      snapshot_ptr<Node> next = last->next.get_snapshot();
      if (last == tail.get_snapshot()) {
        //not necessary but checks again before try
        if (next == nullptr) {
          if (last->next.compare_and_swap(next, node)){
            tail.compare_and_swap(last, node);
            return;
          }
        } else {
          tail.compare_and_swap(last, next);
        }
      }
    }
  }

  //---------------------------------
  V deq(){
    while (true) {
      snapshot_ptr<Node> first = head.get_snapshot();
      snapshot_ptr<Node> last = tail.get_snapshot();
      snapshot_ptr<Node> next = first->next.get_snapshot();
      if (first == head.get_snapshot()) {
        if (first == last) {
          if (next == NULL) {
            return V();
            //throw EmptyException();
          }
          tail.compare_and_swap(last, next);
        } else {
          if (head.compare_and_swap(first, next)){
            return std::move(next->value);
          }
        }
      }
    }
  }
};


template<class T>
class MemoryTracker;

template<class T> class RCUSharedTracker: public BaseTracker<T>{
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

		bool operator < (const RCUInfo& str) const
    {
        return (epoch < str.epoch);
    }
	};
	
private:
	paddedAtomic<uint64_t>* reservations;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<RCUInfo>>* retired; 

	alignas(128) std::atomic<uint64_t> epoch;
	alignas(128) ConcurrentQueue<std::list<RCUInfo>> SharedQueue;

public:
	~RCUSharedTracker(){
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
	RCUSharedTracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, RCUType type, bool collect): 
	 BaseTracker<T>(task_num),mt(mt),task_num(task_num),freq(emptyFreq),epochFreq(epochFreq),collect(collect),type(type){
		retired = new padded<std::list<RCUSharedTracker<T>::RCUInfo>>[task_num];
		reservations = new paddedAtomic<uint64_t>[task_num];
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		for (int i = 0; i<task_num; i++){
			reservations[i].ui.store(UINT64_MAX,std::memory_order_release);
			retired[i].ui.clear();
		}
		epoch.store(0,std::memory_order_release);
	}
	RCUSharedTracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, bool collect) : 
		RCUSharedTracker(mt, task_num,epochFreq,emptyFreq,type_RCU,collect){}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return start_op(tid);
	}
	
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
			reservations[tid].ui.store(e,std::memory_order_seq_cst);
		}
		
	}
	void end_op(int tid){
		if (type == type_RCU){
			reservations[tid].ui.store(UINT64_MAX,std::memory_order_seq_cst);
		} else { //if type == TYPE_QSBR
			uint64_t e = epoch.load(std::memory_order_acquire);
			reservations[tid].ui.store(e,std::memory_order_seq_cst);
		}
	}
	void reserve(int tid){
		start_op(tid);
	}
	void clear_all(int tid){
		end_op(tid);
	}



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
		if(collect && retire_counters[tid]%freq==0){
			// empty retired list
	    std::list<RCUInfo> l1 = SharedQueue.deq();
	    if(!l1.empty()) {
	      std::list<RCUInfo> l2 = SharedQueue.deq();
	      empty(l1, tid);
	      empty(l2, tid);
	      l1.merge(l2);
	      int l1_size = l1.size();
	      if(l1_size > freq*2) {
	        split(l1, l2); // split l1, push into l2, which is now cleared 
	        SharedQueue.enq(std::move(l1));
	        SharedQueue.enq(std::move(l2));
	      } else if(l1_size > freq) {
	        SharedQueue.enq(std::move(l1));
	      } else {
	        myTrash->merge(l1);
	      }
	    }
	    SharedQueue.enq(std::move(*myTrash));
	    retire_counters[tid] = 0;
		}
	}
	
	void split(std::list<RCUInfo>& l1, std::list<RCUInfo>& l2) {
		l2.splice(l2.begin(), l1, l1.begin(), std::next(l1.begin(), l1.size() / 2));
	}

	void empty(std::list<RCUInfo>& retired_list, const int tid){
		uint64_t minEpoch = UINT64_MAX;
		for (int i = 0; i<task_num; i++){
			uint64_t res = reservations[i].ui.load(std::memory_order_acquire);
			if(res<minEpoch){
				minEpoch = res;
			}
		}
		
		// erase safe objects
		for (auto iterator = retired_list.begin(), end = retired_list.end(); iterator != end; ) {
			RCUInfo res = *iterator;
			if(res.obj->deletable() && res.epoch<minEpoch){
				iterator = retired_list.erase(iterator);
				mt->reclaim(res.obj, tid);
				this->dec_retired(tid);
			}
			else{break;}
		}
	}

	bool collecting(){return collect;}
	
};


#endif
