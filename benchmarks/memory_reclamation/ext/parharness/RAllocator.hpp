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



#ifndef RALLOCATOR_HPP
#define RALLOCATOR_HPP

#include <string>
#include "Rideable.hpp"

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

class RAllocator : public Rideable{
public:
	// dequeues item from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual void* allocBlock(int tid)=0;

	// enqueues val into queue.
	// tid: Thread id, unique across all threads
	virtual void freeBlock(void* ptr,int tid)=0;
};
#endif
