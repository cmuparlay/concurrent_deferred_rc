
#include <limits>
#include <utility>

#include <cdrc/marked_arc_ptr.h>

namespace cdrc {

// An implementation of a concurrent sorted linked list using marked pointers
// from our CDRC library. This implementation is based on Harris's Linked List:
// https://www.microsoft.com/en-us/research/wp-content/uploads/2001/10/2001-disc.pdf
class atomic_linked_list {

  struct Node;

  using atomic_sp_t = marked_arc_ptr<Node>;
  using sp_t = marked_rc_ptr<Node>;
  using snapshot_ptr_t = marked_snapshot_ptr<Node>;

  struct Node {
    int key;
    atomic_sp_t next;
    Node(int k) : key(k), next(nullptr) {}
    Node(int k, sp_t next) : key(k), next(std::move(next)) {}
  };

public:
  atomic_linked_list() : tail(sp_t::make_shared(std::numeric_limits<int>::max())),
                         head(sp_t::make_shared(std::numeric_limits<int>::lowest(), tail))
                         {}

  // Looks for key in list
  bool find(int key) {
    auto [left, right] = search(key);
    return right->key == key;
  }

  // Inserts key if it is not in the list.
  bool insert(int key) {
    auto [left, right] = search(key);
    if(right->key == key) return false; // key is in list
    sp_t new_node = sp_t::make_shared(key, right);
    if(left->next.compare_and_swap(right, new_node)) 
      return true;
    else return insert(key); // try again
  }

  // Removes key if it is in the list.
  bool remove(int key) {
    auto [left, right] = search(key);
    if(right->key != key) return false; // key is not in list
    snapshot_ptr_t next = right->next.get_snapshot();
    if(next.get_mark() == 0 && 
      right->next.compare_and_set_mark(next, 1)) {
      if(!left->next.compare_and_swap(right, next))
        search(key);
      return true;
    } else return remove(key); // try again
  }

private:
  atomic_sp_t tail, head;

  std::pair<snapshot_ptr_t, snapshot_ptr_t> search(int key) {
    snapshot_ptr_t right = head.get_snapshot();
    snapshot_ptr_t right_nxt = right->next.get_snapshot();
    snapshot_ptr_t left, left_nxt;

    // find left and right nodes
    do {
      if(right_nxt.get_mark() == 0) {
        left = std::move(right);
        left_nxt = nullptr;
      } else if (left_nxt == nullptr)
        left_nxt = std::move(right);
      right = std::move(right_nxt); // unmark(nxt)
      right.set_mark(0);
      right_nxt = right->next.get_snapshot();
    } while(right_nxt.get_mark() != 0 || right->key < key);
    
    // check nodes are adjacent
    // if not, remove one or more marked nodes
    if(left_nxt == nullptr ||
      left->next.compare_and_swap(left_nxt, right)) {
      if(right->next.get_mark() == 0)
        return std::make_pair<snapshot_ptr_t, snapshot_ptr_t>(
          std::move(left), std::move(right));
    } 
    return search(key); // try again
  }
};

} // namespace cdrc