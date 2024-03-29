# CMake targets to produce figures for the paper
include(ProcessorCount)

set(FIGURE_SCRIPT ${CMAKE_SOURCE_DIR}/results/plot_graphs.py)

# Target that builds all figures
add_custom_target(figures)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/figures)

# Number of processors in the system
ProcessorCount(P)

# Add a new figure target that measures throughput or number of allocations vs number of threads
#
# Usage:
#   add_figure(
#     NAME            figure name
#     BENCHMARK       benchmark name
#     THREADS         comma separated list of thread counts
#     ALGORITHMS      comma separated list of algorithms
#     LABELS          comma separated list of labels for the algorithms
#     COLORS          comma separated list of colors for the algorithms
#     MARKERS         comma separated list of markers for the algorithms
#     BENCHMARK_ARGS  additional arguments to pass to the benchmark executable
#     FIGURE_ARGS     additional arguments to pass to the figure generation script
#     LEGEND          the name of the legend to generate (OPTIONAL)
#     THROUGHPUT      measure the throughput (OPTIONAL - choose at most one of this or ALLOCATED)
#     ALLOCATED       measure the number of allocated objects (OPTIONAL - choose at most one of this or THROUGHPUT)
#   )
#
# Note that if you need to include spaces in one of the arguments (most likely
# in the labels), you should enclose the list in quotes to avoid them being
# accidentally broken up and interepretted as indivual arguments!
#
function(add_scaling_figure)
  set(options THROUGHPUT ALLOCATED INLINE_LEGEND)
  set(oneValueArgs NAME BENCHMARK THREADS ALGORITHMS LABELS COLORS MARKERS LEGEND)
  set(multiValueArgs BENCHMARK_ARGS FIGURE_ARGS)
  cmake_parse_arguments(FIGURE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Output plot file and figure file (optional)
  set(PLOT_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/figures/${FIGURE_NAME})
  if(DEFINED FIGURE_LEGEND)
    set(LEGEND_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/figures/${FIGURE_LEGEND})
  else()
    set(LEGEND_OUTPUT_FILE "")
  endif()

  set(XLABEL "Number of threads")

  set(ENV "NUM_THREADS={x}")

  # Choose whether to measure throughput or allocated objects
  if(FIGURE_ALLOCATED)
    set(YLABEL "Allocated Objects")
    set(YREGEX "objects: (\\S+) ")
  else()
    set(YLABEL "Throughput (Mop/s)")
    set(YREGEX "Throughput = (\\S+) Mop/s")
  endif()

  # Add an inline legend if desired
  if(FIGURE_INLINE_LEGEND)
    set(LEGEND_FLAG "--legend")
  else()
    set(LEGEND_FLAG "")
  endif()

  add_custom_target(${FIGURE_NAME} VERBATIM
    COMMAND python3 ${FIGURE_SCRIPT}
        --xparam=--threads
        --xvalues=${FIGURE_THREADS}
        --xlabel=${XLABEL}
        --ylabel=${YLABEL}
        --yregex=${YREGEX}
        --lineparam=--alg
        --linevalues=${FIGURE_ALGORITHMS}
        --labels=${FIGURE_LABELS}
        --colors=${FIGURE_COLORS}
        --markers=${FIGURE_MARKERS}
        ${LEGEND_FLAG}
        --output=${PLOT_OUTPUT_FILE}
        --output_legend=${LEGEND_OUTPUT_FILE}
        --env=${ENV}
        --xbreakers=${P}
        --numa --jemalloc
        ${FIGURE_FIGURE_ARGS}
        ${CMAKE_BINARY_DIR}/benchmarks/${FIGURE_BENCHMARK}
        --
        ${FIGURE_BENCHMARK_ARGS}
    DEPENDS
      ${FIGURE_BENCHMARK}
  )
  add_dependencies(figures ${FIGURE_NAME})
endfunction()


# Add a new figure target that measures throughput or number of allocations vs an arbitrary parameter
#
# Usage:
#   add_figure(
#     NAME            figure name
#     BENCHMARK       benchmark name
#     PARAM           the parameter to vary (e.g. --size, --stack_size)
#     VALUES          comma separated list of values of the parameter to consider
#     XLABEL          x-axis label for the parameter being varied
#     XSCALE          x-axis scale
#     YSCALE          y-axis scale
#     ENV             environment variables to set
#     ALGORITHMS      comma separated list of algorithms
#     LABELS          comma separated list of labels for the algorithms
#     COLORS          comma separated list of colors for the algorithms
#     MARKERS         comma separated list of markers for the algorithms
#     BENCHMARK_ARGS  additional arguments to pass to the benchmark executable
#     FIGURE_ARGS     additional arguments to pass to the figure generation script
#     LEGEND          the name of the legend to generate (OPTIONAL)
#     THROUGHPUT      measure the throughput (OPTIONAL - choose at most one of this or ALLOCATED)
#     ALLOCATED       measure the number of allocated objects (OPTIONAL - choose at most one of this or THROUGHPUT)
#   )
#
# Note that if you need to include spaces in one of the arguments (most likely
# in the labels), you should enclose the list in quotes to avoid them being
# accidentally broken up and interepretted as individual arguments!
#
function(add_general_figure)
  set(options THROUGHPUT ALLOCATED INLINE_LEGEND)
  set(oneValueArgs NAME BENCHMARK PARAM VALUES XLABEL XSCALE YSCALE ALGORITHMS LABELS COLORS MARKERS LEGEND ENV)
  set(multiValueArgs BENCHMARK_ARGS FIGURE_ARGS)
  cmake_parse_arguments(FIGURE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Output plot file and figure file (optional)
  set(PLOT_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/figures/${FIGURE_NAME})
  if(DEFINED FIGURE_LEGEND)
    set(LEGEND_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/figures/${FIGURE_LEGEND})
  else()
    set(LEGEND_OUTPUT_FILE "")
  endif()

  # Choose whether to measure throughput or allocated objects
  if(FIGURE_ALLOCATED)
    set(YLABEL "Allocated Objects")
    set(YREGEX "objects: (\\S+) ")
  else()
    set(YLABEL "Throughput (Mop/s)")
    set(YREGEX "Throughput = (\\S+) Mop/s")
  endif()

  # Add an inline legend if desired
  if(FIGURE_INLINE_LEGEND)
    set(LEGEND_FLAG "--legend")
  else()
    set(LEGEND_FLAG "")
  endif()

  add_custom_target(${FIGURE_NAME} VERBATIM
          COMMAND python3 ${FIGURE_SCRIPT}
          --xparam=${FIGURE_PARAM}
          --xvalues=${FIGURE_VALUES}
          --xlabel=${FIGURE_XLABEL}
          --ylabel=${YLABEL}
          --yregex=${YREGEX}
          --xscale=${FIGURE_XSCALE}
          --yscale=${FIGURE_YSCALE}
          --lineparam=--alg
          --linevalues=${FIGURE_ALGORITHMS}
          --labels=${FIGURE_LABELS}
          --colors=${FIGURE_COLORS}
          --markers=${FIGURE_MARKERS}
          ${LEGEND_FLAG}
          --output=${PLOT_OUTPUT_FILE}
          --output_legend=${LEGEND_OUTPUT_FILE}
          --env=${FIGURE_ENV}
          --numa --jemalloc
          ${FIGURE_FIGURE_ARGS}
          ${CMAKE_BINARY_DIR}/benchmarks/${FIGURE_BENCHMARK}
          --
          ${FIGURE_BENCHMARK_ARGS}
          DEPENDS
          ${FIGURE_BENCHMARK}
          )
  add_dependencies(figures ${FIGURE_NAME})
endfunction()

# -------------------------------------------------------------------------------------------------
#                                   FIGURE DEFINITIONS START HERE
# -------------------------------------------------------------------------------------------------

# --------------------- Load-store experiment --------------------------

add_custom_target(exp-store-figures)
set(store_load_algorithms gnu herlihy herlihy-opt orc arc)
set(store_load_labels "GNU C++" Herlihy "Herlihy (optimized)" OrcGC DRC)
set(store_load_colors tab:blue tab:red tab:purple tab:brown tab:pink)
set(store_load_markers s P * | >)

# Include Folly benchmarks if Folly is installed
if (FOLLY_AVAILABLE)
  list(INSERT store_load_algorithms 1 folly)
  list(INSERT store_load_labels 1 Folly)
  list(INSERT store_load_colors 1 tab:green)
  list(INSERT store_load_markers 1 o)
endif()

# Include just::thread benchmarks if just::thread is installed
if (JUST_THREADS_AVAILABLE)
  list(INSERT store_load_algorithms 1 jss)
  list(INSERT store_load_labels 1 just::thread)
  list(INSERT store_load_colors 1 tab:orange)
  list(INSERT store_load_markers 1 x)
endif()

string(REPLACE ";" "," store_load_algorithms "${store_load_algorithms}" )
string(REPLACE ";" ","  store_load_labels "${store_load_labels}" )
string(REPLACE ";" ","  store_load_colors "${store_load_colors}" )
string(REPLACE ";" ","  store_load_markers "${store_load_markers}" )


# Subfigure (a): Load-store experiment with 10 atomics and 10% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-store-load-10-10
  BENCHMARK bench_ref_count
  THREADS "{threads}"
  ALGORITHMS ${store_load_algorithms}
  LABELS ${store_load_labels}
  COLORS ${store_load_colors}
  MARKERS ${store_load_markers}
  BENCHMARK_ARGS -r 1 -s 10 -u 10
  LEGEND exp-store-load-legend
)
add_dependencies(exp-store-figures exp-store-load-10-10)

# Subfigure (b): Load-store experiment with 10 atomics and 50% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-store-load-10-50
  BENCHMARK bench_ref_count
  THREADS "{threads}"
  ALGORITHMS ${store_load_algorithms}
  LABELS ${store_load_labels}
  COLORS ${store_load_colors}
  MARKERS ${store_load_markers}
  BENCHMARK_ARGS -r 5 -s 10 -u 50
)
add_dependencies(exp-store-figures exp-store-load-10-50)

# Subfigure (c): Load-store experiment with 10^7 atomics and 10% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-store-load-10000000-10
  BENCHMARK bench_ref_count
  THREADS "{threads}"
  ALGORITHMS ${store_load_algorithms}
  LABELS ${store_load_labels}
  COLORS ${store_load_colors}
  MARKERS ${store_load_markers}
  BENCHMARK_ARGS -r 5 -s 10000000 -u 10
)
add_dependencies(exp-store-figures exp-store-load-10000000-10)

# Extra: Load-store experiment with 100,000 atomics and 10% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-store-load-100000-10
  BENCHMARK bench_ref_count
  THREADS "{threads}"
  ALGORITHMS ${store_load_algorithms}
  LABELS ${store_load_labels}
  COLORS ${store_load_colors}
  MARKERS ${store_load_markers}
  BENCHMARK_ARGS -r 5 -s 100000 -u 10
)

# --------------------- Stack experiment --------------------------

add_custom_target(exp-stack-figures)
set(stack_algorithms gnu herlihy-opt orc weak_atomic arc)
set(stack_labels "GNU C++" "Herlihy (optimized)" OrcGC DRC "DRC (+ snapshots)")
set(stack_colors tab:blue tab:purple tab:brown tab:pink tab:olive)
set(stack_markers s * | > D)

# Include Folly benchmarks if Folly is installed
if (FOLLY_AVAILABLE)
  list(INSERT stack_algorithms 1 folly)
  list(INSERT stack_labels 1 Folly)
  list(INSERT stack_colors 1 tab:green)
  list(INSERT stack_markers 1 o)
endif()

# Include just::thread benchmarks if just::thread is installed
if (JUST_THREADS_AVAILABLE)
  list(INSERT stack_algorithms 1 jss)
  list(INSERT stack_labels 1 just::thread)
  list(INSERT stack_colors 1 tab:orange)
  list(INSERT stack_markers 1 x)
endif()

string(REPLACE ";" "," stack_algorithms "${stack_algorithms}" )
string(REPLACE ";" ","  stack_labels "${stack_labels}" )
string(REPLACE ";" ","  stack_colors "${stack_colors}" )
string(REPLACE ";" ","  stack_markers "${stack_markers}" )

# Subfigure (e): Stack experiment with 10 stacks and 1% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-stack-10-1
  BENCHMARK bench_stack
  THREADS "{threads}"
  ALGORITHMS ${stack_algorithms}
  LABELS ${stack_labels}
  COLORS ${stack_colors}
  MARKERS ${stack_markers}
  BENCHMARK_ARGS -r 1 -s 10 -u 1
  LEGEND exp-stack-legend
)
add_dependencies(exp-stack-figures exp-stack-10-1)

# Subfigure (f): Stack experiment with 10 stacks and 10% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-stack-10-10
  BENCHMARK bench_stack
  THREADS "{threads}"
  ALGORITHMS ${stack_algorithms}
  LABELS ${stack_labels}
  COLORS ${stack_colors}
  MARKERS ${stack_markers}
  BENCHMARK_ARGS -r 5 -s 10 -u 10
)
add_dependencies(exp-stack-figures exp-stack-10-10)

# Subfigure (g): Stack experiment with 10 stacks and 50% stores
add_scaling_figure(
  THROUGHPUT
  NAME exp-stack-10-50
  BENCHMARK bench_stack
  THREADS "{threads}"
  ALGORITHMS ${stack_algorithms}
  LABELS ${stack_labels}
  COLORS ${stack_colors}
  MARKERS ${stack_markers}
  BENCHMARK_ARGS -r 5 -s 10 -u 50
)
add_dependencies(exp-stack-figures exp-stack-10-50)

# --------------------- Memory experiments --------------------------

# Subfigure (d): Number of allocated objects with 1 atomic doing 10% stores, versus the number of threads
add_scaling_figure(
  ALLOCATED
  NAME exp-memory-store-load
  BENCHMARK bench_ref_count
  THREADS "{threads}"
  ALGORITHMS orc,arc
  LABELS "OrcGC,DRC"
  COLORS "tab:brown,tab:pink"
  MARKERS "|,>"
  FIGURE_ARGS "--yscale=log"
  BENCHMARK_ARGS -r 5 -s 1 -u 10
)

# Subfigure (h): Amount of memory used with 10 stacks doing 10% stores, versus the length of the stacks
add_general_figure(
  ALLOCATED
  NAME exp-memory-stack-size
  BENCHMARK bench_stack
  PARAM "--stack_size"
  VALUES "10,100,1000,10000,100000,1000000"
  XLABEL "Live objects"
  XSCALE log
  YSCALE log
  ENV "NUM_THREADS=128"
  BENCHMARK_ARGS -r 5 -s 10 -u 10 --threads=128
  INLINE_LEGEND
  FIGURE_ARGS "--baseline=lambda x:\ 10*x"
  ALGORITHMS orc,arc
  LABELS "OrcGC,DRC"
  COLORS "tab:brown,tab:pink"
  MARKERS "|,>"
)

# --------------------- Weak pointer experiments --------------------------

set(wp_algorithms wp dl)
set(wp_labels "Our Weak Pointers" DoubleLink)
set(wp_colors tab:blue tab:green)
set(wp_markers * o)

# Include just::thread benchmarks if just::thread is installed
if (JUST_THREADS_AVAILABLE)
  list(INSERT wp_algorithms 1 jss)
  list(INSERT wp_labels 1 just::thread)
  list(INSERT wp_colors 1 tab:red)
  list(INSERT wp_markers 1 .)
endif()

string(REPLACE ";" "," wp_algorithms "${wp_algorithms}" )
string(REPLACE ";" ","  wp_labels "${wp_labels}" )
string(REPLACE ";" ","  wp_colors "${wp_colors}" )
string(REPLACE ";" ","  wp_markers "${wp_markers}" )

# Paper 2: Figure 13: Throughput of a single concurrent queue undergoing pushes and pops
add_scaling_figure(
  THROUGHPUT
  INLINE_LEGEND
  NAME exp-weak-ptr-single-queue
  BENCHMARK bench_queue
  THREADS "{threads}"
  ALGORITHMS ${wp_algorithms}
  LABELS ${wp_labels}
  COLORS ${wp_colors}
  MARKERS ${wp_markers}
  BENCHMARK_ARGS -r 5 -s 1 --queue_size={x}
)
