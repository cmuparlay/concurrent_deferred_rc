README

Here's my test harness for doing concurrent data structure testing.

DEPENDENCIES:
libhwloc

COMPILATION:
This project requires a POSIX compliant OS (e.g. Linux) and C++ 11.
To compile, use:
make

EXECUTION:
To execute, use:
./harness -h

usage: ./harness [-m <test_mode>] [-r <rideable (a data structure)>] [-i <interval>] [-t <num_threads>] [-v] [-h]
Rideable 0 : SGLQUEUE
Test Mode 0 : ENQ_DEQ
Test Mode 1 : EMPTY
Test Mode 2 : FAI

All options use number arguments.  For example, to run the ENQ_DEQ test on the SGLQUEUE
for 3 seconds with 4 threads, use:
./harness -m 0 -q 0 -i 3 -t 4
The displayed number is the operations/second sustained during the test.

The verbose option provides additional (but not much more) output.  Otherwise,
the harness simmply displays the number of total operations completed.

SUMMARY:
The idea of this library is to provide a generic interface to plug in a variety
of simple tests and queues.  To complete this assignment, you'll probably
be fine just adding queues and using the current tests.

DESIGN:
The main objects in the library are GlobalTestConfig and LocalTestConfig.
The GlobalTestConfig (GTC) holds all command line arguments and global
configuration options for the test.  The LocalTestConfig (LTC) holds
all thread specific info (e.g. the thread id).  The GTC is responsible
for parsing the command line and executing the actual test.

The Test subclasses implement the abstract Test inferace to initialize,
execute, and cleanup a test.  The main result of a test is operations executed
after a specified period of time.

The cqueue subclasses are queue implementations.  The provided example,
the single global lock queue (SGLQueue) implements a single lock
around a normal STL queue.  Your implementations will also
implement this interface.

FILES:

harness.cpp - Main execution.  Prints all output.
harnessutils.hpp/cpp - simple utility functions
ParallelLaunch.hpp/cpp - creates and runs threads after appropriate set up
queue_defines.hpp - abstract queue type as well as useful concurrent structures
TestConfig.hpp/cpp - GTC and LTC definitions. 
Tests.hpp/cpp - test implementations.

VALGRIND:

Ok, valgrind gets seriously confused by this code, especially with all the 
bit fiddling that goes on.  If you use harness with valgrind, you'll need the 
following command template to help valgrind sort itself out:

valgrind 
	--leak-check=full 
		(check for all types of leaks)
	--suppressions=../cpp_harness/gccLeaks.supp 
		(suppress some STL "memory leaks" in the harness.  These aren't actual leaks but rather 
		nifty interior pointer usage in the STL objects which confuse valgrind.)
	--leak-check-heuristics=all 
		(suppress additional gcc confusion.  Namely, the STL, new[], and multiple inheritance, all
		of which are used in the harness, use interior pointers.  These heuristics suppress these errors).
	--undef-value-errors=no 
		(Use of undefined memory is usually detected by valgrind.  However, counted pointers, to solve the
		ABA problem, explicitly use undefined memory in the count.  Furthermore, and more importantly, valgrind
		ignores memory values it can trace to uninitialized memory when detecting memory leaks, so the use of
		a counted pointer invalidates the coupled pointer reference attached to the serial number. 
		Thus, forgetting this flag results in all memory in a counted pointer structure to be "leaked")
	./harness
		(The actual program)


The full command is thus:
valgrind --leak-check=full --suppressions=../cpp_harness/gccLeaks.supp --leak-check-heuristics=all --undef-value-errors=no ./harness

valgrind --leak-check=full --leak-check-heuristics=all --undef-value-errors=no ./bin/main
