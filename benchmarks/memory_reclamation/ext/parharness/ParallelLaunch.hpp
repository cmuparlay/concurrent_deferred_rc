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



#ifndef PARALLEL_LAUNCH_HPP
#define PARALLEL_LAUNCH_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif


#include <pthread.h>
#include <sys/mman.h>
#include <stdlib.h> 
#include "HarnessUtils.hpp"
#include "TestConfig.hpp"
    

void parallelWork(GlobalTestConfig* gtc);

#endif
