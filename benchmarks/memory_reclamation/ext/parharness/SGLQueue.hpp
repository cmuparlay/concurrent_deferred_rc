/*

Copyright 2015 University of Rochester

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



#ifndef SGL_QUEUE
#define SGL_QUEUE

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <string>
#include <atomic>
#include "RContainer.hpp"

class SGLQueue : public RQueue{

private:
	void lockAcquire(int32_t tid);
	void lockRelease(int32_t tid);

	std::list<int32_t>* q=NULL;
	std::atomic<int32_t> lk;


public:
	SGLQueue();
	~SGLQueue();
	SGLQueue(std::list<int32_t>* contents);

	int32_t dequeue(int tid);
	void enqueue(int32_t val,int tid);
	
};

class SGLQueueFactory : public RContainerFactory{
	SGLQueue* build(GlobalTestConfig* gtc){
		return new SGLQueue();
	}
};

#endif
