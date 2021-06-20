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


#ifndef RORDEREDMAP_HPP
#define RORDEREDMAP_HPP

#include <string>
#include "Rideable.hpp"
#include "RUnorderedMap.hpp"
#include <map>


template <class K, class V> class ROrderedMap : public virtual RUnorderedMap<K, V>{
//class ROrderedMap : public virtual RUnorderedMap<std::string,std::string>{
public:

	//begin - defined in RUnorderedMap:

	//// Gets value corresponding to a key
	//// returns : the most recent value set for that key
	//virtual std::string get(std::string key, int tid)=0;
	//// Puts a new key/value pair into the map	
	//// returns : the previous value for this key,
	//// or NULL if no such value exists
	//virtual std::string put(std::string key, std::string val, int tid)=0;
	//// Inserts a new key/value pair into the map
	//// if the key is not already present
	//// returns : true if the insert is successful, false otherwise
	//virtual bool insert(std::string key, std::string val, int tid)=0;
	//// Removes a value corresponding to a key
	//// returns : the removed value
	//virtual std::string remove(std::string key, int tid)=0;
	//// Replaces the value corresponding to a key
	//// if the key is already present in the map
	//// returns : the replaced value, or NULL if replace was unsuccessful
	//virtual std::string replace(std::string key, std::string val, int tid)=0;

	//end

	// Get all the values between key1 and key2, inclusively. (Tentative)
	// returns: the pointer to the first pointer of an array
	virtual std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid) = 0;
	
};

#endif
