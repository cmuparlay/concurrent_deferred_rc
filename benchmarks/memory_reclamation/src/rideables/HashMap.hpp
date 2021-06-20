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


#ifndef HASH_MAP
#define HASH_MAP

#include <atomic>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "RUnorderedMap.hpp"
#include "HazardTracker.hpp"
#include "MemoryTracker.hpp"
#include "RetiredMonitorable.hpp"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V>
class HashMap : public RUnorderedMap<K,V>, public RetiredMonitorable{

  struct Node{
    K key;
    V val;
    std::atomic<Node*> next;
    Node(){};
    Node(K k, V v, Node* n):key(k),val(v),next(n){};
    inline bool deletable() {return true;}
  };
private:
  std::hash<K> hash_fn;
  const int idxSize;
  paddedAtomic<Node*>* bucket=new paddedAtomic<Node*>[idxSize]{};
  bool findNode(Node* &prev, Node* &cur, Node* &nxt, K key, int tid);
  
  MemoryTracker<Node>* memory_tracker;
  padded<uint64_t>* counter;

  const size_t GET_POINTER_BITS = 0xfffffffffffffffe;
  inline Node* getPtr(Node* mptr){
    return (Node*) ((size_t)mptr & GET_POINTER_BITS);
  }
  inline bool getMk(Node* mptr){
    return (bool)((size_t)mptr & 1);
  }
  inline Node* mixPtrMk(Node* ptr, bool mk){
    return (Node*) ((size_t)ptr | mk);
  }
  inline Node* setMk(Node* mptr){
    return mixPtrMk(mptr,true);
  }

  void destroryList(Node* n) {
    if(n == nullptr) return;
    destroryList(n->next);
    memory_tracker->reclaim(n);
  }

  void reportAlloc(int tid) {
    // counter[tid].ui++;
    // if(counter[tid].ui == 4000) {
    //   counter[tid].ui = 0;
    //   collect_retired_size(memory_tracker->get_allocated(), tid);
    // }
  }

  int64_t get_allocated() {
    return memory_tracker->get_allocated();
  }

  uint64_t size(){
    return 0;
  }
public:
  HashMap(GlobalTestConfig* gtc,int idx_size):
    RetiredMonitorable(gtc),idxSize(idx_size){
    int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
    int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
    std::cout<<"emptyf:"<<emptyf<<std::endl;
    memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 3, COLLECT);

    counter = new padded<uint64_t>[gtc->task_num];
    for(int i = 0; i < gtc->task_num; i++)
      counter[i] = 0;

    // for(int i = 0; i < idx_size; i++) {
    //   bucket[i].ui = mkNode(-1, -1, nullptr, 0); // TODO: handle INF values better
    // }


  }
  ~HashMap(){ 
    #ifdef NO_DESTRUCT
      return;
    #endif
    for(int i = 0; i < idxSize; i++)
      destroryList(bucket[i].ui);
    delete[] bucket;
    delete[] counter;
    delete memory_tracker;
  };

  Node* mkNode(K k, V v, Node* n, int tid){
    void* ptr = memory_tracker->alloc(tid);
    return new (ptr) Node(k, v, n);
  }


  optional<V> get(K key, int tid);
  optional<V> put(K key, V val, int tid) {return {};}
  bool insert(K key, V val, int tid);
  optional<V> remove(K key, int tid);
  optional<V> replace(K key, V val, int tid) {return {};}
};

template <class K, class V> 
class HashMapFactory : public RideableFactory{
  HashMap<K,V>* build(GlobalTestConfig* gtc){  // TODO: set a more reasonable default hash table size
    return new HashMap<K,V>(gtc,atoi((gtc->getEnv("prefill")).c_str()));
  }
};

//-------Definition----------
template <class K, class V> 
optional<V> HashMap<K,V>::get(K key, int tid) {
  reportAlloc(tid);
  Node* prev=nullptr;
  Node* cur=nullptr;
  Node* nxt=nullptr;
  optional<V> res={};

  // collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

  memory_tracker->start_op(tid);
  if(findNode(prev,cur,nxt,key,tid)){
    res=cur->val;
  }
  memory_tracker->clear_all(tid);
  memory_tracker->end_op(tid);
  return res;
}

template <class K, class V> 
bool HashMap<K,V>::insert(K key, V val, int tid){
  reportAlloc(tid);
  Node* tmpNode = nullptr;
  Node* prev=nullptr;
  Node* cur=nullptr;
  Node* nxt=nullptr;
  bool res=false;
  tmpNode = mkNode(key, val, nullptr, tid);

  // collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

  memory_tracker->start_op(tid);
  while(true){
    if(findNode(prev,cur,nxt,key,tid)){
      res=false;
      memory_tracker->reclaim(tmpNode, tid);
      break;
    }
    else{//does not exist, insert.
      size_t idx=hash_fn(key)%idxSize;
      tmpNode->next.store(cur,std::memory_order_release);
      std::atomic<Node*>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->compare_exchange_strong(cur,tmpNode,std::memory_order_acq_rel)){
        res=true;
        break;
      }
    }
  }
  memory_tracker->end_op(tid);
  memory_tracker->clear_all(tid);
  return res;
}

template <class K, class V> 
optional<V> HashMap<K,V>::remove(K key, int tid) {
  reportAlloc(tid);
  Node* prev=nullptr;
  Node* cur=nullptr;
  Node* nxt=nullptr;
  optional<V> res={};

  // collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);

  memory_tracker->start_op(tid);
  while(true){
    if(!findNode(prev,cur,nxt,key,tid)){
      res={};
      break;
    }
    res=cur->val;
    if(!cur->next.compare_exchange_strong(nxt,setMk(nxt),std::memory_order_acq_rel))
      continue;
    size_t idx=hash_fn(key)%idxSize;
    std::atomic<Node*>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
    if(addr->compare_exchange_strong(cur,nxt,std::memory_order_acq_rel)){
      memory_tracker->retire(cur, tid);
    }
    else{
      findNode(prev,cur,nxt,key,tid);
    }
    break;
  }
  memory_tracker->end_op(tid);
  memory_tracker->clear_all(tid);
  return res;
}

template <class K, class V> 
bool HashMap<K,V>::findNode(Node* &prev, Node* &cur, Node* &nxt, K key, int tid){
  // if(tid > 190) std::cout << "tid: " << tid << std::endl;
  int cc = 0;
  while(true){
    size_t idx=hash_fn(key)%idxSize;
    prev=nullptr;
    nxt=nullptr;
    cur = bucket[idx].ui.load();

    while(true){//to lock old and cur
      cc++;
      if(cur==nullptr) {
        // std::cout << "nodes traversed (nullptr) " << cc << std::endl;
        return false;
      }
      memory_tracker->reserve_slot(cur, 0, tid, nullptr);
      std::atomic<Node*>* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->load() != cur) break;
      nxt = cur->next;
      if(getMk(nxt)) {
        if(!addr->compare_exchange_strong(cur,getPtr(nxt),std::memory_order_acq_rel)) 
          break;
        memory_tracker->retire(cur, tid);
        cur = getPtr(nxt);
      } else {
        auto ckey=cur->key;
        if(addr->load() != cur) break;
        if(ckey > key) {
          // std::cout << "nodes traversed " << cc << std::endl;
          return (ckey == key);
        }
        prev = cur;
        memory_tracker->transfer(0,2,tid);
        memory_tracker->transfer(1,0,tid);
        memory_tracker->transfer(2,1,tid);
        cur = nxt;
      }
    }
  }
}
#endif