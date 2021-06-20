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


#ifndef NATARAJAN_TREE
#define NATARAJAN_TREE

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

template <class K, class V>
class NatarajanTree : public ROrderedMap<K,V>, public RetiredMonitorable{
private:
	/* structs*/
	struct Node{
		int level;
		K key;
		V val;
		std::atomic<Node*> left;
		std::atomic<Node*> right;

		virtual ~Node(){};
		static Node* alloc(K k, V v, Node* l, Node* r,MemoryTracker<Node>* memory_tracker,int tid){//TODO: Switch this func to MemoryTracker one
			return alloc(k,v,l,r,-1,memory_tracker, tid);
		}

		inline bool deletable() {return true;}
		static Node* alloc(K k, V v, Node* l, Node* r, int lev, MemoryTracker<Node>* memory_tracker, int tid){
			while(true){
				Node* n = (Node*) memory_tracker->alloc(tid);
				if (n==nullptr) {
					// std::cout<<"alloc fails\n";
					continue;//return alloc(a,k,v,l,r,lev);
				}
				//don't know how to call constructor for malloc
				//http://stackoverflow.com/questions/2995099/malloc-and-constructors
				new (n) Node(k,v,l,r,lev);
				return n;
			}
		}
		Node(){};
		Node(K k, V v, Node* l, Node* r,int lev):level(lev),key(k),val(v),left(l),right(r){};
		Node(K k, V v, Node* l, Node* r):level(-1),key(k),val(v),left(l),right(r){};
	};
	struct SeekRecord{
		Node* ancestor;
		Node* successor;
		Node* parent;
		Node* leaf;
	};

	/* variables */
	MemoryTracker<Node>* memory_tracker;
	K infK{};
	V defltV{};
	// Node r{infK,defltV,nullptr,nullptr,2};
	// Node s{infK,defltV,nullptr,nullptr,1};
	Node* r;
	Node* s;
	padded<SeekRecord>* records;
	padded<uint64_t>* counter;
	const size_t GET_POINTER_BITS = 0xfffffffffffffffc;//for machine 64-bit or less.

	/* helper functions */
	//flag and tags helpers
	inline Node* getPtr(Node* mptr){
		return (Node*) ((size_t)mptr & GET_POINTER_BITS);
	}
	inline bool getFlg(Node* mptr){
		return (bool)((size_t)mptr & 1);
	}
	inline bool getTg(Node* mptr){
		return (bool)((size_t)mptr & 2);
	}
	inline Node* mixPtrFlgTg(Node* ptr, bool flg, bool tg){
		return (Node*) ((size_t)ptr | flg | ((size_t)tg<<1));
	}
	//node comparison
	inline bool isInf(Node* n){
		return getInfLevel(n)!=-1;
	}
	inline int getInfLevel(Node* n){
		//0 for inf0, 1 for inf1, 2 for inf2, -1 for general val
		n=getPtr(n);
		return n->level;
	}
	inline bool nodeLess(Node* n1, Node* n2){
		n1=getPtr(n1);
		n2=getPtr(n2);
		int i1=getInfLevel(n1);
		int i2=getInfLevel(n2);
		return i1<i2 || (i1==-1&&i2==-1&&n1->key<n2->key);
	}
	inline bool nodeEqual(Node* n1, Node* n2){
		n1=getPtr(n1);
		n2=getPtr(n2);
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

	/* private interfaces */
	void seekLeaf(K key, int tid);
	void seek(K key, int tid);
	bool cleanup(K key, int tid);
	void doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res);
	void destructTree(Node* n) {
		if(n == nullptr) return;
		destructTree(n->left);
		destructTree(n->right);
		memory_tracker->reclaim(n);
	}

	void reportAlloc(int tid) {
		// counter[tid].ui++;
		// if(counter[tid].ui == 4000) {
		// 	counter[tid].ui = 0;
		// 	collect_retired_size(memory_tracker->get_allocated(), tid);
		// }
	}

  int64_t get_allocated() {
    return memory_tracker->get_allocated();
  }

	uint64_t size(){
		return 0;
	}

public:
	NatarajanTree(GlobalTestConfig* gtc): RetiredMonitorable(gtc)
	//memory_tracker("HE",gtc->task_num,150,200,5,COLLECT)
	{
		counter = new padded<uint64_t>[gtc->task_num];
		for(int i = 0; i < gtc->task_num; i++)
			counter[i] = 0;

		//std::string type = gtc->getEnv("tracker").empty()? "RCU":gtc->getEnv("tracker");
		//memory_tracker = new MemoryTracker<Node>(type, gtc->task_num, 150, 30, 5, COLLECT);
    int epochf = gtc->getEnv("epochf").empty()? 150:stoi(gtc->getEnv("epochf"));
    int emptyf = gtc->getEnv("emptyf").empty()? 30:stoi(gtc->getEnv("emptyf"));
    std::cout << "emptyf: " << emptyf << std::endl;
    // int emptyf = 1;
		memory_tracker = new MemoryTracker<Node>(gtc, epochf, emptyf, 5, COLLECT);
		r = Node::alloc(infK,defltV,nullptr,nullptr,2,memory_tracker,0);
		s = Node::alloc(infK,defltV,nullptr,nullptr,1,memory_tracker,0);
		r->right = Node::alloc(infK,defltV,nullptr,nullptr,2,memory_tracker,0);
		r->left = s;
		s->right = Node::alloc(infK,defltV,nullptr,nullptr,1,memory_tracker,0);
		s->left = Node::alloc(infK,defltV,nullptr,nullptr,0,memory_tracker,0);
		records = new padded<SeekRecord>[gtc->task_num]{};
	};
	~NatarajanTree(){
		#ifdef NO_DESTRUCT
			return;
		#endif
		destructTree(r);
		delete memory_tracker;
		delete[] records;
		delete[] counter;
	};

	optional<V> get(K key, int tid);
	optional<V> put(K key, V val, int tid);
	bool insert(K key, V val, int tid);
	optional<V> remove(K key, int tid);
	optional<V> replace(K key, V val, int tid);
	std::map<K, V> rangeQuery(K key1, K key2, int& len, int tid);
};

template <class K, class V> 
class NatarajanTreeFactory : public RideableFactory{
	NatarajanTree<K,V>* build(GlobalTestConfig* gtc){
		return new NatarajanTree<K,V>(gtc);
	}
};

//-------Definition----------
template <class K, class V>
void NatarajanTree<K,V>::seekLeaf(K key, int tid){
	// seek_count++;
	/* initialize the seek record using sentinel nodes */
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	SeekRecord* seekRecord=&(records[tid].ui);
	seekRecord->leaf=getPtr(memory_tracker->read(s->left,3,tid,s));

	/* initialize other variables used in the traversal */
	Node* currentField=memory_tracker->read(seekRecord->leaf->left,4,tid,seekRecord->leaf);
	Node* current=getPtr(currentField);

	/* traverse the tree */
	while(current!=nullptr){
		seekRecord->leaf=current;
		memory_tracker->transfer(4,3,tid);

		/* update other variables used in traversal */
		if(nodeLess(&keyNode,current)){
			currentField=memory_tracker->read(current->left,4,tid,current);
		}
		else{
			currentField=memory_tracker->read(current->right,4,tid,current);
		}
		current=getPtr(currentField);
	}
	/* traversal complete */
	return;
}

//-------Definition----------
template <class K, class V>
void NatarajanTree<K,V>::seek(K key, int tid){
	// seek_count++;
	/* initialize the seek record using sentinel nodes */
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	SeekRecord* seekRecord=&(records[tid].ui);
	seekRecord->ancestor=r;
	seekRecord->successor=memory_tracker->read(r->left,1,tid,r);
	seekRecord->parent=memory_tracker->read(r->left,2,tid,r);
	seekRecord->leaf=getPtr(memory_tracker->read(s->left,3,tid,s));

	/* initialize other variables used in the traversal */
	Node* parentField=memory_tracker->read(seekRecord->parent->left,3,tid,seekRecord->parent);
	Node* currentField=memory_tracker->read(seekRecord->leaf->left,4,tid,seekRecord->leaf);
	Node* current=getPtr(currentField);

	/* traverse the tree */
	while(current!=nullptr){
		/* check if the edge from the current parent node is tagged */
		if(!getTg(parentField)){
			// if(seek_count == 100) {
			// 	usleep(100);
			// }
			/* 
			 * found an untagged edge in the access path;
			 * advance ancestor and successor pointers.
			 */
			seekRecord->ancestor=seekRecord->parent;
			memory_tracker->transfer(1,0,tid);
			seekRecord->successor=seekRecord->leaf;
			memory_tracker->copy(3,1,tid);
		}

		/* advance parent and leaf pointers */
		seekRecord->parent=seekRecord->leaf;
		memory_tracker->transfer(3,2,tid);
		seekRecord->leaf=current;
		memory_tracker->transfer(4,3,tid);

		/* update other variables used in traversal */
		parentField=currentField;
		if(nodeLess(&keyNode,current)){
			currentField=memory_tracker->read(current->left,4,tid,current);
		}
		else{
			currentField=memory_tracker->read(current->right,4,tid,current);
		}
		current=getPtr(currentField);
	}
	/* traversal complete */
	return;
}

template <class K, class V>
bool NatarajanTree<K,V>::cleanup(K key, int tid){
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	bool res=false;

	/* retrieve addresses stored in seek record */
	SeekRecord* seekRecord=&(records[tid].ui);
	Node* ancestor=getPtr(seekRecord->ancestor);
	Node* successor=getPtr(seekRecord->successor);
	Node* parent=getPtr(seekRecord->parent);
	Node* leaf=getPtr(seekRecord->leaf);

	std::atomic<Node*>* successorAddr=nullptr;
	std::atomic<Node*>* childAddr=nullptr;
	std::atomic<Node*>* siblingAddr=nullptr;

	/* obtain address of field of ancestor node that will be modified */
	if(nodeLess(&keyNode,ancestor))
		successorAddr=&(ancestor->left);
	else
		successorAddr=&(ancestor->right);

	/* obtain addresses of child fields of parent node */
	if(nodeLess(&keyNode,parent)){
		childAddr=&(parent->left);
		siblingAddr=&(parent->right);
	}
	else{
		childAddr=&(parent->right);
		siblingAddr=&(parent->left);
	}
	Node* tmpChild=childAddr->load(std::memory_order_acquire);
	if(!getFlg(tmpChild)){
		/* the leaf is not flagged, thus sibling node should be flagged */
		tmpChild=siblingAddr->load(std::memory_order_acquire);
		/* switch the sibling address */
		siblingAddr=childAddr;
	}

	/* use TAS to tag sibling edge */
	while(true){
		Node* untagged=siblingAddr->load(std::memory_order_acquire);
		Node* tagged=mixPtrFlgTg(getPtr(untagged),getFlg(untagged),true);
		if(siblingAddr->compare_exchange_strong(untagged,tagged,std::memory_order_acq_rel)){
			break;
		}
	}
	/* read the flag and address fields */
	Node* tmpSibling=siblingAddr->load(std::memory_order_acquire);

	/* make the sibling node a direct child of the ancestor node */
	res=successorAddr->compare_exchange_strong(successor,
		mixPtrFlgTg(getPtr(tmpSibling),getFlg(tmpSibling),false),
		std::memory_order_acq_rel);

	if(res==true){
		while(successor != parent) {
			Node* left = successor->left;
			Node* right = successor->right;
			memory_tracker->retire(successor,tid);
			if(getFlg(left)) {
				memory_tracker->retire(getPtr(left),tid);
				successor = getPtr(right);
			} else {
				memory_tracker->retire(getPtr(right),tid);
				successor = getPtr(left);
			}
		}
		memory_tracker->retire(getPtr(tmpChild),tid);
		memory_tracker->retire(successor,tid);
		// memory_tracker->retire(successor,tid);
	}
	return res;
}

/* to test rangeQuery */
// template <>
// optional<int> NatarajanTree<int,int>::get(int key, int tid){
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

template <class K, class V>
optional<V> NatarajanTree<K,V>::get(K key, int tid){
	reportAlloc(tid);
	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);
	Node* leaf=nullptr;
	// collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);
	memory_tracker->start_op(tid);
	seekLeaf(key,tid);
	leaf=getPtr(seekRecord->leaf);
	if(nodeEqual(&keyNode,leaf)){
		res = leaf->val;
	}
	memory_tracker->clear_all(tid);
	memory_tracker->end_op(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTree<K,V>::put(K key, V val, int tid){ return {}; }

template <class K, class V>
bool NatarajanTree<K,V>::insert(K key, V val, int tid){
	reportAlloc(tid);
	bool res=false;
	SeekRecord* seekRecord=&(records[tid].ui);
	
	Node* newInternal=nullptr;
	Node* newLeaf=Node::alloc(key,val,nullptr,nullptr,memory_tracker,tid);//also for comparing keys
	
	Node* parent=nullptr;
	Node* leaf=nullptr;
	std::atomic<Node*>* childAddr=nullptr;
	// collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);
	memory_tracker->start_op(tid);
	while(true){
		seek(key,tid);
		leaf=getPtr(seekRecord->leaf);
		parent=getPtr(seekRecord->parent);
		if(!nodeEqual(newLeaf,leaf)){//key does not exist
			/* obtain address of the child field to be modified */
			if(nodeLess(newLeaf,parent))
				childAddr=&(parent->left);
			else
				childAddr=&(parent->right);

			/* create left and right leave of newInternal */
			Node* newLeft=nullptr;
			Node* newRight=nullptr;
			if(nodeLess(newLeaf,leaf)){
				newLeft=newLeaf;
				newRight=leaf;
			}
			else{
				newLeft=leaf;
				newRight=newLeaf;
			}

			/* create newInternal */
			if(isInf(leaf)){
				int lev=getInfLevel(leaf);
				newInternal=Node::alloc(infK,defltV,newLeft,newRight,lev,memory_tracker,tid);
			}
			else
				newInternal=Node::alloc(std::max(key,leaf->key),defltV,newLeft,newRight,memory_tracker,tid);

			/* try to add the new nodes to the tree */
			Node* tmpExpected=getPtr(leaf);
			if(childAddr->compare_exchange_strong(tmpExpected,getPtr(newInternal),std::memory_order_acq_rel)){
				res=true;
				break;//insertion succeeds
			}
			else{//fails; help conflicting delete operation
				memory_tracker->reclaim(newInternal, tid);
				Node* tmpChild=childAddr->load(std::memory_order_acquire);
				if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
					/* 
					 * address of the child has not changed
					 * and either the leaf node or its sibling 
					 * has been flagged for deletion
					 */
					cleanup(key,tid);
				}
			}
		}
		else{//key exists, insertion fails
			memory_tracker->reclaim(newLeaf, tid);
			res=false;
			break;
		}
	}
	memory_tracker->clear_all(tid);
	memory_tracker->end_op(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTree<K,V>::remove(K key, int tid){
	reportAlloc(tid);
	bool injecting = true;
	optional<V> res={};
	SeekRecord* seekRecord=&(records[tid].ui);

	Node keyNode{key,defltV,nullptr,nullptr};//node to be compared
	
	Node* parent=nullptr;
	Node* leaf=nullptr;
	std::atomic<Node*>* childAddr=nullptr;
	// collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);
	memory_tracker->start_op(tid);
	while(true){
		seek(key,tid);
		parent=getPtr(seekRecord->parent);
		/* obtain address of the child field to be modified */
		if(nodeLess(&keyNode,parent))
			childAddr=&(parent->left);
		else
			childAddr=&(parent->right);

		if(injecting){
			/* injection mode: check if the key exists */
			leaf=getPtr(seekRecord->leaf);
			if(!nodeEqual(leaf,&keyNode)){//does not exist
				res={};
				break;
			}

			/* inject the delete operation into the tree */
			Node* tmpExpected=getPtr(leaf);
			res=leaf->val;
			if(childAddr->compare_exchange_strong(tmpExpected,
				mixPtrFlgTg(tmpExpected,true,false), std::memory_order_acq_rel)){
				/* advance to cleanup mode to remove the leaf node */
				injecting=false;
				if(cleanup(key,tid)) break;
			}
			else{
				Node* tmpChild=childAddr->load(std::memory_order_acquire);
				if(getPtr(tmpChild)==leaf && (getFlg(tmpChild)||getTg(tmpChild))){
					/*
					 * address of the child has not 
					 * changed and either the leaf
					 * node or its sibling has been
					 * flagged for deletion
					 */
					cleanup(key,tid);
				}
			}
		}
		else{
			/* cleanup mode: check if flagged node still exists */
			if(seekRecord->leaf!=leaf){
				/* leaf no longer in the tree */
				break;
			}
			else{
				/* leaf still in the tree; remove */
				if(cleanup(key,tid)) break;
			}
		}
	}
	memory_tracker->clear_all(tid);
	memory_tracker->end_op(tid);
	return res;
}

template <class K, class V>
optional<V> NatarajanTree<K,V>::replace(K key, V val, int tid){ return {}; }

template <class K, class V>
std::map<K, V> NatarajanTree<K,V>::rangeQuery(K key1, K key2, int& len, int tid){
	reportAlloc(tid);
	//NOT HP-like GC safe.
	if(key1>key2) return {};
	Node k1{key1,defltV,nullptr,nullptr};//node to be compared
	Node k2{key2,defltV,nullptr,nullptr};//node to be compared

	// collect_retired_size(memory_tracker->get_retired_cnt(tid), tid);
	memory_tracker->start_op(tid);
	
	Node* leaf=getPtr(memory_tracker->read(s->left,0,tid,s));
	Node* current=getPtr(memory_tracker->read(leaf->left,1,tid,leaf));

	std::map<K,V> res;
	if(current!=nullptr)
		doRangeQuery(k1,k2,tid,current,res);
	len=res.size();
	memory_tracker->end_op(tid);
	return res;
}

template <class K, class V>
void NatarajanTree<K,V>::doRangeQuery(Node& k1, Node& k2, int tid, Node* root, std::map<K,V>& res){
	Node* left=getPtr(memory_tracker->read(root->left,2,tid,root));
	Node* right=getPtr(memory_tracker->read(root->right,3,tid,root));
	if(left==nullptr&&right==nullptr){
		if(nodeLessEqual(&k1,root)&&nodeLessEqual(root,&k2)){
			
			res.emplace(root->key,root->val);
		}
		return;
	}
	if(left!=nullptr){
		if(nodeLess(&k1,root)){
			doRangeQuery(k1,k2,tid,left,res);
		}
	}
	if(right!=nullptr){
		if(nodeLessEqual(root,&k2)){
			doRangeQuery(k1,k2,tid,right,res);
		}
	}
	return;
}
#endif
