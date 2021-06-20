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



#ifndef HARNESS_UTILS_HPP
#define HARNESS_UTILS_HPP

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string>

// UTILITY CODE -----------------------------
void errexit (const char *err_str);
void faultHandler(int sig);
unsigned int nextRand(unsigned int last);
int warmMemory(unsigned int megabytes);
bool isInteger(const std::string & s);
std::string machineName();
int archBits();

inline int64_t timeDiff(struct timeval* start, struct timeval* end){
	int64_t ret =  (((int64_t)end->tv_sec)-start->tv_sec)*(1000000);
	ret += ((int64_t)end->tv_usec)-start->tv_usec;
	return ret;
}
#endif
