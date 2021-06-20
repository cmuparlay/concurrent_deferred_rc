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




#include "DefaultHarnessTests.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <vector>
#include <set>
#include <iterator>
#include <climits>

using namespace std;

// EnqDeqTest methods
void InsertRemoveTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RContainer*>(ptr);
	if (!q) {
		 errexit("InsertRemoveTest must be run on RContainer type object.");
	}

	if(gtc->checkEnv("prefill")){
		int prefill = atoi(gtc->getEnv("prefill").c_str());
		for(int i = 0; i<prefill; i++){
			q->insert(i+1,0);
		}
	}
/*
	gtc->recorder->addThreadField("insOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOpsEmpty_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOpsEmpty_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOpsEmpty_each",&Recorder::concat);
*/


}

int InsertRemoveTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int insOps = 0;
	int remOps = 0;
	int remOpsEmpty =0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->insert(ops+1,tid);
			insOps++;
		}
		else{
			if(q->remove(tid)==EMPTY){remOpsEmpty++;}
			remOps++;
		}
		ops++;
		gettimeofday(&now,NULL);
	}

/*
	gtc->recorder->reportThreadInfo("insOps_total",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_stddev",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_each",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_total",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_stddev",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_each",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_total",remOpsEmpty,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_stddev",remOpsEmpty,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_each",remOpsEmpty,ltc->tid);
*/

	return ops;
}

void InsertRemoveTest::cleanup(GlobalTestConfig* gtc){

}

// FAITest methods
void FAITest::init(GlobalTestConfig* gtc){
	fai_cntr = 0;
}
int FAITest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday (&now,NULL);
	int ops = 0;
	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		__sync_fetch_and_add(&(fai_cntr),1);
		ops++;
		gettimeofday (&now,NULL);
	}
	return ops;
}



// NearEmptyTest methods
void NearEmptyTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RContainer*>(ptr);
	if (!q) {
		 errexit("NearEmptyTest must be run on RContainer type object.");
	}
}
int NearEmptyTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int tid = ltc->tid;


	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){

		if(ops%200==0){
			q->insert(ops,tid);
		}
		else{
			q->remove(tid);
		}
		ops++;
		gettimeofday (&now,NULL);
	
	}
	return ops;
}


// AllocatorChurnTest
void AllocatorChurnTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->bp = dynamic_cast<RAllocator*>(ptr);
	if (!bp) {
		 errexit("AllocatorChurnTest must be run on RAllocator type object.");
	}
	this->leftovers.resize(gtc->task_num);
}

int AllocatorChurnTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int tid = ltc->tid;
	list<void*>* allocated = new list<void*>();
	unsigned int r = ltc->seed;

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2!=0){
			void* ptr = bp->allocBlock(tid);
			allocated->push_back(ptr);
		}
		else if(allocated->empty()==false){
			if(r%4==1){
				void* ptr = allocated->back();
				allocated->pop_back();
				bp->freeBlock(ptr,tid);
			}
			else{
				void* ptr = allocated->front();
				allocated->pop_front();
				bp->freeBlock(ptr,tid);
			}
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	leftovers[tid]=allocated;

	return ops;
}

void AllocatorChurnTest::cleanup(GlobalTestConfig* gtc){

	if(!gtc->checkEnv("VERIFY")){
		return;
	}

	bool is_in = false;
	bool passed = true;
	set<void*> s;
	for(int i = 0; i < leftovers.size(); i++){
		for (std::list<void*>::const_iterator iterator = leftovers[i]->begin(), end = leftovers[i]->end(); iterator != end; ++iterator) {
			is_in = (s.find(*iterator) != s.end());
			if(!is_in){
				s.insert(*iterator);
			}
			else{
				cout<<"Verification failed, double allocation: "<<*iterator<<endl;
				passed = false;
			}
		}
	}
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}


int SequentialTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int tid = ltc->tid;
	if(tid==0){
		return execute(gtc);
	}
	else{
		cout<<"Running sequential test.  Extra threads ignored."<<endl;
		return 0;
	}
}

void SequentialUnitTest::verify(bool stmt){
	if(!stmt){
		passed = false;
	}
	assert(stmt);
}
void SequentialUnitTest::cleanup(GlobalTestConfig* gtc){
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
	this->clean(gtc);
}


// QueueVerificationTest methods
void QueueVerificationTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RQueue*>(ptr);
	if(!q){
		this->q = dynamic_cast<RPriorityQueue*>(ptr);
	}
	if (!q) {
		cout<<"QueueVerificationTest should be run on RQueue or RPriorityQueue type object."<<endl;
		cout<<"Trying anyway..."<<endl;	
		this->q = dynamic_cast<RContainer*>(ptr);	
	}
	if(!q){
		errexit("QueueVerificationTest must be run on RContainer type object.");
	}
	
	ug = new UIDGenerator(gtc->task_num);
	passed.store(1);
}

int QueueVerificationTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t inserting = ug->initial(tid);

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->insert(inserting,tid);
			inserting = ug->next(inserting,tid);
			if(inserting==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}

		}
		else{
			uint32_t removed = (uint32_t) q->remove(tid);
			if(removed==EMPTY){continue;}
			uint32_t id = ug->id(removed);
			uint32_t cnt = ug->count(removed);

			if(cnt<=found[id]){
				cout<<"Verification failed! Reordering violation."<<endl;
				cout<<"Im thread "<<tid<<endl;
				cout<<"Found "<<found[id]<<" for thread "<<id<<endl;
				cout<<"Putting "<<cnt<<endl;
				passed.store(0);	
				assert(false);
			}
			else{
				found[id]=cnt;
			}
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	return ops;
}


void QueueVerificationTest::cleanup(GlobalTestConfig* gtc){
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}


void StackVerificationTest::barrier()
{
	pthread_barrier_wait(&pthread_barrier);
}

void StackVerificationTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RStack*>(ptr);
	if (!q) {
		if(!q){
			errexit("StackTest must be run on RStack type object.");
		}
	}
	else{
		if(gtc->verbose){
			cout<<"Running PotatoTest on partial container."<<endl;
		}
	}
	gtc->recorder->addThreadField("insOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOps_each",&Recorder::concat);
	ug = new UIDGenerator(gtc->task_num);
	pthread_barrier_init(&pthread_barrier, NULL, gtc->task_num);
	
	opsPerPhase = 5000;
	phaseCount.store(0);
	done.store(false);
}

int StackVerificationTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;
	int phase = 0;

	vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t inserting = ug->initial(tid);
	bool empty = true;
	int count = opsPerPhase;

	while(true){
	
		// barrier
		if((phase%2==0 && empty) || (phase%2==1 && count>=opsPerPhase)){
			for(int i = 0; i<gtc->task_num; i++){
				found[i]=0;
			}
			barrier();
			phaseCount.fetch_add(count);
			barrier();
			if(tid==0){
				assert(phaseCount==gtc->task_num*opsPerPhase || phase==0); 
				cout<<"On phase:"<<phase<<" did "<<phaseCount<<endl;
				phaseCount.store(0);
				done = empty==true && !(now.tv_sec < time_up.tv_sec || (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec));
			}
			count = 0;
			phase++;
			barrier();
			if(done){break;}
			barrier();
		}

		if(phase%2==1){
			q->insert(inserting,tid);
			inserting = ug->next(inserting,tid);
			if(inserting==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}
			empty = false;
			count++;
		}
		else{
			uint32_t removed = (uint32_t) q->remove(tid);
			if(removed==EMPTY){empty=true; continue;}
			uint32_t id = ug->id(removed);
			uint32_t cnt = ug->count(removed);

			if(found[id]!=0 && cnt>=found[id]){
				cout<<"Verification failed! Reordering violation."<<endl;
				cout<<"Im thread "<<tid<<endl;
				cout<<"Found "<<found[id]<<" for thread "<<id<<endl;
				cout<<"Putting "<<cnt<<endl;
				passed.store(0);	
				assert(false);
			}
			else{
				found[id]=cnt;
			}
			count++;
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	cout<<"phases="<<phase<<endl;
	return ops;
}



void MapUnmapTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RMap*>(ptr);
	if (!q) {
		 errexit("MapUnmapTest must be run on RMap type object.");
	}
	ug = new UIDGenerator(gtc->task_num+1);
	
	if(gtc->checkEnv("prefill")){
		int prefill = atoi((gtc->getEnv("prefill")).c_str());
		int32_t insKey = ug->initial(gtc->task_num);
		for(int i = 0; i<prefill; i++){
			q->map(insKey,insKey,0);
			insKey=ug->next(insKey,gtc->task_num);
		}
	}
}

int MapUnmapTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	int32_t insKey = ug->initial(tid);
	int32_t remKey = ug->initial(tid);

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->map(insKey,insKey,tid);
			insKey=ug->next(insKey,tid);
			if(insKey==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}
		}
		else{
			if(q->unmap(remKey,tid)>0){
				remKey=ug->next(remKey,tid);
			}
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	return ops;
}



// MapVerificationTest methods
void MapVerificationTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RMap*>(ptr);
	if (!q) {
		 errexit("MapVerificationTest must be run on RMap type object.");
	}
	ug = new UIDGenerator(gtc->task_num);
	passed.store(1);
}

int MapVerificationTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t insKey = ug->initial(tid);
	uint32_t remKey = ug->initial((tid+1)%(gtc->task_num));

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->map(insKey,insKey,tid);
			insKey = ug->next(insKey,tid);
			if(insKey==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}
		}
		else{
			uint32_t removed = (uint32_t) q->unmap(remKey,tid);
			if(removed==EMPTY){continue;}
			assert(removed==remKey);
			uint32_t id = ug->id(removed);
			uint32_t cnt = ug->count(removed);

			if(cnt<=found[id]){
				cout<<"Verification failed! Reordering violation."<<endl;
				cout<<"Im thread "<<tid<<endl;
				cout<<"Found "<<found[id]<<" for thread "<<id<<endl;
				cout<<"Putting "<<cnt<<endl;
				passed.store(0);
			}
			found[id]=cnt;
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	return ops;
}


void MapVerificationTest::cleanup(GlobalTestConfig* gtc){
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}

UIDGenerator::UIDGenerator(int taskNum){
	this->taskNum=taskNum;
	tidBits = log2(taskNum)+1;
	inc = 1<<tidBits;
}
uint32_t UIDGenerator::initial(int tid){
	return tid+inc;
}
uint32_t UIDGenerator::next(uint32_t prev, int tid){
	uint32_t ret = prev+inc;
	if ((ret | (inc-1))==UINT_MAX){
		ret = 0;
	}
	return ret;
}
uint32_t UIDGenerator::count(uint32_t val){
	return val>>tidBits;	
}
uint32_t UIDGenerator::id(uint32_t val){
	return val%inc;
}









