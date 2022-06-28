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


#ifndef NATARAJAN_TREE_RCSS
#define NATARAJAN_TREE_RCSS

#include <iostream>
#include <atomic>
#include <algorithm>
#include <unistd.h>
#include "Harness.hpp"
#include "ConcurrentPrimitives.hpp"
#include "ROrderedMap.hpp"
#include "HazardTracker.hpp"
#include "RUnorderedMap.hpp"
// #include "ssmem.h"
#include "MemoryTracker.hpp"
#include "RetiredMonitorable.hpp"

#include <cdrc/marked_arc_ptr.h>
#include <cdrc/internal/utils.h>

// //GC Method: ssmem from LPD-EPFL
// #ifdef NGC
// #define ssmem_alloc(x,y) malloc(y)
// #define ssmem_free(x,y)
// #endif
#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

// thread_local int seek_count = 0;

template <class K, class V, template<typename> typename memory_manager, typename guard_t = cdrc::empty_guard_>
class NatarajanTreeRCSS : public ROrderedMap<K,V>, public RetiredMonitorable{

  struct Node;

  using marked_arc_ptr = cdrc::marked_arc_ptr<Node, memory_manager<Node>>;
  using marked_rc_ptr = cdrc::marked_rc_ptr<Node, memory_manager<Node>>;
  using marked_snapshot_ptr = cdrc::marked_snapshot_ptr<Node, memory_manager<Node>>;

 private:
  /* structs*/
  struct Node{
    int level;
    K key;
    V val;
    marked_arc_ptr left;
    marked_arc_ptr right;

    virtual ~Node(){};
    static marked_rc_ptr alloc(K k, V v, marked_rc_ptr l, marked_rc_ptr r,MemoryTracker<Node>* memory_tracker){
      return alloc(k,v,std::move(l),std::move(r),-1,memory_tracker);
    }

    inline bool deletable() {return true;}
    static marked_rc_ptr alloc(K k, V v,marked_rc_ptr l, marked_rc_ptr r, int lev, MemoryTracker<Node>*){
      return marked_rc_ptr::make_shared(k,v,std::move(l),std::move(r),lev);
    }
    Node(){};
    Node(K k, V v, marked_rc_ptr l, marked_rc_ptr r,int lev):level(lev),key(k),val(v),left(std::move(l)),right(std::move(r)){};
    Node(K k, V v, marked_rc_ptr l, marked_rc_ptr r):level(-1),key(k),val(v),left(std::move(l)),right(std::move(r)){};
  };
  struct SeekRecord{
    marked_snapshot_ptr ancestor;
    marked_snapshot_ptr successor;
    marked_snapshot_ptr parent;
    marked_snapshot_ptr leaf;
  };

  /* variables */
  MemoryTracker<Node>* memory_tracker;
  K infK{};
  V defltV{};
  // Node r{infK,defltV,nullptr,nullptr,2};
  // Node s{infK,defltV,nullptr,nullptr,1};
  marked_arc_ptr r; // immutable, can't be marked. I made it an arc pointer just so that it can support snapshots
  marked_arc_ptr s; // immutable, can't be marked
  padded<SeekRecord>* records;
  const uintptr_t FLAG = 2;
  const uintptr_t TAG = 1;

  padded<uint64_t>* counter;

  //node comparison
  inline bool isInf(Node* n){
    return getInfLevel(n)!=-1;
  }
  inline int getInfLevel(Node* n){
    //0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
    return n->level;
  }
  inline bool nodeLess(Node* n1, Node* n2){
    int i1=getInfLevel(n1);
    int i2=getInfLevel(n2);
    return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
  }
  inline bool nodeEqual(Node* n1, Node* n2){
    int i1=getInfLevel(n1);
    int i2=getInfLevel(n2);
    if(i1==-1&&i2==-1)
      return n1->key==n2->key;
    else
      return i1==i2;
  }
  inline bool nodeLessEqual(Node* n1, Node* n2){
    return !nodeLess(n2,n1);
  }

  // bit = 1 or 2
  inline void clear_mark_bit(marked_snapshot_ptr& mptr, uintptr_t bit) {
    uintptr_t mark = mptr.get_mark();
    uintptr_t mask = ~(1 << (bit-1));
    mptr.set_mark(mark & mask);
  }

  inline bool get_mark_bit(marked_snapshot_ptr& mptr, uintptr_t bit) {
    uintptr_t mark = mptr.get_mark();
    return mark & bit;
  }

  void clearSeekRecord(SeekRecord* seekRecord) {
    seekRecord->ancestor.clear();
    seekRecord->successor.clear();
    seekRecord->parent.clear();
    seekRecord->leaf.clear();
  }

  /* private interfaces */
  void seek(K key, int tid);
  void seekLeaf(K key, int tid);
  bool cleanup(K key, int tid);
  void doRangeQuery(Node& k1, Node& k2, int tid, marked_snapshot_ptr root, std::map<K,V>& res);

  void reportAlloc(int) {
    // counter[tid].ui++;
    // if(counter[tid].ui == 4000) {
    // 	counter[tid].ui = 0;
    // 	collect_retired_size(marked_arc_ptr<Node>::currently_allocated(), tid);
    // }
  }

  int64_t get_allocated() {
    return marked_arc_ptr::currently_allocated();
  }

 public:
  NatarajanTreeRCSS(GlobalTestConfig* gtc): RetiredMonitorable(gtc)
  {
    // int emptyf = 1;
    int task_num = 256;
    if(gtc != nullptr) task_num = gtc->task_num;

    counter = new padded<uint64_t>[task_num];
    for(int i = 0; i < task_num; i++)
      counter[i] = 0;

    memory_tracker = nullptr;
    r.store(Node::alloc(infK,defltV,nullptr,nullptr,2,memory_tracker));
    s.store(Node::alloc(infK,defltV,nullptr,nullptr,1,memory_tracker));
    marked_snapshot_ptr rptr = r.get_snapshot();
    marked_snapshot_ptr sptr = s.get_snapshot();

    rptr->right.store(Node::alloc(infK,defltV,nullptr,nullptr,2,memory_tracker));
    rptr->left.store(s.load());
    sptr->right.store(Node::alloc(infK,defltV,nullptr,nullptr,1,memory_tracker));
    sptr->left.store(Node::alloc(infK,defltV,nullptr,nullptr,0,memory_tracker));
    records = new padded<SeekRecord>[task_num]{};
    // std::cout << "constructing NatarajanTreeRCSS" << std::endl;
  };
  ~NatarajanTreeRCSS(){
#ifdef NO_DESTRUCT
    new marked_rc_ptr(r.exchange(nullptr));
    new marked_rc_ptr(s.exchange(nullptr));
    return;
#endif
    // std::cout << "destructing NatarajanTreeRCSS" << std::endl;
    delete[] records;
    delete[] counter;
  };

  // debugging purposes, only works in quiescent state
  uint64_t size() {
    return sizeHelper(r.load());
  }

  uint64_t sizeHelper(marked_rc_ptr node) {
    assert(node.get_mark() == 0);
    if(node.get() == nullptr) return 0;
    uint64_t sum = sizeHelper(node->left.load()) + sizeHelper(node->right.load());
    if(isInf(node.get()) || node->left.load().get() != nullptr) return sum;
    else return 1 + sum;
  }

  uint64_t keySum() {
    return keySumHelper(r.load());
  }

  uint64_t keySumHelper(marked_rc_ptr node) {
    assert(node.get_mark() == 0);
    if(node.get() == nullptr) return 0;
    uint64_t sum = keySumHelper(node->left.load()) + keySumHelper(node->right.load());
    if(isInf(node.get()) || node->left.load().get() != nullptr) return sum;
    else return node->key + sum;
  }

  void conclude() {
    std::cout << "concluding" << std::endl;
  }

  optional<V> get(K key, int tid);
  optional<V> put(K key, V val, int tid);
  bool insert(K key, V val, int tid);
  optional<V> remove(K key, int tid);
  optional<V> replace(K key, V val, int tid);
  std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid);
};

template <class K, class V, template<typename> typename memory_manager, typename guard_t = cdrc::empty_guard_>
class NatarajanTreeRCSSFactory : public RideableFactory{
 public:
  NatarajanTreeRCSS<K,V,memory_manager,guard_t>* build(GlobalTestConfig* gtc){
    return new NatarajanTreeRCSS<K,V,memory_manager,guard_t>(gtc);
  }
};

//-------Definition----------
template <class K, class V, template<typename> typename memory_manager, typename guard_t>
void NatarajanTreeRCSS<K,V,memory_manager,guard_t>::seekLeaf(K key, int tid){
  // seek_count++;
  /* initialize the seek record using sentinel nodes */
  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
  SeekRecord* seekRecord=&(records[tid].ui);
  seekRecord->leaf=s.get_snapshot()->left.get_snapshot();
  seekRecord->leaf.set_mark(0);

  assert(seekRecord->leaf);
  marked_snapshot_ptr current = seekRecord->leaf->left.get_snapshot();
  current.set_mark(0);

  /* traverse the tree */
  while(current){
    /* advance parent and leaf pointers */
    seekRecord->leaf=std::move(current);

    /* update other variables used in traversal */
    if(nodeLess(&keyNode,seekRecord->leaf.get())){
      current=seekRecord->leaf->left.get_snapshot();
    }
    else{
      current=seekRecord->leaf->right.get_snapshot();
    }
    current.set_mark(0);
  }
  /* traversal complete */
  return;
}

//-------Definition----------
template <class K, class V, template<typename> typename memory_manager, typename guard_t>
void NatarajanTreeRCSS<K,V,memory_manager,guard_t>::seek(K key, int tid){
  // seek_count++;
  /* initialize the seek record using sentinel nodes */
  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
  SeekRecord* seekRecord=&(records[tid].ui);
  seekRecord->ancestor=r.get_snapshot();
  seekRecord->successor.clear();
  // seekRecord->successor=seekRecord->ancestor->left.get_snapshot();
  seekRecord->parent=seekRecord->ancestor->left.get_snapshot();
  seekRecord->leaf=seekRecord->parent->left.get_snapshot();
  seekRecord->leaf.set_mark(0);

  /* initialize other variables used in the traversal */
  bool parentTagged = seekRecord->parent->left.get_mark_bit(TAG);
  bool currentTagged = seekRecord->leaf->left.get_mark_bit(TAG);

  assert(seekRecord->leaf);
  marked_snapshot_ptr current = seekRecord->leaf->left.get_snapshot();
  current.set_mark(0);

  /* traverse the tree */
  while(current){
    /* check if the edge from the current parent node is tagged */
    if(!parentTagged){
      /*
       * found an untagged edge in the access path;
       * advance ancestor and successor pointers.
       */
      seekRecord->ancestor=std::move(seekRecord->parent);
      seekRecord->successor.clear();
      // seekRecord->successor.clear();
    } else {
      if(seekRecord->successor == nullptr) {
        seekRecord->successor = std::move(seekRecord->parent);
      }
    }
    // else if(!seekRecord->successor) {
    // 	seekRecord->successor = seekRecord->parent;
    // }

    /* advance parent and leaf pointers */
    seekRecord->parent=std::move(seekRecord->leaf);
    seekRecord->leaf=std::move(current);

    /* update other variables used in traversal */
    parentTagged=currentTagged;
    if(nodeLess(&keyNode,seekRecord->leaf.get())){
      current=seekRecord->leaf->left.get_snapshot();
    }
    else{
      current=seekRecord->leaf->right.get_snapshot();
    }

    currentTagged=(get_mark_bit(current, TAG));
    current.set_mark(0);
  }
  // if(!seekRecord->successor) seekRecord->successor = seekRecord->parent;
  assert(seekRecord->ancestor.get_mark() == 0);
  // assert(seekRecord->successor.get_mark() == 0);
  assert(seekRecord->parent.get_mark() == 0);
  assert(seekRecord->leaf.get_mark() == 0);

  /* traversal complete */
  return;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
bool NatarajanTreeRCSS<K,V,memory_manager,guard_t>::cleanup(K key, int tid){
  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared

  /* retrieve addresses stored in seek record */
  SeekRecord* seekRecord=&(records[tid].ui);
  marked_snapshot_ptr& ancestor = seekRecord->ancestor;
  marked_snapshot_ptr& successor = seekRecord->successor;
  marked_snapshot_ptr& parent = seekRecord->parent;
  // marked_snapshot_ptr<Node>& leaf = seekRecord->leaf;

  marked_arc_ptr* successorAddr=nullptr;
  marked_arc_ptr* childAddr=nullptr;
  marked_arc_ptr* siblingAddr=nullptr;

  /* obtain address of field of ancestor node that will be modified */
  if(nodeLess(&keyNode,ancestor.get()))
    successorAddr=&(ancestor->left);
  else
    successorAddr=&(ancestor->right);

  /* obtain addresses of child fields of parent node */
  if(nodeLess(&keyNode,parent.get())){
    childAddr=&(parent->left);
    siblingAddr=&(parent->right);
  }
  else{
    childAddr=&(parent->right);
    siblingAddr=&(parent->left);
  }

  if(!childAddr->get_mark_bit(FLAG)) {
    /* the leaf is not flagged, thus sibling node should be flagged */
    /* switch the sibling address */
    siblingAddr=childAddr;
  }

  /* use TAS to tag sibling edge */
  siblingAddr->set_mark_bit(TAG);

  /* read the flag and address fields */
  marked_snapshot_ptr tmpSibling=siblingAddr->get_snapshot();
  clear_mark_bit(tmpSibling, TAG);
  /* make the sibling node a direct child of the ancestor node */
  // std::cout << "successor: " << successorAddr->get_snapshot().get() << " " << successorAddr->get_mark() << " " << successor.get() << std::endl;
// std::cout << "tmpSibling: " << tmpSibling.get() << std::endl;
  if(successor == nullptr) {
    // std::cout << "calling CAS" << std::endl;
    return successorAddr->compare_and_swap(parent, tmpSibling);
  }
  else
    return successorAddr->compare_and_swap(successor, tmpSibling);
}

/* to test rangeQuery */
// template <>
// optional<int> NatarajanTreeRCSS<int,int>::get(int key, int tid){
// 	int len=0;
// 	auto x = rangeQuery(key-500,key,len,tid);
// 	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
// 	optional<int> res={};
// 	SeekRecord* seekRecord=&(records[tid].ui);
// 	Node* leaf=nullptr;
// 	seek(key,tid);
// 	leaf=getPtr(seekRecord->leaf);
// 	if(nodeEqual(&keyNode,leaf)){
// 		res = leaf->val;
// 	}
// 	return res;
// }

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
optional<V> NatarajanTreeRCSS<K,V,memory_manager,guard_t>::get(K key, int tid){
  [[maybe_unused]] guard_t guard;

  reportAlloc(tid);
  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
  optional<V> res={};
  SeekRecord* seekRecord=&(records[tid].ui);
  seekLeaf(key,tid);
  marked_snapshot_ptr& leaf = seekRecord->leaf;
  if(nodeEqual(&keyNode,leaf.get())){
    res = leaf->val;
  }
  seekRecord->leaf.clear();
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
optional<V> NatarajanTreeRCSS<K,V,memory_manager,guard_t>::put(K, V, int){ return {}; }

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
bool NatarajanTreeRCSS<K,V,memory_manager,guard_t>::insert(K key, V val, int tid){
  [[maybe_unused]] guard_t guard;

  reportAlloc(tid);
  bool res=false;
  SeekRecord* seekRecord=&(records[tid].ui);

  marked_rc_ptr newInternal;
  marked_rc_ptr newLeaf=Node::alloc(key,val,nullptr,nullptr,memory_tracker);//also for comparing keys
  marked_arc_ptr* childAddr=nullptr;
  while(true){
    seek(key,tid);
    marked_snapshot_ptr& leaf = seekRecord->leaf;
    marked_snapshot_ptr& parent = seekRecord->parent;
    if(!nodeEqual(newLeaf.get(),leaf.get())){//key does not exist
      /* obtain address of the child field to be modified */
      if(nodeLess(newLeaf.get(),parent.get()))
        childAddr=&(parent->left);
      else
        childAddr=&(parent->right);

      /* create left and right leave of newInternal */
      marked_rc_ptr newLeft=nullptr;
      marked_rc_ptr newRight=nullptr;
      bool leftc;
      if(nodeLess(newLeaf.get(),leaf.get())){
        newLeft=std::move(newLeaf);
        newRight=marked_rc_ptr(leaf);
        leftc = true;
      }
      else{
        newLeft=marked_rc_ptr(leaf);
        newRight=std::move(newLeaf);
        leftc = false;
      }

      /* create newInternal */
      if(isInf(leaf.get())){
        int lev=getInfLevel(leaf.get());
        newInternal=Node::alloc(infK,defltV,std::move(newLeft),std::move(newRight),lev,memory_tracker);
      }
      else
        newInternal=Node::alloc(std::max(key,leaf->key),defltV,std::move(newLeft),std::move(newRight),memory_tracker);
      // TODO: I probabably got rid of too many getPtrs because leaf can still be tagged.
      /* try to add the new nodes to the tree */
      if(childAddr->compare_and_swap(leaf,std::move(newInternal))){
        res=true;
        break;//insertion succeeds
      }
      else{//fails; help conflicting delete operation
        // auto tmpChild = childAddr->get();
        // if(getPtr(tmpChild) == leaf.get() && getMark(tmpChild) != 0){
        marked_snapshot_ptr tmpChild=childAddr->get_snapshot();
        if(tmpChild.get() == leaf.get() && tmpChild.get_mark() != 0){
          /*
           * address of the child has not changed
           * and either the leaf node or its sibling
           * has been flagged for deletion
           */
          cleanup(key,tid);
        }
      }
      if(leftc)
        newLeaf = std::move(newInternal->left);
      else
        newLeaf = std::move(newInternal->right);
    }
    else{//key exists, insertion fails
      res=false;
      break;
    }
  }
  clearSeekRecord(seekRecord);
  return res;
}

// Optimizations; avoid copy, avoid get snapshots of root (HP doesn't do that either),

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
optional<V> NatarajanTreeRCSS<K,V,memory_manager,guard_t>::remove(K key, int tid){
  [[maybe_unused]] guard_t guard;

  reportAlloc(tid);

  bool injecting = true;
  optional<V> res={};
  SeekRecord* seekRecord=&(records[tid].ui);

  Node keyNode{key,defltV,nullptr,nullptr};//node to be compared

  Node* leaf = nullptr;
  marked_arc_ptr* childAddr=nullptr;
  while(true){
    // std::cerr << injecting;
    seek(key,tid);
    marked_snapshot_ptr& parent = seekRecord->parent;
    /* obtain address of the child field to be modified */
    if(nodeLess(&keyNode,parent.get()))
      childAddr=&(parent->left);
    else
      childAddr=&(parent->right);

    if(injecting){
      /* injection mode: check if the key exists */
      leaf = seekRecord->leaf.get();
      if(!nodeEqual(leaf,&keyNode)){//does not exist
        res={};
        break;
      }

      /* inject the delete operation into the tree */
      res=seekRecord->leaf->val;
      // std::cout << std::endl << childAddr->get_snapshot().get() << " " << childAddr->get_mark() << " " << leaf.get() << std::endl;
      if(childAddr->compare_and_set_mark(seekRecord->leaf, FLAG)){
        /* advance to cleanup mode to remove the leaf node */
        injecting=false;
        if(cleanup(key,tid)) break;
      }
      else{
        marked_snapshot_ptr tmpChild=childAddr->get_snapshot();
        if(tmpChild.get() == leaf && tmpChild.get_mark() != 0){
          /*
           * address of the child has not
           * changed and either the leaf
           * node or its sibling has been
           * flagged for deletion
           */
          // std::cout << "cleanup" << std::endl;
          cleanup(key,tid);
        }
      }
    }
    else{
      /* cleanup mode: check if flagged node still exists */
      if(seekRecord->leaf.get()!=leaf){
        /* leaf no longer in the tree */
        break;
      }
      else{
        /* leaf still in the tree; remove */
        if(cleanup(key,tid)) break;
      }
    }
  }
  clearSeekRecord(seekRecord);
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
optional<V> NatarajanTreeRCSS<K,V,memory_manager,guard_t>::replace(K, V, int){ return {}; }

// TODO: It's unclear whether or not it's better to use marked or snapshot pointers here.
template <class K, class V, template<typename> typename memory_manager, typename guard_t>
std::map<K, V> NatarajanTreeRCSS<K,V,memory_manager,guard_t>::rangeQuery(K key1, K key2, int& len, int tid){
  [[maybe_unused]] guard_t guard;
  
  reportAlloc(tid);
  //NOT HP-like GC safe.
  if(key1>key2) return {};
  Node k1{key1,defltV,nullptr,nullptr};//node to be compared
  Node k2{key2,defltV,nullptr,nullptr};//node to be compared

  marked_snapshot_ptr sptr = s.get_snapshot();
  marked_snapshot_ptr leaf= sptr->left.get_snapshot();
  marked_snapshot_ptr current = leaf->left.get_snapshot();

  std::map<K,V> res;
  if(current)
    doRangeQuery(k1,k2,tid,std::move(current),res);
  len=res.size();
  return res;
}

template <class K, class V, template<typename> typename memory_manager, typename guard_t>
void NatarajanTreeRCSS<K,V,memory_manager,guard_t>::doRangeQuery(Node& k1, Node& k2, int tid, marked_snapshot_ptr root, std::map<K,V>& res){
  marked_snapshot_ptr left = root->left.get_snapshot();
  marked_snapshot_ptr right = root->right.get_snapshot();
  if(!left && !right){
    if(nodeLessEqual(&k1,root.get())&&nodeLessEqual(root.get(),&k2)){
      res.emplace(root->key,root->val);
    }
    return;
  }
  if(left){
    if(nodeLess(&k1,root.get())){
      doRangeQuery(k1,k2,tid,std::move(left),res);
    }
  }
  if(right){
    if(nodeLessEqual(root.get(),&k2)){
      doRangeQuery(k1,k2,tid,std::move(right),res);
    }
  }
  return;
}
#endif
