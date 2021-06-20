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


#ifndef RETIREDMONITORABLE_HPP
#define RETIREDMONITORABLE_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

class RetiredMonitorable{
private:
	padded<std::atomic<uint64_t>>* retired_cnt;
	// padded<uint64_t>* total_alloc;
	// padded<uint64_t>* num_samples;
	// padded<uint64_t>* counter;
public:
	RetiredMonitorable(GlobalTestConfig* gtc){
		if(gtc == nullptr) {
			retired_cnt = new padded<std::atomic<uint64_t>>[256];
		} else {
			retired_cnt = new padded<std::atomic<uint64_t>>[gtc->task_num];
			// total_alloc = new padded<uint64_t>[gtc->task_num];
			// num_samples = new padded<uint64_t>[gtc->task_num];
			// counter = new padded<uint64_t>[gtc->task_num];
			for(int i = 0; i < gtc->task_num; i++) {
				retired_cnt[i].ui = 0;
				// total_alloc[i] = 0;
				// num_samples[i] = 0;
				// counter[i] = 0;
			}
		}
	}
	~RetiredMonitorable() {
		if(retired_cnt != nullptr) delete[] retired_cnt;
	}
	void collect_retired_size(uint64_t size, int tid){
		retired_cnt[tid].ui = size;
	}
	uint64_t report_retired(int tid){
		return retired_cnt[tid].ui;
	}

	// void count_allocated_nodes(int tid) {
	// 	counter[tid]++;
	// 	if(counter[tid] % 1000 == 0) {

	// 	}
	// }

};

#endif