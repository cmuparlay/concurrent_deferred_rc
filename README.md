# Concurrent Deferred Reference Counting

[![Build Status](https://github.com/cmuparlay/concurrent_deferred_rc/actions/workflows/build.yml/badge.svg)](https://github.com/cmuparlay/concurrent_deferred_rc/actions) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A library for high-performance reference-counted pointers for automatic memory management in C++.

This library is part of the following research project. If you use it for scientific purposes, please cite the corresponding papers:

> **Turning Manual Concurrent Memory Reclamation
into Automatic Reference Counting**<br />
> Daniel Anderson, Guy E. Blelloch, Yuanhao Wei<br />
> The 43rd ACM SIGPLAN Conference on Programming Language Design and Implementation (PLDI 2022), 2022

> **Concurrent Deferred Reference Counting with Constant-Time Overhead**<br />
> Daniel Anderson, Guy E. Blelloch, Yuanhao Wei<br />
> The 42nd ACM SIGPLAN Conference on Programming Language Design and Implementation (PLDI 2021), 2021

## Getting started

To use the library on its own, you can
simply include the `include` directory. The library is header only and has no other dependencies. Our types are provided inside the namespace `cdrc`. You'll need a compiler that supports C++20. The library should work on Linux, MacOS, and Windows. The benchmarks however only work on Linux.

There may be some additional dependencies to run the benchmarks. To work with the tests and benchmarks, you will need a recent version of CMake. Compiling the benchmarks will also require the [Boost](https://www.boost.org/) library, the [hwloc](https://www.open-mpi.org/projects/hwloc/) library, and the [jemalloc](https://github.com/jemalloc/jemalloc) library, but these are not required to just use our library itself. To produce the benchmark figures, you will additionally need a recent version of Python with Matplotlib.

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

### Weak pointers

In addition to the three main pointer types, three additional types are provided for writing more complicated data structures with cyclic references. To enable cyclic references to be collected, **weak pointers** are a kind of smart pointer that hold a reference to a shared object, but do not contribute to its reference count. Since they do not contribute to the reference count, they can not be read directly, but must instead be updgraded to an `rc_ptr` before they can be dereferrenced. We also provide a `weak_snapshot_ptr` type, which is analagous to `snapshot_ptr`, which enables reading from an `atomic_weak_ptr` without incrementing the reference count. The types, in summary, are:

* **atomic_weak_ptr**: `atomic_weak_ptr<T>` is analagous to `atomic_rc_ptr<T>` and is modelled after C++'s `atomic<weak_ptr<T>>`. It supports the same operations as `atomic_rc_ptr` for storing instances of `weak_ptr`, including `load()`, `get_snapshot()`, `store(desired)`, `compare_and_swap(expected, desired)`, and `compare_exchange_weak(expected, desired)`. Note that `get_snapshot` returns a `weak_snapshot_ptr` rather than a `snapshot_ptr`.
* **weak_ptr**: `weak_ptr<T>` is modelled after C++'s `weak_ptr<T>` and supports the same interface. Note that `weak_ptr` can not be dereferrenced or read directly. To read a `weak_ptr`, it must be converted into an `rc_ptr` by calling its `lock()` method. `weak_ptr` can conversely be constructed from an instance of `rc_ptr`.
* **weak_snapshot_ptr**: `weak_snapshot_ptr<T>` is analagous to `snapshot_ptr` for `atomic_weak_ptr`. Note that it is possible for the reference count of an object to reach zero while holding a `weak_snapshot_ptr` that refers to it. However, it is guaranteed that the object will not be destroyed and hence is safe to read as long as the snapshot is held.

These types are provided in the corresponding headers `<cdrc/atomic_weak_ptr.h>`, `<cdrc/weak_ptr.h>`, `<cdrc/weak_snapshot_ptr.h>`.

### Marked pointers

To support writing advanced data structures, we also support *marked* pointers. Marked pointers allow you to utilize some of the redundant bits of the pointer representation to store flags, avoiding the need to store flags adjacent to the pointer and having to use double-word instructions to read/write the pointer/flag pair atomically. To use marked pointers, the types `marked_arc_ptr`, `marked_rc_ptr`, and `marked_snapshot_ptr` can be used as substitutes for `atomic_rc_ptr`, `rc_ptr`, and `snapshot_ptr` respectively. There are also marked versions of the three weak pointer types, `marked_aw_ptr`, `marked_weak_ptr`, and `marked_ws_ptr`, for `atomic_weak_ptr`, `weak_ptr`, and `weak_snapshot_ptr` respectively. You can include them all from the header `<cdrc/marked_arc_ptr.h>` They support all of the same methods, but additionally support:
* `get_mark()`. Gets the current mark on the pointer.
* `set_mark(mark)`. Sets the current mark on the pointer. The mark must be an unsigned integer at most 3, i.e., use only the bottom two bits. 

Like regular `rc_ptr`, to create a new reference-counted object managed by a `marked_rc_ptr`, use `marked_rc_ptr<T>::make_shared(args...)`.

As expected, the `load` method of `marked_arc_ptr` returns a `marked_rc_ptr`, and the `get_snapshot` method returns a `marked_snapshot_ptr`. The `get` method of `marked_rc_ptr` and `marked_snapshot_ptr` returns returns a raw pointer **without** the mark, and hence can be safely dereferenced.

Lastly, the `marked_arc_ptr` and `marked_aw_ptr` types also support these additional operations:
* `compare_and_set_mark(expected, desired_mark)`. Atomically compares the current marked pointer with expected (which should be a `marked_rc_ptr` or a `marked_snapshot_ptr`), and, if they are equal, sets the mark to `desired_mark` and returns true. Otherwise returns false.
* `set_mark_bit(bit)`. Atomically sets the value of the bit at the given position (must be 1 or 2) to 1
* `get_mark_bit(bit)`. Returns the value of the mark bit at the given position (must be 1 or 2)

An example of how to use these marked pointers can be found in [linked_list.h](./examples/linked_list.h) in the [examples](./examples) directory.

## Using different memory management backends

CDRC can be configured to use different memory management algorithms under the hood, which can result in different performance profiles. By default, it uses the hazard-pointer backend, which has good performance and bounded garbage accumulation. There are four backends available to choose from, summarized in the following table.

| Scheme                           | Throughput | Memory usage |
|----------------------------------| -----------| ------------ |
| Hazard-pointers (default)        | Moderate | Low |
| Epoch-based reclamation (EBR)    | High | High |
| Interval-based reclamation (IBR) | Moderate-high | Moderate-high |
| Hyaline                          | High | Moderate-high |

### Guard types

For every backend other than the default (hazard-pointers), an additional tool is required to safely use the smart pointer types. Before performing any potentially concurrent read or write to an atomic pointer type, the user must first acquire a **guard** object. For EBR and IBR, the guard object is of type ``cdrc::epoch_guard``. For Hyaline, the guard object is of type ``cdrc::hyaline_guard``. For example, using EBR, the `pop_front` method of our example stack becomes

```c++
std::optional<T> pop_front() {
  epoch_guard g;  // This is important!!
  snapshot_ptr_ebr<Node> p = head.get_snapshot();
  while (p != nullptr && !head.compare_exchange_weak(p, p->next)) { }
  if (p != nullptr) return {p->t};
  else return {};
}
```

The guard is released automatically at the end of the enclosing scope. Note that snapshot pointers cannot outlive the guard that they were created during. It is safe to hold multiple nested guards inside nested scopes. Guards should not be held for long periods of time, as they may delay memory reclamation and lead to the accumulation of more garbage. Ideally, the lifetime of a guard should denote the span of a single operation on the data structure.


### Selecting an alternate backend

There are two ways to select an alternate memory management algorithm. One is to provide an additional template argument to the pointer types. For example, to declare an `atomic_rc_ptr` that uses EBR as the backend, we can write

```c++
cdrc::atomic_r_ptr<int, cdrc::ebr_backend<int>> p;
```

Note that this extra template argument applies to all six of the pointer types, and pointers with different memory management backends are not compatible (e.g., you can not store an `rc_ptr<T, ebr_backend<T>>` inside an `atomic_rc_ptr<T, hp_backend<T>>`).

Alternatively, a set of predefined alias templates for each of the pointer types with each backend are available. Each alias corresponds to the original pointer type with a suffix indicating the backend, e.g., `atomic_rc_ptr_ebr<T>` is an alias template for `atomic_rc_ptr<T, ebr_backend<T>>`. The full list of backend types and suffixes is as follows

| Backend | Type                       | Suffix | Guard type |
| ------- |----------------------------| ------ | ---------- |
| Hazard-pointers | `cdrc::hp_backend<T>`      | `_hp` | None |
| EBR | `cdrc::ebr_backend<T>`     | `_ebr` | `cdrc::epoch_guard` |
| IBR | `cdrc::ibr_backend<T>`     | `_ibr` | `cdrc::epoch_guard` |
| Hyaline | `cdrc::hyaline_backend<T>` | `_hyaline` | `cdrc::hyaline_guard` |

Note that the marked pointer alias templates also support both the additional template argument to select a backend, and the suffixed template alises, e.g., `marked_aw_ptr<T, cdrc::ebr_backend<T>>` and `marked_aw_ptr_ebr<T>` are valid and equivalent.

## Configuring the CMake project for testing and benchmarking

To configure the project for testing and benchmarking, create a build directory and run CMake. This is as easy as

```
mkdir build
cd build
cmake ..
make
```

This will build both the benchmarks and the tests. By default, the CMake project will build in Release mode. You probably want to do a Debug build for testing, by adding `-DCMAKE_BUILD_TYPE=Debug` to the CMake configuration command. The tests can then be run by writing `make test` from the build directory. These will validate that the code is functioning sensibly. See [benchmarks](benchmarks/README.md) for information on running the provided benchmarks.
