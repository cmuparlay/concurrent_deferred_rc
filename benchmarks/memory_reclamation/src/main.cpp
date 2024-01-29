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


#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Harness.hpp"
#include "CustomTests.hpp"

#include "rideables/NatarajanTree.hpp"
#include "rideables/NatarajanTreeRC.hpp"
#include "rideables/NatarajanTreeRCSS.hpp"
#include "rideables/LinkList.hpp"
#include "rideables/LinkListRC.hpp"
#include "rideables/LinkListRCSS.hpp"

#include "rideables/SortedUnorderedMap.hpp"
#include "rideables/SortedUnorderedMapRC.hpp"
#include "rideables/SortedUnorderedMapRCSS.hpp"

#include <cdrc/internal/smr/acquire_retire.h>
#include <cdrc/internal/smr/acquire_retire_ibr.h>
#include <cdrc/internal/smr/acquire_retire_ebr.h>
#include <cdrc/internal/smr/acquire_retire_hyaline.h>

using namespace std;

GlobalTestConfig* gtc;

template<template<class, class, template<typename> typename, typename> typename FactoryType>
void addRideableOptions(GlobalTestConfig* testConfig, const std::string ds_name) {
	testConfig->addRideableOption(new FactoryType<int,int,cdrc::internal::acquire_retire,cdrc::empty_guard>, (ds_name + "RCHP").c_str());
	testConfig->addRideableOption(new FactoryType<int,int,cdrc::internal::acquire_retire_ebr,cdrc::epoch_guard>, (ds_name + "RCEBR").c_str());
	testConfig->addRideableOption(new FactoryType<int,int,cdrc::internal::acquire_retire_ibr,cdrc::epoch_guard>, (ds_name + "RCIBR").c_str());
	testConfig->addRideableOption(new FactoryType<int,int,cdrc::internal::acquire_retire_hyaline, cdrc::hyaline_guard>, (ds_name + "RCHyaline").c_str());
}

// the main function
// sets up output and tests
int main(int argc, char *argv[])
{
	gtc = new GlobalTestConfig();

	// additional rideables go here
	gtc->addRideableOption(nullptr, "Unused");
	gtc->addRideableOption(new SortedUnorderedMapFactory<int,int>(), "SortedUnorderedMap");
	gtc->addRideableOption(new SortedUnorderedMapRCFactory<int,int>(), "SortedUnorderedMapRC");
	addRideableOptions<SortedUnorderedMapRCSSFactory>(gtc, "SortedUnorderedMap");

	gtc->addRideableOption(new LinkListFactory<int,int>(), "LinkList");
	gtc->addRideableOption(new LinkListRCFactory<int,int>(), "LinkedListRC");
	addRideableOptions<LinkListRCSSFactory>(gtc, "LinkList");

	gtc->addRideableOption(new NatarajanTreeFactory<int,int>(), "NatarajanTree");
  	gtc->addRideableOption(new NatarajanTreeRCFactory<int,int>(), "NatarajanTreeRC");
  	addRideableOptions<NatarajanTreeRCSSFactory>(gtc, "NatarajanTree");

	gtc->addTestOption(new SequentialRemoveTest(4096), "SequentialRemoveTest:prefill=20K");
	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,200,100), "ObjRetire:u50:range=200:prefill=100");
  	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,2000,1000), "ObjRetire:u50:range=2000:prefill=1000");
  	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,200000,100000), "ObjRetire:u50:range=200K:prefill=100K");
	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,2000000,1000000), "ObjRetire:u50:range=2M:prefill=1M");
	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,20000000,10000000), "ObjRetire:u50:range=20M:prefill=10M");

	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,200,100), "ObjRetire:u10:range=200:prefill=100");
  	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,2000,1000), "ObjRetire:u10:range=2000:prefill=1000");
  	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,200000,100000), "ObjRetire:u10:range=200K:prefill=100K");
	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,2000000,1000000), "ObjRetire:u10:range=2M:prefill=1M");
	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,20000000,10000000), "ObjRetire:u10:range=20M:prefill=10M");

	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,200,100), "ObjRetire:u1:range=200:prefill=100");
  	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,2000,1000), "ObjRetire:u1:range=2000:prefill=1000");
  	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,200000,100000), "ObjRetire:u1:range=200K:prefill=100K");
	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,2000000,1000000), "ObjRetire:u1:range=2M:prefill=1M");
	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,20000000,10000000), "ObjRetire:u1:range=20M:prefill=10M");

	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,8192,4096), "ObjRetire:u50:range=8192:prefill=4096");
	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,8192,4096), "ObjRetire:u10:range=8192:prefill=4096");
	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,8192,4096), "ObjRetire:u1:range=8192:prefill=4096");

	gtc->addTestOption(new ObjRetireTest<int>(50,50,0,0,200000000,100000000), "ObjRetire:u50:range=200M:prefill=100M");
	gtc->addTestOption(new ObjRetireTest<int>(90,10,0,0,200000000,100000000), "ObjRetire:u10:range=200M:prefill=100M");
	gtc->addTestOption(new ObjRetireTest<int>(99,1,0,0,200000000,100000000), "ObjRetire:u1:range=200M:prefill=100M");

  	gtc->addTestOption(new SequentialRemoveTest(0), "SequentialRemoveTest:prefill=0");
  	gtc->addTestOption(new SequentialRemoveTest(1), "SequentialRemoveTest:prefill=1");

  	gtc->addTestOption(new ObjRetireTest<int>(0,50,0,50,200000,100000), "ObjRetire:u50rq50:range=200K:prefill=100K");
	gtc->addTestOption(new ObjRetireTest<int>(0,50,0,50,2000000,1000000), "ObjRetire:u50rq50:range=2M:prefill=1M");
	gtc->addTestOption(new ObjRetireTest<int>(0,50,0,50,20000000,10000000), "ObjRetire:u50rq50:range=20M:prefill=10M");
	gtc->addTestOption(new ObjRetireTest<int>(0,50,0,50,200000000,100000000), "ObjRetire:u50rq50:range=200M:prefill=100M");

	// parse command line
	gtc->parseCommandLine(argc,argv);

	if(gtc->verbose){
		fprintf(stdout, "Testing:  %d threads for %lu seconds on %s with %s\n",
		  gtc->actual_task_num,gtc->interval,gtc->getTestName().c_str(),gtc->getRideableName().c_str());
	}

	// register fancy seg fault handler to get some
	// info in case of crash
	signal(SIGSEGV, faultHandler);

	// do the work....
	gtc->runTest();

	Recorder* r = gtc->recorder;
	long long retired_nodes = 0;
	
	try {
		retired_nodes = std::stod(r->globalFields["obj_retired"])/gtc->actual_task_num;
	} catch (const std::invalid_argument& ia) {}
	// std::cout << "Average allocated nodes: " << retired_nodes << std::endl;
		

	printf("Throughput: %f Mop/s\n",gtc->total_operations/gtc->interval/1000000.0);

	return 0;
}

