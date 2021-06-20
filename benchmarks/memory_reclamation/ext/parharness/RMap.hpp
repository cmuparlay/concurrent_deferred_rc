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



#ifndef RMAP_HPP
#define RMAP_HPP

#include <assert.h>
#include <stddef.h>
#include <atomic>
#include "Rideable.hpp"
#include "ConcurrentPrimitives.hpp"


#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

class RMap : public virtual Rideable{
public:
	virtual bool map(int32_t key, int32_t val, int tid)=0;
	//virtual int32_t search(int32_t key,int tid)=0;
	virtual int32_t unmap(int32_t key,int tid)=0;

	virtual int32_t get(int32_t key, int tid)=0;

};

#endif
