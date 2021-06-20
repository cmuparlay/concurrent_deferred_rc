/*

Based on HETracker.hpp.
WFE-related changes are under the 2-Clause BSD license:

Copyright (c) 2019 Ruslan Nikolaev.  All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

The original copyright notice from HETracker.hpp:

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


#ifndef WFE_TRACKER_HPP
#define WFE_TRACKER_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

#include "BaseTracker.hpp"

#include "dcas.hpp"

union word_pair_t {
	std::atomic<uint64_t> pair[2];
	std::atomic<__uint128_t> full;
};

union value_pair_t {
	uint64_t pair[2];
	__uint128_t full;
};

struct state_t {
	word_pair_t result;
	std::atomic<uint64_t> epoch;
	std::atomic<uint64_t> pointer;
};

template<class T> class WFETracker: public BaseTracker<T>{
private:
	int task_num;
	int he_num;
	int epochFreq;
	int freq;
	bool collect;

public:
	struct WFESlot {
		padded<state_t*> state;
		padded<word_pair_t*> slot;
	};

	class WFEInfo {
	public:
		T* obj;
		uint64_t birth_epoch;
		uint64_t retire_epoch;
		WFEInfo(T* obj, uint64_t b_epoch, uint64_t r_epoch):
			obj(obj), birth_epoch(b_epoch), retire_epoch(r_epoch){}
	};

private:
	WFESlot* reservations;
	padded<uint64_t>* retire_counters;
	padded<uint64_t>* alloc_counters;
	padded<std::list<WFEInfo>>* retired;
	paddedAtomic<uint64_t> counter_start, counter_end;

	paddedAtomic<uint64_t> epoch;

public:
	~WFETracker(){};
	WFETracker(int task_num, int he_num, int epochFreq, int emptyFreq, bool collect):
	// Unlike EBR/IBR, Hazard Eras also increment epoch in retire() and
	// therefore emptyFreq is somewhat different. Use epochFreq+emptyFreq
	// for retire()'s frequency for now.
	 BaseTracker<T>(task_num),task_num(task_num),he_num(he_num),epochFreq(epochFreq),freq(epochFreq+emptyFreq),collect(collect){
		retired = new padded<std::list<WFETracker<T>::WFEInfo>>[task_num];
		reservations = new WFESlot[task_num];
		for (int i = 0; i<task_num; i++){
			reservations[i].state.ui = new state_t[he_num];
			reservations[i].slot.ui = new word_pair_t[he_num+2];
			for (int j = 0; j<he_num; j++){
				reservations[i].state.ui[j].result.pair[0] = 0;
				reservations[i].state.ui[j].result.pair[1] = 0;
				reservations[i].state.ui[j].pointer = 0;
				reservations[i].state.ui[j].epoch = 0;
				reservations[i].slot.ui[j].pair[0] = 0;
				reservations[i].slot.ui[j].pair[1] = 0;
			}
			reservations[i].slot.ui[he_num].pair[0] = 0;
			reservations[i].slot.ui[he_num].pair[1] = 0;
			reservations[i].slot.ui[he_num+1].pair[0] = 0;
			reservations[i].slot.ui[he_num+1].pair[1] = 0;
		}
		retire_counters = new padded<uint64_t>[task_num];
		alloc_counters = new padded<uint64_t>[task_num];
		counter_start.ui.store(0, std::memory_order_release);
		counter_end.ui.store(0, std::memory_order_release);
		epoch.ui.store(1, std::memory_order_release); // use 0 as infinity
	}
	WFETracker(int task_num, int emptyFreq) : WFETracker(task_num,emptyFreq,true){}

	void __attribute__ ((deprecated)) reserve(uint64_t e, int tid){
		return reserve(tid);
	}
	uint64_t getEpoch(){
		return epoch.ui.load(std::memory_order_acquire);
	}

	inline void help_thread(int tid, int index, int mytid)
	{
		value_pair_t last_result;
		last_result.full = dcas_load(reservations[tid].state.ui[index].result.full, std::memory_order_acquire);
		if (last_result.pair[0] != (uint64_t) -1LL)
			return;
		uint64_t birth_epoch = reservations[tid].state.ui[index].epoch.load(std::memory_order_acquire);
		reservations[mytid].slot.ui[he_num].pair[0].store(birth_epoch, std::memory_order_seq_cst);
		std::atomic<T*> *obj = (std::atomic<T*> *) reservations[tid].state.ui[index].pointer.load(std::memory_order_acquire);
		uint64_t seqno = reservations[tid].slot.ui[index].pair[1].load(std::memory_order_acquire);
		if (last_result.pair[1] == seqno) {
			uint64_t prev_epoch = getEpoch();
			do {
				reservations[mytid].slot.ui[he_num+1].pair[0].store(prev_epoch, std::memory_order_seq_cst);
				T* ptr = obj ? obj->load(std::memory_order_acquire) : nullptr;
				uint64_t curr_epoch = getEpoch();
				if (curr_epoch == prev_epoch) {
					value_pair_t value;
					value.pair[0] = (uint64_t) ptr;
					value.pair[1] = curr_epoch;
					if (dcas_compare_exchange_strong(reservations[tid].state.ui[index].result.full, last_result.full, value.full, std::memory_order_acq_rel, std::memory_order_acquire)) {
						value.pair[0] = curr_epoch;
						value.pair[1] = seqno + 1;
						value_pair_t old;
						old.pair[1] = reservations[tid].slot.ui[index].pair[1].load(std::memory_order_acquire);
						old.pair[0] = reservations[tid].slot.ui[index].pair[0].load(std::memory_order_acquire);
						do { // 2 iterations at most
							if (old.pair[1] != seqno)
								break;
						} while (!dcas_compare_exchange_weak(reservations[tid].slot.ui[index].full, old.full, value.full, std::memory_order_acq_rel, std::memory_order_acquire));
					}
					break;
				}
				prev_epoch = curr_epoch;
			} while (last_result.full == dcas_load(reservations[tid].state.ui[index].result.full, std::memory_order_acquire));
			reservations[mytid].slot.ui[he_num+1].pair[0].store(0, std::memory_order_seq_cst);
		}
		reservations[mytid].slot.ui[he_num].pair[0].store(0, std::memory_order_seq_cst);
	}

	inline void help_read(int mytid)
	{
		// locate threads that need helping
		uint64_t ce = counter_end.ui.load(std::memory_order_acquire);
		uint64_t cs = counter_start.ui.load(std::memory_order_acquire);
		if (cs - ce != 0) {
			for (int i = 0; i < task_num; i++) {
				for (int j = 0; j < he_num; j++) {
					uint64_t result_ptr = reservations[i].state.ui[j].result.pair[0].load(std::memory_order_acquire);
					if (result_ptr == (uint64_t) -1LL) {
						help_thread(i, j, mytid);
					}
				}
			}
		}
	}

	void* alloc(int tid){
		alloc_counters[tid] = alloc_counters[tid]+1;
		if(alloc_counters[tid]%(epochFreq*task_num)==0){
			// help other threads first
			help_read(tid);
			// only after that increment the counter
			epoch.ui.fetch_add(1,std::memory_order_acq_rel);
		}
		char* block = (char*) malloc(sizeof(uint64_t) + sizeof(T));
		uint64_t* birth_epoch = (uint64_t*)(block + sizeof(T));
		*birth_epoch = getEpoch();
		return (void*)block;
	}

	void reclaim(T* obj)
	{
		obj->~T();
		free ((char*)obj);
	}

	T* read(std::atomic<T*>& obj, int index, int tid, T* node)
	{
		// fast path
		uint64_t prev_epoch = reservations[tid].slot.ui[index].pair[0].load(std::memory_order_acquire);
		size_t attempts = 16;
		do {
			T* ptr = obj.load(std::memory_order_acquire);
			uint64_t curr_epoch = getEpoch();
			if (curr_epoch == prev_epoch) {
				return ptr;
			} else {
				reservations[tid].slot.ui[index].pair[0].store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		} while (--attempts != 0);

		return slow_path(&obj, index, tid, node);
	}

	void reserve_slot(T* obj, int index, int tid, T* node){
		// fast path
		uint64_t prev_epoch = reservations[tid].slot.ui[index].pair[0].load(std::memory_order_acquire);
		size_t attempts = 16;
		do {
			uint64_t curr_epoch = getEpoch();
			if (curr_epoch == prev_epoch){
				return;
			} else {
				reservations[tid].slot.ui[index].pair[0].store(curr_epoch, std::memory_order_seq_cst);
				prev_epoch = curr_epoch;
			}
		} while (--attempts != 0);

		slow_path(nullptr, index, tid, node);
	}

	__attribute__((noinline)) T* slow_path(std::atomic<T*>* obj, int index, int tid, T* node)
	{
		// slow path
		uint64_t prev_epoch = reservations[tid].slot.ui[index].pair[0].load(std::memory_order_acquire);
		counter_start.ui.fetch_add(1, std::memory_order_acq_rel);
		reservations[tid].state.ui[index].pointer.store((uint64_t) obj, std::memory_order_release);
		uint64_t birth_epoch = (node == nullptr) ? 0 :
			*(uint64_t*)((char*)node + sizeof(T));
		reservations[tid].state.ui[index].epoch.store(birth_epoch, std::memory_order_release);
		uint64_t seqno = reservations[tid].slot.ui[index].pair[1].load(std::memory_order_acquire);
		value_pair_t last_result;
		last_result.pair[0] = (uint64_t) -1LL;
		last_result.pair[1] = seqno;
		dcas_store(reservations[tid].state.ui[index].result.full, last_result.full, std::memory_order_release);

		uint64_t result_epoch, result_ptr;
		do {
			value_pair_t value, last_epoch;
			T* ptr = obj ? obj->load(std::memory_order_acquire) : nullptr;
			uint64_t curr_epoch = getEpoch();
			if (curr_epoch == prev_epoch) {
				last_result.pair[0] = (uint64_t) -1LL;
				last_result.pair[1] = seqno;
				value.pair[0] = 0;
				value.pair[1] = 0;
				if (dcas_compare_exchange_strong(reservations[tid].state.ui[index].result.full, last_result.full, value.full, std::memory_order_acq_rel, std::memory_order_acquire)) {
					reservations[tid].slot.ui[index].pair[1].store(seqno + 1, std::memory_order_release);
					counter_end.ui.fetch_add(1, std::memory_order_acq_rel);
					return ptr;
				}
			}
			last_epoch.pair[0] = prev_epoch;
			last_epoch.pair[1] = seqno;
			value.pair[0] = curr_epoch;
			value.pair[1] = seqno;
			dcas_compare_exchange_strong(reservations[tid].slot.ui[index].full, last_epoch.full, value.full, std::memory_order_seq_cst, std::memory_order_acquire);
			prev_epoch = curr_epoch;
			result_ptr = reservations[tid].state.ui[index].result.pair[0].load(std::memory_order_acquire);
		} while (result_ptr == (uint64_t) -1LL);

		result_epoch = reservations[tid].state.ui[index].result.pair[1].load(std::memory_order_acquire);
		reservations[tid].slot.ui[index].pair[0].store(result_epoch, std::memory_order_release);
		reservations[tid].slot.ui[index].pair[1].store(seqno + 1, std::memory_order_release);
		counter_end.ui.fetch_add(1, std::memory_order_acq_rel);

		return (T*) result_ptr;
	}

	void clear(int tid){
		for (int i = 0; i < he_num; i++){
			reservations[tid].slot.ui[i].pair[0].store(0, std::memory_order_seq_cst);
		}
	}

	inline void incrementEpoch(){
		epoch.ui.fetch_add(1, std::memory_order_acq_rel);
	}
	
	void retire(T* obj, int tid){
		if(obj==NULL){return;}
		std::list<WFEInfo>* myTrash = &(retired[tid].ui);

		uint64_t retire_epoch = epoch.ui.load(std::memory_order_acquire);
		uint64_t birth_epoch = *(uint64_t*)((char*)obj + sizeof(T));
		myTrash->push_back(WFEInfo(obj, birth_epoch, retire_epoch));
		if(collect && retire_counters[tid]%freq==0){
			if(epoch.ui.load(std::memory_order_acquire) == retire_epoch) {
				// help other threads first
				help_read(tid);
				// only after that increment the counter
				epoch.ui.fetch_add(1,std::memory_order_acq_rel);
			}
			empty(tid);
		}
		retire_counters[tid]=retire_counters[tid]+1;
	}
	
	bool can_delete(uint64_t birth_epoch, uint64_t retire_epoch, int js, int je){
		for (int i = 0; i < task_num; i++){
			for (int j = js; j < je; j++){
				const uint64_t epo = reservations[i].slot.ui[j].pair[0].load(std::memory_order_acquire);
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
		std::list<WFEInfo>* myTrash = &(retired[tid].ui);
		for (auto iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ) {
			WFEInfo res = *iterator;
			uint64_t ce = counter_end.ui.load(std::memory_order_acquire);
			if (res.obj->deletable() && can_delete(res.birth_epoch, res.retire_epoch, 0, he_num) && can_delete(res.birth_epoch, res.retire_epoch, he_num, he_num+1)) {
				uint64_t cs = counter_start.ui.load(std::memory_order_acquire);
				if (ce == cs || (can_delete(res.birth_epoch, res.retire_epoch, he_num+1, he_num+2) && can_delete(res.birth_epoch, res.retire_epoch, 0, he_num))) {
					reclaim(res.obj);
					iterator = myTrash->erase(iterator);
					this->dec_retired(tid);
				}
			}
			else{++iterator;}
		}
	}

	bool collecting(){return collect;}
	
};

#endif
