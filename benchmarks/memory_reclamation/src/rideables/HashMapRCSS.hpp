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


#ifndef HASH_MAP_RCSS
#define HASH_MAP_RCSS

#include <atomic>
#include <Harness.hpp>
#include <ConcurrentPrimitives.hpp>
#include <RUnorderedMap.hpp>
#include <HazardTracker.hpp>
#include <MemoryTracker.hpp>
#include <RetiredMonitorable.hpp>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <cdrc/marked_arc_ptr.h>
#include <cdrc/utils.hpp>

using cdrc::marked_arc_ptr;
using cdrc::marked_rc_ptr;
using cdrc::marked_snapshot_ptr;

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V>
class HashMapRCSS : public RUnorderedMap<K,V>, public RetiredMonitorable{
  struct Node;

  struct Node{
    K key;
    V val;
    marked_arc_ptr<Node> next;
    Node(){};
    Node(K k, V v):key(k),val(v),next(){};
    inline bool deletable() {return true;}
  };
private:
  std::hash<K> hash_fn;
  padded<uint64_t>* counter;
  const int idxSize;
  padded<marked_arc_ptr<Node>>* bucket=new padded<marked_arc_ptr<Node>>[idxSize]{};
  bool findNode(marked_snapshot_ptr<Node> &prev, marked_snapshot_ptr<Node> &cur, marked_snapshot_ptr<Node> &nxt, K key, int tid);

  void reportAlloc(int tid) {
    // counter[tid].ui++;
    // if(counter[tid].ui == 4000) {
    //   counter[tid].ui = 0;
    //   collect_retired_size(marked_arc_ptr<Node>::currently_allocated(), tid);
    // }
  }

  int64_t get_allocated() {
    return marked_arc_ptr<Node>::currently_allocated();
  }

public:
  HashMapRCSS(GlobalTestConfig* gtc,int idx_size):
    RetiredMonitorable(gtc),idxSize(idx_size){
    int task_num = 256;
    if(gtc != nullptr) task_num = gtc->task_num;
    counter = new padded<uint64_t>[task_num];
    for(int i = 0; i < task_num; i++)
      counter[i] = 0;
  }
  ~HashMapRCSS(){
    #ifdef NO_DESTRUCT
      return;
    #endif
    delete[] bucket;
    delete[] counter;
  };

  marked_rc_ptr<Node> mkNode(K k, V v, int tid){
    return marked_rc_ptr<Node>::make_shared(k, v);
  }

  // debugging purposes, only works in quiescent state
  uint64_t size() {
    uint64_t sum = 0;
    for(int i = 0; i < idxSize; i++)
      sum += sizeHelper(bucket[i].ui.load());
    return sum;
  }

  uint64_t sizeHelper(marked_rc_ptr<Node> node) {
    assert(node.get_mark() == 0);
    if(node.get() == nullptr) return 0;
    return 1 + sizeHelper(node->next.load());
  }

  uint64_t keySum() {
    uint64_t sum = 0;
    for(int i = 0; i < idxSize; i++)
      sum += keySumHelper(bucket[i].ui.load());
    return sum;
  }

  uint64_t keySumHelper(marked_rc_ptr<Node> node) {
    assert(node.get_mark() == 0);
    if(node.get() == nullptr) return 0;
    return node->key + keySumHelper(node->next.load());
  }

  optional<V> get(K key, int tid);
  optional<V> put(K key, V val, int tid);
  bool insert(K key, V val, int tid);
  optional<V> remove(K key, int tid);
  optional<V> replace(K key, V val, int tid);
};

template <class K, class V> 
class HashMapRCSSFactory : public RideableFactory{
public: 
  HashMapRCSS<K,V>* build(GlobalTestConfig* gtc){
    // return new HashMapRCSS<K,V>(gtc,3000);
    return new HashMapRCSS<K,V>(gtc,atoi((gtc->getEnv("prefill")).c_str()));
  }
};

//-------Definition----------
template <class K, class V> 
optional<V> HashMapRCSS<K,V>::get(K key, int tid) {
  reportAlloc(tid);
  marked_snapshot_ptr<Node> prev;
  marked_snapshot_ptr<Node> cur;
  marked_snapshot_ptr<Node> nxt;
  optional<V> res={};

  if(findNode(prev,cur,nxt,key,tid)){
    res=cur->val;
  }
  return res;
}

template <class K, class V> 
optional<V> HashMapRCSS<K,V>::put(K key, V val, int tid) {
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_rc_ptr<Node> tmpNode = mkNode(key, val, tid);
  marked_snapshot_ptr<Node> prev;
  marked_snapshot_ptr<Node> cur;
  marked_snapshot_ptr<Node> nxt;
  optional<V> res={};

  while(true){
    if(findNode(prev,cur,nxt,key,tid)){
      res=cur->val;
      tmpNode->next.store(cur);
      marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      assert(tmpNode);
      if(addr->compare_and_swap(cur,tmpNode)){
        assert(tmpNode);
        cur->next.set_mark(1);
        nxt = cur->next.get_snapshot();
        nxt.set_mark(0);
        if(!tmpNode->next.compare_and_swap(cur,nxt)){
          findNode(prev,cur,nxt,key,tid);
        }
        break;
      }
    }
    else{//does not exist, insert.
      res={};
      tmpNode->next.store(cur);
      marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->compare_and_swap(cur,std::move(tmpNode))){
        break;
      }
    }
  }
  return res;
}

template <class K, class V> 
bool HashMapRCSS<K,V>::insert(K key, V val, int tid){
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_rc_ptr<Node> tmpNode;
  marked_snapshot_ptr<Node> prev;
  marked_snapshot_ptr<Node> cur;
  marked_snapshot_ptr<Node> nxt;
  bool res=false;
  tmpNode = mkNode(key, val, tid);

  while(true){
    if(findNode(prev,cur,nxt,key,tid)){
      res=false;
      break;
    }
    else{//does not exist, insert.
      // std::cout << "cur: " << cur.get() << std::endl;
      tmpNode->next.store(cur);
      marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->compare_and_swap(cur,std::move(tmpNode))){
        res=true;
        break;
      }
    }
  }
  return res;
}

template <class K, class V> 
optional<V> HashMapRCSS<K,V>::remove(K key, int tid) {
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_snapshot_ptr<Node> prev;
  marked_snapshot_ptr<Node> cur;
  marked_snapshot_ptr<Node> nxt;
  optional<V> res={};

  while(true){
    if(!findNode(prev,cur,nxt,key,tid)){
      res={};
      break;
    }
    res=cur->val;
    if(!cur->next.compare_and_set_mark(nxt, 1))
      continue;
    marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
    if(!addr->compare_and_swap(cur,nxt)) {
      findNode(prev,cur,nxt,key,tid);
    }
    break;
  }
  return res;
}

template <class K, class V> 
optional<V> HashMapRCSS<K,V>::replace(K key, V val, int tid) {
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_rc_ptr<Node> tmpNode;
  marked_snapshot_ptr<Node> prev;
  marked_snapshot_ptr<Node> cur;
  marked_snapshot_ptr<Node> nxt;
  optional<V> res={};
  tmpNode = mkNode(key, val, tid);

  while(true){
    if(findNode(prev,cur,nxt,key,tid)){
      res=cur->val;
      tmpNode->next.store(cur);
      marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->compare_and_swap(cur,tmpNode)){
        cur->next.set_mark(1);
        nxt = cur->next.get_snapshot();
        nxt.set_mark(0);
        if(!tmpNode->next.compare_and_swap(cur,nxt)){
          findNode(prev,cur,nxt,key,tid);
        }
        break;
      }
    }
    else{//does not exist
      res={};
      break;
    }
  }
  return res;
}

template <class K, class V> 
bool HashMapRCSS<K,V>::findNode(marked_snapshot_ptr<Node> &prev, marked_snapshot_ptr<Node> &cur, marked_snapshot_ptr<Node> &nxt, K key, int tid){
  while(true){
    prev.clear();
    nxt.clear();
    size_t idx=hash_fn(key)%idxSize;
    cur= bucket[idx].ui.get_snapshot();

    while(true){//to lock old and cur
      if(!cur) return false;
      marked_arc_ptr<Node>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(!addr->contains(cur)) break;
      nxt = cur->next.get_snapshot();
      if(nxt.get_mark()) {
        nxt.set_mark(0);
        if(!addr->compare_and_swap(cur, nxt)) break;
        cur = std::move(nxt);
      } else {
        auto ckey=cur->key;
        if(!addr->contains(cur)) break;
        if(ckey >= key) return (ckey == key);
        prev = std::move(cur);
        cur = std::move(nxt);
      }
    }
  }
}
#endif
