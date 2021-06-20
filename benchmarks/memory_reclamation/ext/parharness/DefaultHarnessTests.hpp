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



#ifndef DEFAULT_HARNESS_TESTS_HPP
#define DEFAULT_HARNESS_TESTS_HPP

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

#include "Harness.hpp"
#include "RAllocator.hpp"
#include "RMap.hpp"
#include "RContainer.hpp"
#include <vector>
#include <list>
#include <map>
#include <hwloc.h>

class InsertRemoveTest : public Test{
public:
	RContainer* q;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};


class NothingTest : public Test{
public:
	Rideable* r;
	void init(GlobalTestConfig* gtc){r = gtc->allocRideable();}
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){return 0;}
	void cleanup(GlobalTestConfig* gtc){}
};



class FAITest :  public Test{
public:
	unsigned long int fai_cntr;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};


class NearEmptyTest :  public Test{
public:
	RContainer* q;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

class AllocatorChurnTest :  public Test{
public:
	RAllocator* bp;
	std::vector<std::list<void*>*> leftovers;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

class SequentialTest : public Test{
public:

	virtual int execute(GlobalTestConfig* gtc)=0;
	virtual void init(GlobalTestConfig* gtc)=0;
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	virtual void cleanup(GlobalTestConfig* gtc)=0;

};

class SequentialUnitTest : public SequentialTest{
public:

	bool passed=true;


	virtual int execute(GlobalTestConfig* gtc)=0;
	virtual void init(GlobalTestConfig* gtc)=0;
	void cleanup(GlobalTestConfig* gtc);
	virtual void clean(GlobalTestConfig* gtc)=0;
	void verify(bool stmt);

};


class UIDGenerator{
	
	int taskNum;
	int tidBits;
	uint32_t inc;

public:
	UIDGenerator(int taskNum);
	uint32_t initial(int tid);
	uint32_t next(uint32_t prev, int tid);

	uint32_t count(uint32_t val);
	uint32_t id(uint32_t val);


};

class QueueVerificationTest : public Test{
public:
	RContainer* q;
	std::atomic<bool> passed;
	UIDGenerator* ug;

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

class StackVerificationTest : public Test{
public:
	RStack* q;
	std::atomic<bool> passed;
	std::atomic<bool> done;
	UIDGenerator* ug;
	pthread_barrier_t pthread_barrier;
	int opsPerPhase;
	std::atomic<int> phaseCount;


	void barrier();
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};


class MapUnmapTest : public Test{
public:
	RMap* q;
	UIDGenerator* ug;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

class MapVerificationTest : public Test{
public:
	RMap* q;
	std::atomic<bool> passed;
	UIDGenerator* ug;

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};


class TopologyReport : public Test{
public:
	hwloc_topology_t top;
	unsigned int topodepth;
	void init(GlobalTestConfig* gtc){
		hwloc_topology_init(&top);
		hwloc_topology_load(top);
		topodepth = hwloc_topology_get_depth(top);
		char buf[128];
		puts("Machine Topology");
		print_children(top, hwloc_get_root_obj(top), 0);
	}
	
// from:
// https://www.open-mpi.org/projects/hwloc/doc/v1.7/
	void print_children(hwloc_topology_t topology, hwloc_obj_t obj, int depth){
			char type[128];
			char attr[128];
			unsigned i;
			int osidx = obj->os_index;
			hwloc_obj_type_snprintf(type, sizeof(type), obj, 0);
			hwloc_obj_attr_snprintf(attr, sizeof(attr), obj, "#",0);
			printf("%*s%s", 2*depth, "", type);		
			if(osidx!=-1){printf("#%d", osidx);}
			if(attr[0]!='\0'){printf("(%s)", attr);}
			printf("\n");
			//remove below because the mthod is deprecated
			//hwloc_obj_snprintf(string, sizeof(string), topology, obj, "#", 0);
			//printf("%*s%s\n", 2*depth, "", string);			
			for (i = 0; i < obj->arity; i++) {
					print_children(topology, obj->children[i], depth + 1);
			}
	}
	
	
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
		char buf[128];
		hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
		hwloc_bitmap_zero(cpuset);
		if (0 == hwloc_get_cpubind(top,cpuset,HWLOC_CPUBIND_THREAD)){

			// we have to search because they don't have this method in the API.
			// I find this stupid.
			hwloc_obj_t ans = NULL;
			unsigned int depth = topodepth-1;
			for (unsigned int i = 0; i < hwloc_get_nbobjs_by_depth(top, depth) && ans==NULL; i++) {
				hwloc_obj_t obj = hwloc_get_obj_by_depth(top, depth, i);
				if(hwloc_bitmap_isequal(cpuset,obj->cpuset)){
					ans = obj;
				}
			}	
		
			if(ans!=NULL){
				hwloc_obj_type_snprintf (buf, sizeof(buf), ans, 0);
				int osidx = ans->os_index;
				printf("Thread %u @ %s#%d\n", ltc->tid, buf, osidx);
				//remove below because the method is deprecated
				//hwloc_obj_snprintf(buf, sizeof(buf), top, ans, "#", 0);
				//printf("Thread %u @ %s\n", ltc->tid, buf);
			}
			else{
				printf("Thread %u @ Lookup error!\n", ltc->tid);
			}
		}
		hwloc_bitmap_free(cpuset);
		return 0;
	}
	void cleanup(GlobalTestConfig* gtc){
		hwloc_topology_destroy(top);
	}
};


#endif
