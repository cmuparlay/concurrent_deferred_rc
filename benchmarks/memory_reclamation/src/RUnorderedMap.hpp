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


#ifndef RUNORDEREDMAP_HPP
#define RUNORDEREDMAP_HPP

#include <string>
#include "Rideable.hpp"

#include "optional.hpp"

template <class K, class V> class RUnorderedMap : public virtual Rideable{
public:

	// Gets value corresponding to a key
	// returns : the most recent value set for that key
	virtual optional<V> get(K key, int tid)=0;

	// Puts a new key/value pair into the map	
	// returns : the previous value for this key,
	// or NULL if no such value exists
	virtual optional<V> put(K key, V val, int tid)=0;

	// Inserts a new key/value pair into the map
	// if the key is not already present
	// returns : true if the insert is successful, false otherwise
	virtual bool insert(K key, V val, int tid)=0;

	// Removes a value corresponding to a key
	// returns : the removed value
	virtual optional<V> remove(K key, int tid)=0;

	// Replaces the value corresponding to a key
	// if the key is already present in the map
	// returns : the replaced value, or NULL if replace was unsuccessful
	virtual optional<V> replace(K key, V val, int tid)=0;

	virtual uint64_t size()=0;

	virtual int64_t get_allocated() = 0;
};

#endif
