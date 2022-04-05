# Concurrent Deferred Reference Counting

A library for high-performance reference-counted pointers for automatic memory management.

This library is part of the following research project. If you use it for scientific purposes, please cite it as follows:

> **Concurrent Deferred Reference Counting with Constant-Time Overhead**<br />
> Daniel Anderson, Guy E. Blelloch, Yuanhao Wei<br />
> The 42nd ACM SIGPLAN Conference on Programming Language Design and Implementation (PLDI 2021), 2021

## Getting started

To work with the tests and benchmarks, you will need a recent version of CMake. To use the library on its own, you can
simply include the `include` directory. The library is header only and has no other dependencies. Our types are provided inside the namespace `cdrc`.
You'll need a compiler that supports at least C++17. Some of our tests and benchmarks require C++20, but the library
itself should work with just C++17.

### Using the library

Our library provides three types that work similarly to C++'s standard [`shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr) and [`atomic<shared_ptr>`](https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2). These types, and some of their important supported methods are:

* **atomic_rc_ptr**: `atomic_rc_ptr<T>` is closely modelled after C++’s `atomic<shared_ptr<T>>`. It provides support for all of the standard operations, such as atomic load, store, exchange and CAS.
    * `load()`. Atomically creates an `rc_ptr` to the currently managed object, returning the rc_ptr
    * `get_snapshot()`. Atomically creates a `snapshot_ptr` to the currently managed object, returning the `snapshot_ptr`.
    * `store(desired)`. Atomically replaces the currently managed pointer with desired, which may be either an `rc_ptr` or a `snapshot_ptr`.
    * `compare_and_swap(expected, desired)`. Atomically compares the managed pointer with expected, and if they are equal, replaces the managed pointer with desired. The types of expected and desired may be either `rc_ptr` or `snapshot_ptr`, and need not be the same. If `desired` is passed in as an r-value reference, then it is only moved from if the compare_and_swap succeeds.
    * `compare_exchange_weak(expected, desired)`. Same as `compare_and_swap` but, if the managed pointer is not equal to expected, loads the currently managed pointer into expected. This operation may spuriously return false, i.e. it is possible that the value of expected does not change
* **rc_ptr**: The `rc_ptr` type is closely modelled after C++’s standard library `shared_ptr`. It supports all
pointer-like operations, such as dereferencing, i.e. obtaining
a reference to the underlying managed object, and assignment of another `rc_ptr` to replace the current one. It is safe to read/copy an `rc_ptr` concurrently from many threads, as long as there is never a race between one thread updating the `rc_ptr` and another reading it. Such a situation should be handled by an `atomic_rc_ptr`.
* **snapshot_ptr**: The `snapshot_ptr` type supports all of the same operations as `rc_ptr`, except that `snapshot_ptr` can only be moved and not copied. Additionally, while `rc_ptr` can safely be shared between threads, `snapshot_ptr` should only be used locally by the thread that created it. The use of `snapshot_ptr` should result in better performance than `rc_ptr` provided that each thread does not hold too many `snapshot_ptr` at once. Therefore, `snapshot_ptr` is ideal for reading short-lived local references, for example, reading nodes in a data structure while traversing it.

These types are provided in the correspondingly named header files: `<cdrc/atomic_rc_ptr.h>`, `<cdrc/rc_ptr.h>`, and `<cdrc/snapshot_ptr.h>`.

To allocate a new reference-counted object mananged by an `rc_ptr`, use the static method `rc_ptr<T>::make_shared(args...)` in the same way as the standard C++ `std::make_shared`. Alternatively, the convenient aliases `cdrc::make_rc<T>(args...)` and `cdrc::make_shared<T>(args...)` can be used.

As a simple example, the following code implements a concurrent stack using our library. You can read a complete implementation in [stack.h](./examples/stack.h) in the [examples](./examples) directory.

```
struct Node { T t; rc_ptr<Node> next; }
atomic_rc_ptr<Node> head;

void push_front(T t) {
  rc_ptr<Node> p = make_rc<Node>(t, head.load());
  while (!head.compare_exchange_weak(p->next, p)) {}
}

std::optional<T> pop_front() {
  snapshot_ptr<Node> p = head.get_snapshot();
  while (p != nullptr && !head.compare_exchange_weak(p, p->next)) { }
  if (p != nullptr) return {p->t};
  else return {};
}
```

The head node of the
stack is stored in an `atomic_rc_ptr` because it may be modified and read concurrently by multiple threads. Each node
of the stack stores its next pointer as a non-atomic `rc_ptr`.
This is safe, because although multiple threads may read the
same pointer concurrently, the internal nodes of the stack
are never modified, only the head is. Lastly, we can use a
`snapshot_ptr` while performing `pop_front`, since reading the
head is a short-lived local reference that will never be shared
with another thread.

### Marked pointers

To support writing advanced data structures, we also support *marked* pointers. Marked pointers allow you to utilize some of the redundant bits of the pointer representation to store flags, avoiding the need to store flags adjacent to the pointer and having to use double-word instructions to read/write the pointer/flag pair atomically. To use marked pointers, the types `marked_arc_ptr`, `marked_rc_ptr`, and `marked_snapshot_ptr` can be used as substitutes for `atomic_rc_ptr`, `rc_ptr`, and `snapshot_ptr` respectively. You can include them from the header `<cdrc/marked_arc_ptr.h>` They support all of the same methods, but additionally support:
* `get_mark()`. Gets the current mark on the pointer.
* `set_mark(mark)`. Sets the current mark on the pointer. The mark must be an unsigned integer at most 3, i.e., use only the bottom two bits. 

Like regular `rc_ptr`, to create a new reference-counted object managed by a `marked_rc_ptr`, use `marked_rc_ptr<T>::make_shared(args...)`.

As expected, the `load` method of `marked_arc_ptr` returns a `marked_rc_ptr`, and the `get_snapshot` method returns a `marked_snapshot_ptr`. The `get` method of `marked_rc_ptr` and `marked_snapshot_ptr` returns returns a raw pointer **without** the mark, and hence can be safely dereferenced.

Lastly, the `marked_arc_ptr` type also supports these additional operations:
* `compare_and_set_mark(expected, desired_mark)`. Atomically compares the current marked pointer with expected (which should be a `marked_rc_ptr` or a `marked_snapshot_ptr`), and, if they are equal, sets the mark to `desired_mark` and returns true. Otherwise returns false.
* `set_mark_bit(bit)`. Atomically sets the value of the bit at the given position (must be 1 or 2) to 1
* `get_mark_bit(bit)`. Returns the value of the mark bit at the given position (must be 1 or 2)

An example of how to use these marked pointers can be found in [linked_list.h](./examples/linked_list.h) in the [examples](./examples) directory.

## Configuring the CMake project for testing and benchmarking

To configure the project for testing and benchmarking, create a build directory and run cmake. This is as easy as

```
mkdir build
cd build
cmake ..
make
```

This will build both the benchmarks and the tests. By default, the CMake project will build in Release mode. You probably want to do a Debug build for testing, by adding `-DCMAKE_BUILD_TYPE=Debug` to the CMake configuration command. The tests can then be run by writing `make test` from the build directory. These will validate that the code is functioning sensibly. See [benchmarks](benchmarks/README.md) for information on running the provided benchmarks.


