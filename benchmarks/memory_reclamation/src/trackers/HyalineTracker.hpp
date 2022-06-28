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



#ifndef HYALINE_TRACKER_HPP
#define HYALINE_TRACKER_HPP

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

template<class T> class HyalineTracker: public BaseTracker<T>{
private:
	struct Reservation;

	struct Node { // TODO: unclear if this should be aligned
		T* obj;
		union {      // Each node takes 3 memory words
			std::atomic<int64_t> refc;  // REFS: Refer. counter
			Node* bnext;    // SLOT: Next node
		};
		Node* next;           // SLOT: After retiring
		Node* blink;    // REFS: First SLOT node,

		Node(T* obj) : obj(obj), bnext(nullptr), next(nullptr), blink(nullptr) {}
	};

	inline static Node* invptr = reinterpret_cast<Node*>(0x1);
	inline static const int64_t REFC_PROTECT = (1ull << 62); 

	struct alignas(128) Batch {
		Node* first;
		Node* refs;
		int counter;
		Batch() : first(nullptr), refs(nullptr), counter(0) {}
	};

	struct alignas(128) Reservation {
		std::atomic<Node*> list;
		Reservation() : list(invptr) {}
	};

	MemoryTracker<T>* mt;
	int task_num;
	int freq;      // batch size
	int epochFreq; // unused
	bool collect;
	std::vector<Batch> local_batch;
	std::vector<Reservation> rsrv; 

public:
	~HyalineTracker() {
		#ifdef NO_DESTRUCT
      return;
    #endif
    for(int i = 0; i < task_num; i++) {
    	Node* node = local_batch[i].first;
    	while(node != nullptr) {
    		Node* next = node->bnext;
    		T* obj = node->obj;
    		obj->~T();
				free(obj);
    		delete node;
    		if(node == local_batch[i].refs) break;
    		node = next;
    	}
    }
	}

	HyalineTracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, bool collect): 
	 BaseTracker<T>(task_num),
	 mt(mt),
	 task_num(task_num),
	 freq(emptyFreq),
	 epochFreq(epochFreq),
	 collect(collect),
	 local_batch(task_num),
	 rsrv(task_num) {}

	void* alloc(int tid){
		return (void*)malloc(sizeof(T));
	}
	void start_op(int tid){
			rsrv[tid].list.store(nullptr);	
	}
	void end_op(int tid){
			Node* p = rsrv[tid].list.exchange(invptr);
			assert(p != invptr);
			traverse(p, tid);
	}

	void traverse(Node* next, const int tid) {
		while(next != nullptr) {
			Node* curr = next;
			assert(curr != invptr);
			next = curr->next;
			Node* refs = curr->blink;
			if(refs->refc.fetch_add(-1) == 1) free_batch(refs, tid);
		}
	}

	void free_batch(Node* refs, const int tid) {
		Node* n = refs->blink;
		do {
			Node* obj = n;
			// refc and bnext overlap and are 0
			// (nullptr) for the last REFS node
			assert(n != invptr);
			n = n->bnext;
			mt->reclaim(obj->obj, tid);
			this->dec_retired(tid);
			delete obj;
		} while(n != nullptr);
	}

	void retire(T* obj, const int tid) {
		Batch& batch = local_batch[tid];
		if(obj==nullptr){return;}
		Node* node = new Node(obj);
		if(!batch.first) { // the REFS node
			batch.refs = node;
			node->refc.store(REFC_PROTECT, std::memory_order_release);
		} else { // SLOT nodes
			node->blink = batch.refs; // points to REFS
			node->bnext = batch.first;
		}
		batch.first = node;
		// Must have MAX_THREADS+1 nodes to insert to
		// MAX_THREADS lists, exit if do not have enough
		if(batch.counter++ < freq*task_num) return;
		// blink of REFS points to the 1st SLOT node
		batch.refs->blink = batch.first;
		Node* curr = batch.first;
		int64_t cnt = -REFC_PROTECT;
		for(int i = 0; i < task_num; i++) {
			while(true) {
				Node* prev = rsrv[i].list.load();
				if(prev == invptr) break;
				curr->next = prev;
				if(rsrv[i].list.compare_exchange_strong(prev, curr)) {
					cnt++;
					break;
				}
			}
			curr = curr->bnext;
		}
		if(batch.refs->refc.fetch_add(cnt) == -cnt) // Finish
			free_batch(batch.refs, tid); // retiring: change refc
		batch.first = nullptr;
		batch.counter = 0;
	}

	void clear_all(int tid) {}
		
	bool collecting(){return collect;}
	
};


#endif
