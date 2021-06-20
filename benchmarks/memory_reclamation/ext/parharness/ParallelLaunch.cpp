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



#include "ParallelLaunch.hpp"
#include "HarnessUtils.hpp"
#include <atomic>
#include <hwloc.h>


using namespace std;

// BARRIERS --------------------------------------------

// utility barrier function using pthreads barrier
// for timing the other primitives
pthread_barrier_t pthread_barrier;
void barrier()
{
	pthread_barrier_wait(&pthread_barrier);
}
void initSynchronizationPrimitives(int task_num){
	// create barrier
	pthread_barrier_init(&pthread_barrier, NULL, task_num);
}

// ALARM handler ------------------------------------------
// in case of infinite loop
bool testComplete;
void alarmhandler(int sig){
	if(testComplete==false){
		fprintf(stderr,"Time out error.\n");
		faultHandler(sig);
	}
}


// AFFINITY ----------------------------------------------

/*
void attachThreadToCore(int core_id) {
	pthread_t my_thread = pthread_self(); 
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);
   
	pthread_setaffinity_np(my_thread, sizeof(cpuset), &cpuset);
}
*/

void setAffinity(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int tid = ltc->tid;
	ltc->cpuset=gtc->affinities[tid]->cpuset;
	hwloc_set_cpubind(gtc->topology,ltc->cpuset,HWLOC_CPUBIND_THREAD);
	ltc->cpu=gtc->affinities[tid]->os_index;
}

// TEST EXECUTION ------------------------------
// Initializes any locks or barriers we need for the tests
void initTest(GlobalTestConfig* gtc){
	mlockall(MCL_CURRENT | MCL_FUTURE);
	mallopt(M_TRIM_THRESHOLD, -1);	
  	mallopt(M_MMAP_MAX, 0);
	gtc->test->init(gtc);
	for(int i = 0; i<gtc->allocatedRideables.size() && gtc->getEnv("report")=="1"; i++){
		if(Reportable* r = dynamic_cast<Reportable*>(gtc->allocatedRideables[i])){
			r->introduce();
		}
	}
}

// function to call the appropriate test
int executeTest(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int ops = gtc->test->execute(gtc,ltc);
	return ops;
}

// Cleans up test
void cleanupTest(GlobalTestConfig* gtc){
	for(int i = 0; i<gtc->allocatedRideables.size() && gtc->getEnv("report")=="1"; i++){
		if(Reportable* r = dynamic_cast<Reportable*>(gtc->allocatedRideables[i])){
			r->conclude();
		}
	}
	gtc->test->cleanup(gtc);
}


// THREAD MANIPULATION ---------------------------------------------------

// Thread manipulation from SOR sample code
// this is the thread main function.  All threads start here after creation
// and continue on to run the specified test
static void * thread_main (void *lp)
{
	atomic_thread_fence(std::memory_order::memory_order_acq_rel);
	CombinedTestConfig* ctc = ((CombinedTestConfig *) lp);
	GlobalTestConfig* gtc = ctc->gtc;
	LocalTestConfig* ltc = ctc->ltc;
	int task_id = ltc->tid;
	setAffinity(gtc,ltc);

	barrier(); // barrier all threads before setting times

	if(task_id==0){
        	gettimeofday (&gtc->start, NULL);
        	gtc->finish=gtc->start;
			gtc->finish.tv_sec+=gtc->interval;
	}



	barrier(); // barrier all threads before starting

	/* ------- WE WILL DO ALL OF THE WORK!!! ---------*/
	int ops = executeTest(gtc,ltc);

	if(ltc->tid < gtc->actual_task_num) {
		// record standard statistics
		__sync_fetch_and_add (&gtc->total_operations, ops);
		gtc->recorder->reportThreadInfo("ops",ops,ltc->tid);
		gtc->recorder->reportThreadInfo("ops_stddev",ops,ltc->tid);
		gtc->recorder->reportThreadInfo("ops_each",ops,ltc->tid);
	}

	barrier(); // barrier all threads at end

	return NULL;
}


// This function creates our threads and sets them loose
void parallelWork(GlobalTestConfig* gtc){

	pthread_attr_t attr;
	pthread_t *threads;
	CombinedTestConfig* ctcs;
	int i;
	// int task_num = gtc->task_num;
	int actual_task_num = gtc->actual_task_num+1;

	// init globals
	initSynchronizationPrimitives(actual_task_num);
	initTest(gtc);
	testComplete = false;

	// initialize threads and arguments ----------------
	ctcs = (CombinedTestConfig *) malloc (sizeof (CombinedTestConfig) * actual_task_num);
	threads = (pthread_t *) malloc (sizeof (pthread_t) * actual_task_num);
	if (!ctcs || !threads){ errexit ("out of shared memory"); }
	pthread_attr_init (&attr);
	pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
	//pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 1024*1024);
	srand((unsigned) time(NULL));
	for (i = 0; i < actual_task_num; i++) {
		ctcs[i].gtc = gtc;
		ctcs[i].ltc = new LocalTestConfig();
		ctcs[i].ltc->tid=i;
		ctcs[i].ltc->seed = rand();
	}

	// signal(SIGALRM, &alarmhandler);  // set a signal handler
	// if(gtc->timeOut){
	// 	alarm(gtc->interval+30);  // set an alarm for interval+10 seconds from now
	// }

	atomic_thread_fence(std::memory_order::memory_order_acq_rel);

	// launch threads -------------
	for (i = 1; i < actual_task_num; i++) {
		pthread_create (&threads[i], &attr, thread_main, &ctcs[i]);
	}

	struct timeval start;
	struct timeval end;
	gettimeofday(&start,NULL);
	//pthread_key_create(&thread_id_ptr, NULL);
	thread_main(&ctcs[0]); // start working also
	gettimeofday(&end,NULL);
	cout << "Elapsed time: " << timeDiff(&start, &end)/1000000.0 << " seconds" << endl;
	// All threads working here... ( in thread_main() )
	

	// join threads ------------------
	for (i = 1; i < actual_task_num; i++)
    	pthread_join (threads[i], NULL);

	for (i = 0; i < actual_task_num; i++) {
		delete ctcs[i].ltc;
	}


	testComplete = true;
	free(ctcs);
	free(threads);
	cleanupTest(gtc);
}
































