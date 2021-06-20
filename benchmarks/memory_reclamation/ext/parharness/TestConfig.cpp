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



#include "TestConfig.hpp"
#include "ParallelLaunch.hpp"
#include "DefaultHarnessTests.hpp"
#include "RContainer.hpp"
#include "SGLQueue.hpp"

#include <sys/time.h>
#include <sys/resource.h>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

Rideable* GlobalTestConfig::allocRideable(){
	Rideable* r = rideableFactories[rideableType]->build(this);
	allocatedRideables.push_back(r);
	return r;
}

void GlobalTestConfig::printargdef(){
	int i;
	fprintf(stderr, "usage: %s [-m <test_mode>] [-r <rideable_test_object>] [-a single|dfs|default] [-i <interval>] [-t <num_threads>] [-o <output_csv_file>] [-w <warm_up_MBs>] [-d <env_variable>=<value>] [-z] [-v] [-h]\n", argv0);
	for(i = 0; i< rideableFactories.size(); i++){
		fprintf(stderr, "Rideable %d : %s\n",i,rideableNames[i].c_str());
	}
	for(i = 0; i< tests.size(); i++){
		fprintf(stderr, "Test Mode %d : %s\n",i,testNames[i].c_str());
	}
	exit(0);
}

void GlobalTestConfig::parseCommandLine(int argc, char** argv){
	int c;
	argv0 = argv[0];

	// if no args, print help
	if(argc==1){
			printargdef();
			errexit("");
	}

	if(tests.size()==0){
		errexit("No test options provided.  Use GlobalTestConfig::addTestOption() to add.");
	}


	// Read command line
	while ((c = getopt (argc, argv, "d:w:o:i:t:m:a:r:vhz")) != -1){
		switch (c) {
			case 'i':
				this->interval = atoi(optarg);
			 	break;
			case 'v':
			 	this->verbose = 1;
			 	break;
			case 'w':
				this->warmup = atoi(optarg);
			 	break;
			case 't':
				this->task_num = std::max(atoi(optarg), 32);
				this->actual_task_num = atoi(optarg);
				break;
			case 'm':
				this->testType = atoi(optarg);
				if(testType>=tests.size()){
					fprintf(stderr, "Invalid test mode (-m) option.\n");
					printargdef();
				}
				break;
			case 'r':
				this->rideableType = atoi(optarg);
				if(rideableType>=rideableFactories.size()){
					fprintf(stderr, "Invalid rideable (-r) option.\n");
					printargdef();
				}
				break;
			case 'a':
				this->affinity = std::string(optarg);
				break;
			case 'h':
				printargdef();
				break;
			case 'o':
				this->outFile = std::string(optarg);
				break;
			case 'z':
				this->timeOut = false;
				break;
			case 'd':
				string s = std::string(optarg);
				string k,v;
				std::string::size_type pos = s.find('=');
				if (pos != std::string::npos){
					k = s.substr(0, pos);
					v = s.substr(pos+1, std::string::npos);
				}
				else{
				  k = s; v = "1";
				}
				if(v=="true"){v="1";}
				if(v=="false"){v="0";}
				environment[k]=v;
				break;
	     	}			
	}

	test = tests[testType];
	
	/*
	if(affinityFile.size()==0){
		affinityFile = "";
		affinityFile += "./ext/parharness/affinity/"+machineName()+".aff";
	}
	readAffinity();*/
	hwloc_topology_init(&topology);
	hwloc_topology_load(topology);
	num_procs = hwloc_get_nbobjs_by_depth(topology,  
	 hwloc_get_type_depth(topology, HWLOC_OBJ_PU));
	buildAffinity();


	recorder = new Recorder(task_num);
	recorder->reportGlobalInfo("datetime",Recorder::dateTimeString());
	recorder->reportGlobalInfo("threads",task_num);
	recorder->reportGlobalInfo("cores",num_procs);
	recorder->reportGlobalInfo("rideable",getRideableName());
	recorder->reportGlobalInfo("affinity",affinity);
	recorder->reportGlobalInfo("test",getTestName());
	recorder->reportGlobalInfo("interval",interval);
	recorder->reportGlobalInfo("language","C++");
	recorder->reportGlobalInfo("machine",machineName());
	recorder->reportGlobalInfo("archbits",archBits());
	recorder->reportGlobalInfo("preheated(MBs)",warmup);
	recorder->reportGlobalInfo("notes","");
	recorder->addThreadField("ops",&Recorder::sumInts);
	recorder->addThreadField("ops_stddev",&Recorder::stdDevInts);
	recorder->addThreadField("ops_each",&Recorder::concat);


	string env ="";
	for(auto it = environment.cbegin(); it != environment.cend(); ++it){
		 env = env+ it->first + "=" + it->second + ":";
	}
	if(env.size()>0){env.pop_back();}
	recorder->reportGlobalInfo("environment",env);

	if(verbose && environment.size()>0){
		cout<<"Using flags:"<<endl;
		for(auto it = environment.cbegin(); it != environment.cend(); ++it){
			 std::cout << it->first << " = \"" << it->second << "\"\n";
		}
	}
}

// based on
// https://www.open-mpi.org/projects/hwloc/doc/v1.7/
void GlobalTestConfig::buildDFSAffinity_helper(hwloc_obj_t obj){
	if(obj->type==HWLOC_OBJ_PU){
		affinities.push_back(obj);
		return;
	}
	if(affinities.size()>=task_num){return;}
	for (unsigned i = 0; i < obj->arity; i++) {
			buildDFSAffinity_helper(obj->children[i]);
	}
}

void GlobalTestConfig::buildDFSAffinity(){
	buildDFSAffinity_helper(hwloc_get_root_obj(topology));
}

int GlobalTestConfig::buildDefaultAffinity_findCoresInSocket(hwloc_obj_t obj, vector<hwloc_obj_t>* cores){
	if(obj->type==HWLOC_OBJ_CORE){
		cores->push_back(obj);
		return 1;
	}
	if(obj->type==HWLOC_OBJ_PU){
		return 0; // error: we shouldn't reach PU's before cores...
	}
	int ret = 1;
	for (unsigned i = 0; i < obj->arity; i++) {
			ret = ret && buildDefaultAffinity_findCoresInSocket(obj->children[i],cores);
	}
	return ret;
}

int GlobalTestConfig::buildDefaultAffinity_buildPUsInCores(vector<hwloc_obj_t>* cores){
		int coreIndex = 0;
		int coresFilled = 0;
		while(coresFilled<cores->size()){
			for(int i = 0; i<cores->size(); i++){
				// so stride over cores, expecting that next level down are PUs
				if(coreIndex==cores->at(i)->arity){
						coresFilled++;
						continue;
				}
				hwloc_obj_t obj = cores->at(i)->children[coreIndex];
				if(obj->type!=HWLOC_OBJ_PU){return 0;}
				affinities.push_back(obj);
			}
			coreIndex++;
		}
		return 1;
}

// descend to socket level, then build each socket individually
int GlobalTestConfig::buildDefaultAffinity_findAndBuildSockets(hwloc_obj_t obj){
	// recursion terminates at sockets
	if(obj->type==HWLOC_OBJ_SOCKET){
		vector<hwloc_obj_t> cores;
		if(!buildDefaultAffinity_findCoresInSocket(obj,&cores)){
			return 0; // couldn't find cores in this socket, so flag error
		}
		// now "cores" is filled with all cores below the socket,
		// so assign threads to this core
		return buildDefaultAffinity_buildPUsInCores(&cores);
	}
	// recursion down by DFS
	int ret = 1;
	for (unsigned i = 0; i < obj->arity; i++) {
			ret = ret && buildDefaultAffinity_findAndBuildSockets(obj->children[i]);
	}
	return ret;
}

void GlobalTestConfig::buildDefaultAffinity(){
	if(!buildDefaultAffinity_findAndBuildSockets(hwloc_get_root_obj(topology))){
		fprintf(stderr, "Unsupported topology for default thread pinning (unable to detect sockets and cores).");
		fprintf(stderr, "Switching to depth first search affinity.\n");
		affinities.resize(0);
		buildDFSAffinity();	
	}
}

void GlobalTestConfig::buildSingleAffinity_helper(hwloc_obj_t obj){
	if(obj->type==HWLOC_OBJ_PU){
		for(int i =0; i<task_num; i++){
			affinities.push_back(obj);
		}
		return;
	}
	buildSingleAffinity_helper(obj->children[0]);
}

void GlobalTestConfig::buildSingleAffinity(){
	buildSingleAffinity_helper(hwloc_get_root_obj(topology));
}

// reference:
// https://www.open-mpi.org/projects/hwloc//doc/v1.2.2/hwloc_8h.php
void GlobalTestConfig::buildAffinity(){
	if(affinity.compare("dfs")==0){
		buildDFSAffinity();
	}
	else if(affinity.compare("single")==0){
		buildSingleAffinity();
	}
	else{
		buildDefaultAffinity();
	}
	if(affinities.size()<actual_task_num+1){
		affinities.resize(actual_task_num+1);
	}
	for(int i=num_procs; i<actual_task_num+1; i++){
		affinities[i] = affinities[i%num_procs];
	}
}



void GlobalTestConfig::setEnv(std::string key, std::string value){
	if(verbose){
		std::cout <<"setEnv: "<< key << " = \"" << value << "\"\n";
	}
	environment[key]=value;
}
bool GlobalTestConfig::checkEnv(std::string key){
	if(verbose){
		std::cout <<"checkEnv: "<< key << "\n";
	}
	return environment[key]!="" && environment[key]!="0";
}
std::string GlobalTestConfig::getEnv(std::string key){
	if(verbose){
		std::cout <<"getEnv: "<< key << "\n";
	}
	return environment[key];
}

void GlobalTestConfig::setArg(std::string key, void* value){
	if(verbose){
		std::cout <<"setArg: "<< key << " = \"" << value << "\"\n";
	}
	arguments[key]=value;
}
bool GlobalTestConfig::checkArg(std::string key){
	if(verbose){
		std::cout <<"checkArg: "<< key << "\n";
	}
	return arguments[key]!=NULL;
}
void* GlobalTestConfig::getArg(std::string key){
	if(verbose){
		std::cout <<"getArg: "<< key << "\n";
	}
	return arguments[key];
}


GlobalTestConfig::GlobalTestConfig():
	rideableFactories(),
	rideableNames(),
	tests(),
	testNames(),
	outFile(),
	allocatedRideables(){
}

GlobalTestConfig::~GlobalTestConfig(){
	delete recorder;
	delete test;
	for(int i = 0; i< rideableFactories.size(); i++){
		delete rideableFactories[i];
	}
	for(int i = 0; i< tests.size(); i++){
		delete tests[i];
	}
}


void GlobalTestConfig::addRideableOption(RideableFactory* h, const char name[]){
	rideableFactories.push_back(h);
	string s = string(name);
	rideableNames.push_back(s);
}

void GlobalTestConfig::addTestOption(Test* t, const char name[]){
	tests.push_back(t);
	string s = string(name);
	testNames.push_back(s);
}

std::string GlobalTestConfig::getRideableName(){
	return rideableNames[this->rideableType];
}
std::string GlobalTestConfig::getTestName(){
	return testNames[this->testType];
}



void GlobalTestConfig::runTest(){
	if(warmup!=0){
		warmMemory(warmup);
	}

	parallelWork(this);

	recorder->summarize();
	if(outFile.size()!=0){
		recorder->outputToFile(outFile);
		if(verbose){std::cout<<"Stored test results in: "<<outFile<<std::endl;}
	}
	if(verbose){std::cout<<recorder->getCSV()<<std::endl;}
}













