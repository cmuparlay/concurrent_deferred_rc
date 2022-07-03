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


#ifndef SORTED_UNORDEREDMAP_RCSS
#define SORTED_UNORDEREDMAP_RCSS

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
#include <cdrc/internal/utils.h>

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V, template<typename> typename memory_manager, typename guard_t = cdrc::empty_guard>
class SortedUnorderedMapRCSS : public RUnorderedMap<K,V>, public RetiredMonitorable{
  struct Node;

  using marked_arc_ptr = cdrc::marked_arc_ptr<Node, memory_manager<Node>>;
  using marked_rc_ptr = cdrc::marked_rc_ptr<Node, memory_manager<Node>>;
  using marked_snapshot_ptr = cdrc::marked_snapshot_ptr<Node, memory_manager<Node>>;

  struct Node{
    K key;
    V val;
    marked_arc_ptr next;
    Node(){};
    Node(K k, V v):key(k),val(v),next(){};
    inline bool deletable() {return true;}
  };
private:
  std::hash<K> hash_fn;
  padded<uint64_t>* counter;
  const int idxSize;
  padded<marked_arc_ptr>* bucket=new padded<marked_arc_ptr>[idxSize]{};
  bool findNode(marked_snapshot_ptr &prev, marked_snapshot_ptr &cur, marked_snapshot_ptr &nxt, K key, int tid);

  void reportAlloc([[maybe_unused]] int tid) {
    // counter[tid].ui++;
    // if(counter[tid].ui == 4000) {
    //   counter[tid].ui = 0;
    //   collect_retired_size(marked_arc_ptr::currently_allocated(), tid);
    // }
  }

public:
  SortedUnorderedMapRCSS(GlobalTestConfig* gtc,int idx_size):
    RetiredMonitorable(gtc),idxSize(idx_size){
    int task_num = 256;
    if(gtc != nullptr) task_num = gtc->task_num;
    counter = new padded<uint64_t>[task_num];
    for(int i = 0; i < task_num; i++)
      counter[i] = 0;
  }
  ~SortedUnorderedMapRCSS(){
    #ifdef NO_DESTRUCT
      return;
    #endif
    delete[] bucket;
    delete[] counter;
  };

  int64_t get_allocated() {
    return marked_arc_ptr::currently_allocated();
  }

  marked_rc_ptr mkNode(K k, V v, int){
    return marked_rc_ptr::make_shared(k, v);
  }

  // debugging purposes, only works in quiescent state
  uint64_t size() {
    uint64_t sum = 0;
    for(int i = 0; i < idxSize; i++)
      sum += sizeHelper(bucket[i].ui.load());
    return sum;
  }

  uint64_t sizeHelper(marked_rc_ptr node) {
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

  uint64_t keySumHelper(marked_rc_ptr node) {
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

template <class K, class V, template<typename> typename memory_manager, typename guard_t = cdrc::empty_guard>
class SortedUnorderedMapRCSSFactory : public RideableFactory{
public: 
  SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>* build(GlobalTestConfig* gtc){
    return new SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>(gtc,atoi((gtc->getEnv("prefill")).c_str()));
  }
};

template <class K, class V, template<typename> typename memory_manager, typename guard_t = cdrc::empty_guard>
class SortedUnorderedMapRCSSTestFactory : public RideableFactory{
 public:
  SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>* build(GlobalTestConfig* gtc){
    return new SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>(gtc,10000);
  }
};

//-------Definition----------
template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
optional<V> SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::get(K key, int tid) {
  [[maybe_unused]] guard_t guard;
  reportAlloc(tid);
  marked_snapshot_ptr prev;
  marked_snapshot_ptr cur;
  marked_snapshot_ptr nxt;
  optional<V> res={};

  if(findNode(prev,cur,nxt,key,tid)){
    res=cur->val;
  }
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
optional<V> SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::put(K, V, int) { return {}; }

template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
bool SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::insert(K key, V val, int tid){
  [[maybe_unused]] guard_t guard;
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_rc_ptr tmpNode;
  marked_snapshot_ptr prev;
  marked_snapshot_ptr cur;
  marked_snapshot_ptr nxt;
  bool res=false;
  tmpNode = mkNode(key, val, tid);

  while(true){
    if(findNode(prev,cur,nxt,key,tid)){
      res=false;
      break;
    }
    else{//does not exist, insert.
      // std::cout << "cur: " << cur.get() << std::endl;
      tmpNode->next.store_non_racy(cur);
      marked_arc_ptr* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(addr->compare_and_swap(cur,std::move(tmpNode))){
        res=true;
        break;
      }
    }
  }
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
optional<V> SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::remove(K key, int tid) {
  [[maybe_unused]] guard_t guard;
  reportAlloc(tid);
  size_t idx=hash_fn(key)%idxSize;
  marked_snapshot_ptr prev;
  marked_snapshot_ptr cur;
  marked_snapshot_ptr nxt;
  optional<V> res={};

  while(true){
    if(!findNode(prev,cur,nxt,key,tid)){
      res={};
      break;
    }
    res=cur->val;
    if(!cur->next.compare_and_set_mark(nxt, 1))
      continue;
    marked_arc_ptr* addr = (prev ? &(prev->next) : &bucket[idx].ui);
    if(!addr->compare_and_swap(cur,nxt)) {
      findNode(prev,cur,nxt,key,tid);
    }
    break;
  }
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
optional<V> SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::replace(K, V, int) { return {}; }

template <class K, class V, template<typename> typename memory_manager, typename guard_t> 
bool SortedUnorderedMapRCSS<K,V,memory_manager,guard_t>::findNode(marked_snapshot_ptr &prev, marked_snapshot_ptr &cur, marked_snapshot_ptr &nxt, K key, int){
  while(true){
    prev.clear();
    size_t idx=hash_fn(key)%idxSize;
    //bool cmark=false;
    cur= bucket[idx].ui.get_snapshot();

    while(true){//to lock old and cur
      if(!cur) return false;
      nxt = cur->next.get_snapshot();
      bool cmark= (bool) nxt.get_mark();
      if(!cur->next.contains(nxt))
        break;
      nxt.set_mark(0);
      auto ckey=cur->key;
      marked_arc_ptr* addr = (prev ? &(prev->next) : &bucket[idx].ui);
      if(!addr->contains(cur))
        break;//return findNode(prev,cur,nxt,key,tid);
      if(!cmark){
        if(ckey>=key) return ckey==key;
        prev=std::move(cur);
      }
      else{
        if(!addr->compare_and_swap(cur,nxt))
          break;//return findNode(prev,cur,nxt,key,tid);
      }
      cur=std::move(nxt);
    }
  }
}
#endif
