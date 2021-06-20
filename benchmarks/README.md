# Running the Benchmarks

The repository contains the benchmarks presented in our PLDI paper. They can be reproduced as follows. Note that our benchmarks contain a comparison against a commercial libaray, [just::thread](https://www.stdthread.co.uk/), which will be automatically omitted if not installed.

You should first configure the CMake project and run `make` to build the benchmark executables. See the main [README](../README.md#configuring-the-cmake-project) for how to do this. You want to make sure that the benchmarks are built in Release mode. This is the case by default, but can also be forced by adding `-DCMAKE_BUILD_TYPE=Release` to the CMake configuration command.

## Requirements

* A multicore machine, preferably with a large number (64+) of cores. Fewer will work, but results may vary
* Your system should support [NUMA](https://www.kernel.org/doc/html/latest/admin-guide/mm/numa_memory_policy.html) policy
* [CMake](https://cmake.org/) version 3.5+
* [GCC](https://gcc.gnu.org/) version 9+ or a compatible compiler. We use very recent C++ features (concepts), so many compilers will not be up-to-date enough
* [jemalloc](http://jemalloc.net/)
* [Python3](https://www.python.org/) (with the [Matplotlib](https://matplotlib.org/) library for making the figures)
* [Boost C++ Libraries](https://www.boost.org/) (at least the program-options module)
* [hwloc](https://www.open-mpi.org/projects/hwloc/) (for the SMR benchmark suite)
* [Folly](https://github.com/facebook/folly) (optional -- if you want to benchmark against Folly)
* [just::thread](https://www.stdthread.co.uk/) (optional -- if you want to benchmark against just::thread)

## Reference-counting microbenchmarks (Section 7.1)

From the build directory, Figures 6a--h can be produced by writing `make figures`. To make individual figures rather than all of them, the following `make` targets will produce the corresponding figures (e.g., `make exp-store-load-10-10`):

* 6a: `exp-store-load-10-10`
* 6b: `exp-store-load-10-50`
* 6c: `exp-store-load-10000000-10`
* 6d: `exp-memory-store-load`
* 6e: `exp-stack-10-1`
* 6f: `exp-stack-10-10`
* 6g: `exp-stack-10-50`
* 6h: `exp-memory-stack-size`

These figures will be generated and written to the directory `./results/figures/`.

## Manual SMR benchmarks (Section 7.2)

Scripts for building and running our SMR experiments are stored in `./benchmarks/memory_reclamation`. Figures 7a--f can be produced by executing 

```
python3 run_experiments.py all
```

To generate individual figures rather than all of them, the following arguments can be used in place of 'all' (e.g. 'python3 run_experiments.py exp-list-1000-10'):

* 7a: `exp-list-1000-10`
* 7b: `exp-hashtable-100K-10`
* 7c: `exp-bst-100K-10`
* 7d: `exp-bst-100M-10`
* 7e: `exp-bst-100K-1`
* 7f: `exp-bst-100K-50`

These figures will be generated and written to the directory `./benchmarks/memory_reclamation/graphs`.

## Running custom benchmarks

### Reference-counting microbenchmarks

In order to achieve good performance and reproduce the results of our experiments, all benchmark executables should be ran with the prefix

```
LD_PRELOAD=/usr/local/lib/libjemalloc.so numactl -i all
```

This ensures that jemalloc is loaded as a faster replacement for the system malloc, and that numa settings are configured to interleave memory allocations across nodes.

To run your own custom workloads for the reference-counting benchmarks (raw throughput and concurrent stack benchmarks), you will need to run the `./benchmarks/bench_ref_cnt` and `./benchmarks/bench_stack` executables. 

The arguments for **bench_ref_cnt**, the raw throughput benchmark, are:

* -t, --threads: The number of threads to use
* -s, --size: The number of reference-counted objects to maintain
* -u, --update: The percentage of operations that perform updates (stores)
* -r, --runtime: The number of seconds to run the benchmark
* -i, --iterations: The number of iterations of the benchmark to perform
* -a, --alg: The reference-counting algorithm to use. See below.

Similarly, to run a custom workload for the concurrent stack benchmark, the arguments for **bench_stack** are:

* -t, --threads: The number of threads to use
* -s, --size: The number of stacks to use
* -u, --update: The percentage of operations that perform updates (a push/pop pair)
* -r, --runtime: The number of seconds to run the benchmark
* -i, --iterations: The number of iterations of the benchmark to perform
* -a, --alg: The reference-counting algorithm to use. See below.
* --stack_size: The initial size of each stack

The reference counting algorithms available are:
* `gnu`, which will use libstdc++'s [atomic free functions](https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic)
* `jss`, the [just::threads](https://www.stdthread.co.uk/) library's atomic shared pointer
* `folly`, [Folly's](https://github.com/facebook/folly) atomic shared pointer
* `herlihy`, Our implementation of [Herlihy et al's algorithm](https://dl.acm.org/doi/abs/10.1145/1062247.1062249)
* `weak_atomic`, Our atomic shared pointer implementation, but without snapshotting
* `arc`, Our atomic shared pointer implementation

Note that shapshotting has no effect on the raw throughput benchmark, so `weak_atomic` and `arc` should perform the same. For the concurrent stack benchmark, snapshotting matters, so `weak_atomic` and `arc` will perform differently.


### Manual SMR benchmarks

The SMR benchmarks can be run with different thread counts and workloads. Custom thread counts can be used by modifying the `threads` variable in `run_experiments.py`. Each data structure ('hashtable', 'bst', or 'list') can also be run with different initial sizes and update frequencies using the following command:

```
python3 run_experiments.py exp-[datastructure]-[size]-[update_percent]
```

For example, to run a hashtable initialized with 1000 keys on a workload with 50% updates, you can use `python3 run_experiments.py exp-hashtable-1000-50`. Note that not all workloads are supported. The supported sizes are [100, 1000, 100K, 1M, 10M, 100M] and the supported update frequencies are [1, 10, 50]. Note that the BST experiments may crash when data structure size is small and update frequency is large. This is due to a bug from the IBR benchmark suite which has to do with improper application of HP, HE, and IBR to the Natarajan-Mittal BST. More details on this can be found in Section 8 of our paper.

The runtime and number of iterators can also be changed by changing the `runtime` and `repeats` variables in `run_experiments.py`.
