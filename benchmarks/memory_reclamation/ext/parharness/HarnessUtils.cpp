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



#include "HarnessUtils.hpp"
#include <list>
#include <iostream> 

using namespace std;

void errexit (const char *err_str)
{
	fprintf (stderr, "%s\n", err_str);
	throw 1;
}


void faultHandler(int sig) {
	void *buf[30];
	size_t sz;

	// scrape backtrace
	sz = backtrace(buf, 30);

	// print error msg
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(buf, sz, STDERR_FILENO);
	exit(1);
}


bool isInteger(const std::string &s){
	// check for empty case, and easy fail at start of string
	if(s.empty() || ( (s[0] != '-') && (s[0] != '+') && (!isdigit(s[0])) ) ){
		return false;
	}

	// strol parses only if valid long.
	char* buf;
	strtol(s.c_str(), &buf, 60);
	return (*buf == 0);
}


std::string machineName(){
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	return std::string(hostname);
}

int archBits(){
	if(sizeof(void*) == 8){
		return 64;
	}
	else if(sizeof(void*) == 16){
		return 128;
	}
	else if(sizeof(void*) == 4){
		return 32;
	}
	else if(sizeof(void*) == 2){
		return 8;
	}
}

unsigned int nextRand(unsigned int last) {
	unsigned int next = last;
	next = next * 1664525 + 1013904223;
	return next;	
	// return rand();
}

int warmMemory(unsigned int megabytes){
	uint64_t preheat = megabytes*(2<<20);

	int blockSize = sysconf(_SC_PAGESIZE);
	int toAlloc = preheat / blockSize;
	list<void*> allocd;

	int ret = 0;

	for(int i = 0; i<toAlloc; i++){
		int32_t* ptr  = (int32_t*)malloc(blockSize);
		// ptr2,3 reserves space in case the list needs room
		// this prevents seg faults, but we may be killed anyway
		// if the system runs out of memory ("Killed" vs "Segmentation Fault")
		int32_t* ptr2  = (int32_t*)malloc(blockSize); 
		int32_t* ptr3  = (int32_t*)malloc(blockSize);
		if(ptr==NULL || ptr2==NULL || ptr3==NULL){
			ret = -1;
			break;
		}
		free(ptr2); free(ptr3);
		ptr[0]=1;
		allocd.push_back(ptr);
	}
	for(int i = 0; i<toAlloc; i++){
		void* ptr = allocd.back();
		allocd.pop_back();
		free(ptr);
	}
	return ret;
}
