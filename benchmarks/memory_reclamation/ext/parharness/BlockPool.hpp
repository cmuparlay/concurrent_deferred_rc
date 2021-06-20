#ifndef BLOCK_POOL_HPP
#define BLOCK_POOL_HPP
 
#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <malloc.h>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

//////////////////////////////
//
//  Get memory from the OS directly

inline void* alloc_mmap(unsigned size){
    void* mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                     -1, 0);
    assert(mem != MAP_FAILED);
    return mem;
}

inline void free_mmap(void*){
    // NB: Should do something here, but we don't have a size to munmap with.
    std::cerr << "Ignoring a request to munmap.\n";
}

//////////////////////////////
//
//  Block Pool


template<typename T>
class BlockPool : public RAllocator
{
    struct shared_block_t
    {
        struct shared_block_t* volatile next;
        struct shared_block_t* volatile next_group;
        volatile T payload;
    };

    struct block_head_node_t
    {
        shared_block_t* volatile top;    // top of stack
        shared_block_t* volatile nth;    // ptr to GROUP_SIZE+1-th node, if
                                         // any, counting up from the bottom of
                                         // the stack
        volatile unsigned long count;    // number of nodes in list
    } __attribute__((aligned(LEVEL1_DCACHE_LINESIZE)));

	// flag to switch to regular glibc memory management
	bool glibc_mem;


	// number of threads
	int num_threads;

    // add and remove blocks from/to global pool in clumps of this size
    static const unsigned long GROUP_SIZE = 8;

    static const int blocksize = sizeof(shared_block_t);

    // [?] why don't we make this a static member, align it, make it volatile,
    // and then pass its address to CAS?
    // MLS: not safe if there's more than one block pool
    cptr<shared_block_t>* global_pool;

    block_head_node_t* head_nodes;  // one per thread

    // The following is your typical memory allocator hack.  Callers care about
    // payloads, which may be of different sizes in different pools.  Pointers
    // returned from alloc_block and passed to free_block point to the payload.
    // The bookkeeping data (next and next_group) are *in front of* the
    // payload.  We find them via pointer arithmetic.
    static inline shared_block_t* make_shared_block_t(void* block)
    {
        shared_block_t dum;
        return (shared_block_t*) (((unsigned char*)block) -
                                  ((unsigned char*)&(dum.payload) - (unsigned char*)&dum));
    }

public:

	// make sure nothing else is in the same line as this object
	static void* operator new(size_t size) { return alloc_mmap(size); }

	static void operator delete(void* ptr) { return free_mmap(ptr); }

    //  Create and return a pool to hold blocks of a specified size, to be shared
    //  by a specified number of threads.  Return value is an opaque pointer.  This
    //  routine must be called by one thread only.
    //
    BlockPool<T>(int _numthreads, bool _glibc_mem){
		glibc_mem = _glibc_mem;
		num_threads = _numthreads;
		if(glibc_mem){
			//puts("glibc");
			return;
		}

		// get memory for the head nodes
		head_nodes = (block_head_node_t*)memalign(LEVEL1_DCACHE_LINESIZE, _numthreads * sizeof(block_head_node_t));
		// get memory for the global pool
		global_pool = (cptr<shared_block_t>*)memalign(LEVEL1_DCACHE_LINESIZE, LEVEL1_DCACHE_LINESIZE);


		// make sure the allocations worked
		assert(head_nodes != 0 && global_pool != 0);
		memset (head_nodes,0,_numthreads * sizeof(block_head_node_t));
		memset (global_pool,0,LEVEL1_DCACHE_LINESIZE);

		// zero out the global pool
		global_pool->init(NULL,0);

		// configure the head nodes
		for (int i = 0; i < _numthreads; i++) {
			block_head_node_t* hn = &head_nodes[i];
			hn->top = hn->nth = 0;
			hn->count = 0;
		}
    }

	// NOTE doesn't automatically pad allocations anymore
	void appendBlockGroup(block_head_node_t* hn){
		shared_block_t* array = (shared_block_t*)memalign(LEVEL1_DCACHE_LINESIZE, blocksize*GROUP_SIZE);
		assert(array);
		memset (array,0,blocksize*GROUP_SIZE);
		hn->top = &array[0];
		hn->nth = hn->top;
		hn->count = GROUP_SIZE;
		array[GROUP_SIZE-1].next=NULL;
		for(int i = 0; i<GROUP_SIZE-1;i++){
			array[i].next=&array[i+1];
		}
	} 


    T* alloc(int tid){

		if(glibc_mem){
			return (T*)memalign(sizeof(T),1<<((int)log2(sizeof(T)))+2);
		}
        block_head_node_t* hn = &head_nodes[tid];
        shared_block_t* b = hn->top;
        if (b) {
            hn->top = b->next;
            hn->count--;
            if (b == hn->nth)
                hn->nth = 0;
        }
        else {
            // local pool is empty
            while (true) {
                cptr_local<shared_block_t> oldp;
                oldp.init(global_pool->all());
                if ((b = (shared_block_t*)oldp.ptr())) {
                    if (global_pool->CAS(oldp,b->next_group)) {
                        // successfully grabbed group from global pool
                        hn->top = b->next;
                        hn->count = GROUP_SIZE-1;
                        break;
                    }
                    // else somebody else got into timing window; try again
                }
					else{
						// global pool is empty
						//b = (shared_block_t*)memalign(LEVEL1_DCACHE_LINESIZE, blocksize);
						appendBlockGroup(hn);
					 	b = hn->top;
				      hn->top = b->next;
				      hn->count--;
				      if (b == hn->nth){hn->nth = 0;}
						assert(b != 0);
						break;
					}
            }
            // In real-time code I might want to limit the number of iterations of
            // the above loop, and go ahead and malloc a new node when there is
            // very heavy contention for the global pool.  In practice I don't
            // expect a starvation problem.  Note in particular that the code as
            // written is preemption safe.
        }
        return (T*)&b->payload;
    }

    void free(T* block, int tid){
		if(glibc_mem){
			::free(block);
			return;
		}
        block_head_node_t* hn = &head_nodes[tid];
        shared_block_t* b = make_shared_block_t(block);

        b->next = hn->top;
        hn->top = b;
        hn->count++;
        if (hn->count == GROUP_SIZE+1) {
            hn->nth = hn->top;
        }
        else if (hn->count == GROUP_SIZE * 2) {
            // got a lot of nodes; move some to global pool
            shared_block_t* ng = hn->nth->next;
            while (true) {
                cptr_local<shared_block_t> oldp;
                oldp.init(global_pool->all());
                ng->next_group = (shared_block_t*)oldp.ptr();
                if (global_pool->CAS(oldp, ng)) break;
                // else somebody else got into timing window; try again
            }
            // In real-time code I might want to limit the number of
            // iterations of the above loop, and let my local pool grow
            // bigger when there is very heavy contention for the global
            // pool.  In practice I don't expect a problem.  Note in
            // particular that the code as written is preemption safe.
            hn->nth->next = 0;
            hn->nth = 0;
            hn->count -= GROUP_SIZE;
        }
    }

	// for Rideable interface
	void* allocBlock(int tid){
		return alloc(tid);
	}
	void freeBlock(void* ptr,int tid){
		free((T*)ptr, tid);
	}
	BlockPool<T>* clone(){
		return new BlockPool<T>(num_threads,glibc_mem);
	}

	void preheat(int quantity){
		// preheat block pools
		std::list<T*> v;
		for(int i=0;i<num_threads;i++){
			for(int j=0; j<quantity; j++){
				v.push_back(alloc(i));
			}
		}
		for(int i=0;i<num_threads;i++){
			for(int j=0; j<quantity; j++){
				free(v.front(),i);
				v.pop_front();
			}
		}
	}


};

template<typename T>
class BlockPoolFactory : public RideableFactory{
	BlockPool<T>* build(GlobalTestConfig* gtc){
		return new BlockPool<T>(gtc->task_num, gtc->environment["glibc"]=="1");
	}
};

#endif
