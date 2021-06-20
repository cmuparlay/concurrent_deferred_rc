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



#ifndef RIDEABLE_HPP
#define RIDEABLE_HPP

#include <string>
#include "TestConfig.hpp"

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif


class GlobalTestConfig;

class Rideable{
public:
	virtual ~Rideable(){};
};


class Reportable{
public:
	virtual void introduce(){};
	virtual void conclude(){};
};

class RideableFactory{
public:
	virtual Rideable* build(GlobalTestConfig* gtc)=0;
	virtual ~RideableFactory(){};
};
#endif
