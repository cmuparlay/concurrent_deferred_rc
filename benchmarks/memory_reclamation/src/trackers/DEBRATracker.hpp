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

// NOTE: no point scanning announcement array in a deamortized manner
// NOTE: 

// TODO: tune read delay
// TODO: cap task_num

#ifndef DEBRA_TRACKER_HPP
#define DEBRA_TRACKER_HPP

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

#define BITS_EPOCH(ann) ((ann)&~(1ull))
#define QUIESCENT(ann) ((ann)&1ull)
#define GET_WITH_QUIESCENT(ann) ((ann)|1ull)

template<class T> class DEBRATracker: public BaseTracker<T>{
private:
	static const uint64_t EPOCH_INCREMENT = 2;
	static const int NUMBER_OF_EPOCH_BAGS = 3; // why is this more than 3?
	static const int NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS = 0;

	MemoryTracker<T>* mt;
	int task_num;
	int min_ops_before_read; // emptyf, either 1 or 20, default 30
	bool collect;

	template<class V>
	struct alignas(128) AlignedAtomic : public std::atomic<V> {
		std::atomic<V> val;
		AlignedAtomic() = default;
		AlignedAtomic(V val) : val(val) {}
		AlignedAtomic(AlignedAtomic& other) : val(other.val) {}
		V load(std::memory_order order = std::memory_order_seq_cst) {
			return val.load(order);
		}
		void store(V v, std::memory_order order = std::memory_order_seq_cst) {
			return val.store(v, order);
		}
	};

	struct alignas(128) ThreadData {
		uint64_t localvar_announcedEpoch;
		// NOTE: the following might be faster as a vector
		std::vector<T*> epochbags[NUMBER_OF_EPOCH_BAGS]; // NOTE: potential for false sharing in the list
		int index;
		int checked;
		int opsSinceRead;

		// ThreadData() : localvar_announcedEpoch(0),
		//                index(0),
		//                checked(0),
		//                opsSinceRead(0) {}
		// ThreadData(ThreadData& t) :  localvar_announcedEpoch(t.localvar_announcedEpoch),
		// 							               index(t.index),
		// 							               checked(t.checked),
		// 							               opsSinceRead(0) {}
	};

	alignas(128) std::atomic<uint64_t> epoch;
	alignas(128) std::vector<AlignedAtomic<uint64_t>> announcedEpoch;
	alignas(128) std::vector<ThreadData> threadData;

public:
	~DEBRATracker(){ 
		#ifdef NO_DESTRUCT
      return;
    #endif
    for (int tid=0;tid<task_num;++tid) {
        // move contents of all bags into pool
        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
        	empty(tid, threadData[tid].epochbags[i]);
        }
    }
    // delete[] threadData;
	};
	DEBRATracker(MemoryTracker<T>* mt, int task_num, int epochFreq, int emptyFreq, bool collect): 
	       BaseTracker<T>(task_num), 
	       mt(mt),
	       task_num(task_num),
	       min_ops_before_read(emptyFreq),
	       collect(collect),
	       epoch(0),
	       announcedEpoch(task_num+1, GET_WITH_QUIESCENT(0ull)),
	       threadData(task_num+1) {
	  // threadData = new ThreadData[task_num+1];
	}
	       

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return start_op(tid);
	}

	void* alloc(int tid){
		return (void*)malloc(sizeof(T));
	}

  // rotate the epoch bags and reclaim any objects retired two epochs ago.
  inline void rotateEpochBags(const int tid) {
    int nextIndex = (threadData[tid].index+1) % NUMBER_OF_EPOCH_BAGS;
    empty(tid, threadData[tid].epochbags[(nextIndex+NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS) % NUMBER_OF_EPOCH_BAGS]);
    threadData[tid].index = nextIndex;
  }

  void start_op(int tid){
    bool result = false;

    uint64_t readEpoch = epoch.load();
    const uint64_t ann = threadData[tid].localvar_announcedEpoch;
    threadData[tid].localvar_announcedEpoch = readEpoch;

    // if our announced epoch was different from the current epoch
    if (readEpoch != ann /* invariant: ann is not quiescent */) {
        // rotate the epoch bags and
        // reclaim any objects retired two epochs ago.
        threadData[tid].checked = 0;
        rotateEpochBags(tid);
        //this->template rotateAllEpochBags<First, Rest...>(tid, reclaimers, 0);
        result = true;
    }
    // we should announce AFTER rotating bags if we're going to do so!!
    // (very problematic interaction with lazy dirty page purging in jemalloc triggered by bag rotation,
    //  which causes massive non-quiescent regions if non-Q announcement happens before bag rotation)
    announcedEpoch[tid].store(readEpoch, std::memory_order_seq_cst); // note: this must be written, regardless of whether the announced epochs are the same, because the quiescent bit will vary
    // note: readEpoch, when written to announcedEpoch[tid],
    //       sets the state to non-quiescent and non-neutralized

    // incrementally scan the announced epochs of all threads
    // TODO: amortize this scan
    if (++threadData[tid].opsSinceRead == min_ops_before_read*task_num) {
                threadData[tid].opsSinceRead = 0;
   		for(int i = 0; i < task_num; i++) {
   			uint64_t otherAnnounce = announcedEpoch[i].load(std::memory_order_relaxed);
   			if (!(BITS_EPOCH(otherAnnounce) == readEpoch || QUIESCENT(otherAnnounce)))
   				return;
   		}
   		epoch.compare_exchange_strong(readEpoch, readEpoch+EPOCH_INCREMENT);
    }
	}

	void end_op(int tid){
		announcedEpoch[tid].store(GET_WITH_QUIESCENT(threadData[tid].localvar_announcedEpoch), std::memory_order_release);
	}

	void reserve(int tid){
		start_op(tid);
	}
	void clear_all(int tid){
		end_op(tid);
	}
	
	void __attribute__ ((deprecated)) retire(T* obj, uint64_t e, int tid){
		return retire(obj,tid);
	}
	
	void retire(T* obj, int tid){
		if(obj==NULL){return;}
		threadData[tid].epochbags[threadData[tid].index].push_back(obj);
	}
	
	void empty(int tid, std::vector<T*>& myTrash){
		for(T* const& obj : myTrash) {
			if(obj->deletable()) {
				mt->reclaim(obj, tid);
				this->dec_retired(tid);
			}
		}
		myTrash.clear();
	}
		
	bool collecting(){return collect;}
	
};


#endif
