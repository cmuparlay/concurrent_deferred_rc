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


#include "CustomTests.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <vector>
#include <set>
#include <iterator>
#include <climits>

using namespace std;

void SequentialRemoveTest::init(GlobalTestConfig* gtc) {
	Rideable* ptr = gtc->allocRideable();
	if (!dynamic_cast<RetiredMonitorable*>(ptr)){
		errexit("ObjRetireTest must be run on RetiredMonitorable type object.");
	}
	this->m = dynamic_cast<RUnorderedMap<int,int>*>(ptr);
	if (!m) {
		 errexit("ObjRetireTest must be run on RUnorderedMap<T,T> type object.");
	}

	if(gtc->checkEnv("prefill")){
		prefill = atoi((gtc->getEnv("prefill")).c_str());
	}

	int i;
	for(i = 0; i<prefill; i++)
		m->insert(i,i,0);
	if(gtc->verbose)
		printf("Prefilled %d\n",i);
}

int SequentialRemoveTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc) {
	int tid = ltc->tid;
	int ops = 0;

	while(remove_next < prefill) {
		int k = remove_next.ui.fetch_add(1);
		if(k >= prefill) break;
		m->remove(k, tid);
		ops++;
	}
	return ops;
}

void DebugTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->m = dynamic_cast<RUnorderedMap<string, string>*>(ptr);
	if (!m) {
		 errexit("DebugTest must be run on RUnorderedMap type object.");
	}
	if (gtc->task_num > 1){
		errexit("DebugTest only support single thread.");
	}

	cout<<"Hello from DebugTest::init"<<endl;
}
void DebugTest::parInit(GlobalTestConfig* gtc, LocalTestConfig* ltc){

}
void DebugTest::get(string key, int tid){
	if (!m->get(key, tid).has_value()){
		cout<<"key "<<key<<" DNE"<<endl;
		return;
	}
	cout<<"get '"<<key<<"':'"<<m->get(key, tid).value()<<"'"<<endl;
}
void DebugTest::put_get(string key, string value, int tid){
	cout<<"put<'"<<key<<"','"<<value<<"'>"<<endl;
	m->put(key, value, tid);
	cout<<"get '"<<key<<"':'"<<m->get(key, tid).value()<<"'"<<endl;
}
void DebugTest::remove_get(string key, int tid){
	cout<<"remove'"<<key<<"'"<<endl;
	m->remove(key, tid);
	cout<<"get '"<<key<<"':'"<<m->get(key, tid).value()<<"'"<<endl;
}
int DebugTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int tid = ltc->tid;
	
	put_get("b", "b", tid);
	put_get("c", "c", tid);
	m->remove("c", tid);
	get("c", tid);
	
	return 0;
}





