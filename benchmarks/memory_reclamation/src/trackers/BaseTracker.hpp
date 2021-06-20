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



#ifndef BASE_TRACKER_HPP
#define BASE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"


template<class T> class BaseTracker{
public:
	padded<uint64_t>* retired_cnt;

	BaseTracker(int task_num){
		retired_cnt = new padded<uint64_t>[task_num];
		for(int i = 0 ; i<task_num; i++){
			retired_cnt[i].ui = 0;
		}
	}

	~BaseTracker() {
		#ifdef NO_DESTRUCT
      return;
    #endif
		delete[] retired_cnt;
	}

	uint64_t get_retired_cnt(int tid){
		return retired_cnt[tid].ui;
	}

	void inc_retired(int tid){
		retired_cnt[tid].ui++;
	}
	void dec_retired(int tid){
		retired_cnt[tid].ui--;
	}

	virtual void* alloc(int tid){
		return alloc();
	}

	virtual void* alloc(){
		return (void*)malloc(sizeof(T));
	}
	//NOTE: reclaim shall be only used to thread-local objects.
	virtual void reclaim(T* obj){
		assert(obj != NULL);
		obj->~T();
		free(obj);
	}

	//NOTE: reclaim (obj, tid) should be used on all retired objects.
	virtual void reclaim(T* obj, int tid){
		reclaim(obj);
	}

	virtual void start_op(int tid){}
	
	virtual void end_op(int tid){}

	virtual T* read(std::atomic<T*>& obj, int idx, int tid, T* node){
		return obj.load(std::memory_order_acquire);
	}
	
	virtual void transfer(int src_idx, int dst_idx, int tid){}

	virtual void copy(int src_idx, int dst_idx, int tid){}

	virtual void reserve_slot(T* obj, int idx, int tid, T* node){}

	virtual void release(int idx, int tid){}

	virtual void clear_all(int tid){}

	virtual void retire(T* obj, int tid){}
};

#endif
