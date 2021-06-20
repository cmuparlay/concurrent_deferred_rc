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




#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Harness.hpp"

using namespace std;

GlobalTestConfig* gtc;

// the main function
// sets up output and tests
int main(int argc, char *argv[])
{

	gtc = new GlobalTestConfig();

	gtc->addRideableOption(new SGLQueueFactory(), "SGLQueue (default)");

	gtc->addTestOption(new InsertRemoveTest(), "InsertRemove Test");
	gtc->addTestOption(new NearEmptyTest(), "NearEmpty Test");
	gtc->addTestOption(new FAITest(), "FAI Test");

	
	try{
		gtc->parseCommandLine(argc,argv);
	}
	catch(...){
		return 0;
	}

	if(gtc->verbose){
		fprintf(stdout, "Testing:  %d threads for %lu seconds on %s\n",
		  gtc->task_num,gtc->interval,gtc->getTestName().c_str());
	}

	// register fancy seg fault handler to get some
	// info in case of crash
	signal(SIGSEGV, faultHandler);

	// do the work....
	try{
		gtc->runTest();
	}
	catch(...){
		return 0;
	}


	// print out results
	if(gtc->verbose){
		printf("Operations/sec: %ld\n",gtc->total_operations/gtc->interval);
	}
	else{
		printf("%ld \t",gtc->total_operations/gtc->interval);
	}

	return 0;
}

























