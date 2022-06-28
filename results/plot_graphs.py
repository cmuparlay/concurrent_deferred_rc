# Plot graphs for benchmark throughput tests
#

import argparse
import matplotlib
import multiprocessing
import os
import re
import subprocess

# Use headless backend for matplotlib. This allows it
# to run on a server without a GUI available.
matplotlib.use("Agg")

import matplotlib.pyplot as plt

# Returns True if the given command returns a non-zero return code,
# i.e., is probably a valid command. This will execute the given shell
# command verbatim so it probably shouldn't have any side effects and
# most definitely shouldn't do anything dangerous
def valid_command(cmd):
  DEVNULL = open(os.devnull, 'r+b', 0)
  returncode = subprocess.call([cmd], stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL, shell=True)
  return returncode == 0

class bcolors:
  HEADER = '\033[95m'
  OKBLUE = '\033[94m'
  OKCYAN = '\033[96m'
  OKGREEN = '\033[92m'
  WARNING = '\033[93m'
  FAIL = '\033[91m'
  ENDC = '\033[0m'
  BOLD = '\033[1m'
  UNDERLINE = '\033[4m'


if valid_command('numactl -i all echo'):
  NUMA_COMMAND = 'numactl -i all '
else:
  NUMA_COMMAND = ""

if valid_command('jemalloc-config --libdir'):
  JEMALLOC_COMMAND = "LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` "
else:
  JEMALLOC_COMMAND = ""

NUM_THREADS_COMMAND = 'NUM_THREADS={} '

# Plot markers and colors to use
DEFAULT_MARKERS = ['s', 'x', 'o', 'P', '*', '|', '>', 'v', '^', '<']
DEFAULT_COLORS = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple', 'tab:brown', 'tab:pink', 'tab:gray', 'tab:olive', 'tab:cyan']
TERMINAL_COLORS = {'b':'\u001b[34m', 'g':'\u001b[32m', 'r':'\u001b[31m', 'c':'\u001b[36m', 'm':'\u001b[35m', 'y':'\u001b[33m', 'k':'\u001b[37m', 'tab:blue':'\u001b[94m', 'tab:orange':'\u001b[33m', 'tab:green':'\u001b[92m', 'tab:red':'\u001b[31m', 'tab:purple':'\u001b[35m', 'tab:brown':'\u001b[31m', 'tab:pink':'\u001b[95m', 'tab:gray':'\u001b[90m', 'tab:olive':'\u001b[33m', 'tab:cyan':'\u001b[96m'}

# Return a sequence of thread counts suitable for benchmarking
def get_thread_series():
  P = multiprocessing.cpu_count()
  p_step_size = P // 5
  return [1] + [k*p_step_size for k in range(1,5)] + [P] + [P + k*p_step_size for k in range(1,3)]

# Return the corresponding ANSI escape code for colored
# text in the given color, or '' if the given color is
# not one of the base colors.
def terminal_color(color):
  if color in TERMINAL_COLORS: return TERMINAL_COLORS[color]
  else: return ''

# Compute the median of the given sequence
def median(results):
  assert(len(results) > 0)
  return sorted(results)[len(results)//2]

# Given the output from stdout of the benchmark executable, return a list
# of floating point numbers indicating the measurement of each iteration
# If measure_memory is True, it evaluates the memory usage statistic,
# otherwise it evaluates the throughput.
def parse_benchmark_output(output, regex):
  results = []
  for line in output.split('\n'):
    r = re.search(regex, line)
    if r is not None:
      results.append(float(r.group(1)))
  return results

# Given a benchmark executable, a thread count parameter, algorithm parameter, and
# some additional arguments, execute the benchmark with these parameters and arguments
# and return a list of floating point numbers indicating the throughput of each iteration
def get_benchmark_results(benchmark, benchmark_arguments, numa, jemalloc, env, result_regex):

  # Build the shell command to run the benchmark with the required arguments
  full_command = '{} {}'.format(benchmark, benchmark_arguments)
  if numa: full_command = NUMA_COMMAND + full_command
  if jemalloc: full_command = JEMALLOC_COMMAND + full_command
  full_command = env + ' ' + full_command
  print(bcolors.OKGREEN + full_command + bcolors.ENDC)

  # Execute the benchmark and check whether it succeeded
  process = subprocess.run(full_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  if process.returncode != 0:
    print(bcolors.FAIL + 'Error: Benchmark exited with return code {}'.format(process.returncode))
    if process.stderr is not None:
      error = process.stderr.decode('ascii')
      print(error)
    exit(process.returncode)
  else:
    output = process.stdout.decode('ascii')
    results = parse_benchmark_output(output, result_regex)
    if len(results) == 0:
      print(bcolors.FAIL + 'Error: No results found in the benchmark output, which was:' + bcolors.ENDC)
      print(output)
      exit(1)
    else:
      return results


def plot_benchmarks(benchmark, benchmark_config, plot_config, output):

  plt.rcParams["font.size"] = "18"
  plt.grid(True, linestyle='dotted')

  plt.xlabel(plot_config['xlabel'])
  plt.ylabel(plot_config['ylabel'])
  plt.xscale(plot_config['xscale'])
  plt.yscale(plot_config['yscale'])

  # Plot the lines
  for line, label, marker, color in zip(benchmark_config['linevalues'], plot_config['line_labels'], plot_config['markers'], plot_config['colors']):
    print(terminal_color(color) + '{} ({})'.format(label, line) + bcolors.ENDC)
    measurements = []
    for x in sorted(xvalues):

      # Generate the full argument list
      x_arg = '{}={}'.format(benchmark_config['xparam'], x)
      line_arg = '{}={}'.format(benchmark_config['lineparam'], line)
      extra_args = benchmark_config['args'].replace('{x}', str(x)).replace('{line}', str(line))
      full_args = '{} {} {}'.format(x_arg, line_arg, extra_args)

      # Format the environment variables. We replace {x} with the value of x and
      # {line} with the key of the line. This allows users to vary an environment
      # variable from one run to another rather than keeping it constant.
      env = benchmark_config['env'].replace('{x}', str(x)).replace('{line}', str(line))

      results = get_benchmark_results(benchmark, full_args, benchmark_config['numa'], benchmark_config['jemalloc'], env, benchmark_config['regex'])
      median_result = median(results)
      measurements.append(median_result)
      scale_factor = median_result / measurements[0]
      print('  {}={:<8} {} ({:.1f}x Increase)'.format(benchmark_config['xparam'], x, median_result, scale_factor))
    plt.plot(benchmark_config['xvalues'], measurements, label=label, marker=marker, color=color)

  # Plot the baseline (if any)
  if benchmark_config['baseline'] is not None:
    baseline = eval(benchmark_config['baseline'])
    measurements = []
    for x in sorted(xvalues):
      measurements.append(baseline(x))
    plt.plot(benchmark_config['xvalues'], measurements, label='Baseline', marker=None, color='k')

  # Plot vertical dividers (if any)
  for breaker in plot_config['xbreakers']:
    plt.axvline(breaker, linestyle='--', color='grey')

  if (plot_config['legend']): plt.legend()
  
  f = plt.gcf()
  filepath = output
  f.savefig(filepath + '.png', bbox_inches='tight')
  f.savefig(filepath + '.eps', bbox_inches='tight')

# Output a standalone legend for the given algorithms. The plots must have already been
# generated, as this method simply takes the data from the current axis and generates
# its legend.
def create_legend(filename):
  plt.rcParams["font.size"] = "12"
  plt.grid(False)
  ax = plt.gca()
  fig = plt.figure()
  ax2 = fig.add_subplot()
  ax2.axis('off')
  legend = ax.legend(*ax.get_legend_handles_labels(), bbox_to_anchor=(1.01, 1), frameon=False, loc='upper left', ncol=10)
  fig = legend.figure
  fig.canvas.draw()
  bbox = legend.get_window_extent().transformed(fig.dpi_scale_trans.inverted())
  fig.savefig(filename + '.png', dpi="figure", bbox_inches=bbox)
  fig.savefig(filename + '.eps', dpi="figure", bbox_inches=bbox)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument('benchmark', type=str, help='The benchmark executable to run')
  parser.add_argument('--xparam', type=str, required=True, help='The benchmark parameter to vary as the x-axis of the plot')
  parser.add_argument('--xvalues', type=str, required=True, help='A comma-separated list of parameters that denote the x values in the benchmark plot. Alternatively, set to {threads} to automatically select a sequence of appropriate thread counts for the machine')
  parser.add_argument('--lineparam', type=str, required=True, help='The benchmark parameter to vary to produce each line on the plot')
  parser.add_argument('--linevalues', type=str, required=True, help='A comma-separated list of parameters that denote the lines in the benchmark plot')
  parser.add_argument('--baseline', type=str, required=False, help='A python lambda defining a "baseline" line to compare on the plot')
  parser.add_argument('--xlabel', type=str, required=True, help='Label for the x-axis')
  parser.add_argument('--ylabel', type=str, required=True, help='Label for the y-axis')
  parser.add_argument('--xscale', type=str, required=False, default='linear', help='Scale for the x axis')
  parser.add_argument('--yscale', type=str, required=False, default='linear', help='Scale for the y axis')
  parser.add_argument('--yregex', type=str, required=True, help='A regular expression that extracts the result to be measured from the program output')
  parser.add_argument('--xbreakers', type=str, required=False, help='A comma-separated list of x values at which to display a dashed vertical line')
  parser.add_argument('--labels', type=str, default=None, required=False, help='A comma separated list of labels to name the lines in the plot. By default, uses their corresponding parameter key')
  parser.add_argument('--env', type=str, default='', required=False, help='Environment variables to run the benchmark with')
  parser.add_argument('--markers', type=str, default=None, required=False, help='A comma separated list of markers to use for each line.')
  parser.add_argument('--colors', type=str, default=None, required=False, help='A comma separated list of colors to use for each line.')
  parser.add_argument('--legend', action='store_true', default=False, help='Display a legend on the plots')
  parser.add_argument('--output', type=str, required=True, help='Output filename prefix (no extension)')
  parser.add_argument('--output_legend', type=str, default=None, required=False, help='Output a standalone legend to this file')
  parser.add_argument('--numa', action='store_true', default=False, help='Run the benchmarks with numactl -i all')
  parser.add_argument('--jemalloc', action='store_true', default=False, help='Run the benchmarks with jemalloc')
  parser.add_argument('arguments', nargs=argparse.REMAINDER, help='(Seperated by --) Additional arguments to forward to the benchmark executable')
  args = parser.parse_args()

  # Use TrueType fonts
  matplotlib.rcParams['pdf.fonttype'] = 42
  matplotlib.rcParams['ps.fonttype'] = 42
  
  # Determine the benchmark to run
  benchmark = args.benchmark
  print(bcolors.HEADER + 'Benchmarking {}'.format(benchmark) + bcolors.ENDC)
  print('---------------------------------------------------')
  if not os.path.isfile(benchmark):
    print(bcolors.FAIL + 'Error: {} does not exist'.format(benchmark) + bcolors.ENDC)
    exit(1)
  
  # Determine the different shared pointer implementations to compare
  lines = args.linevalues.split(',')
  labels = args.labels.split(',') if args.labels is not None else lines
  if (len(lines) != len(labels)):
    print(bcolors.FAIL + 'Error: Number of labels does not equal number of lines. Found {} lines but {} labels'.format(len(lines), len(labels)) + bcolors.ENDC)
    exit(1)
  
  # Plot markers
  markers = args.markers.split(',') if args.markers is not None else DEFAULT_MARKERS[:len(lines)]
  if (len(markers) != len(lines)):
    print(bcolors.FAIL + 'Error: Number of markers does not equal number of lines. Found {} lines but {} markers'.format(len(lines), len(markers)) + bcolors.ENDC)
    exit(1)
    
  # Line colors
  colors = args.colors.split(',') if args.colors is not None else DEFAULT_COLORS[:len(lines)]
  if (len(colors) != len(lines)):
    print(bcolors.FAIL + 'Error: Number of colors does not equal number of lines. Found {} lines but {} colors: {}'.format(len(lines), len(colors)) + bcolors.ENDC)
    exit(1)
    
  print('Comparing the following values for the parameter {}:'.format(args.lineparam))
  for color,line,label,marker in zip(colors, lines, labels, markers):
    print(terminal_color(color) + '  ({}) {:<16} {}'.format(marker, line, label) + bcolors.ENDC)
  
  # Determine the x values to run the benchmark on
  # If the xvalues parameter is set to {threads}, then a sequence of thread values
  # appropriate for the running machine will be used
  xvalues = get_thread_series() if args.xvalues == '{threads}' else list(map(int, args.xvalues.split(',')))
  print('Using values {} for parameter {} on the x-axis'.format(', '.join(str(x) for x in xvalues), args.xparam))

  xbreakers = map(float, args.xbreakers.split(',')) if args.xbreakers is not None else []

  # Make sure that the output directory is accessible
  if not os.path.exists(os.path.dirname(args.output)):
    print(bcolors.FAIL + 'Error: output directory does not exist' + bcolors.ENDC)
    exit(1)
  else:
    print('Outputting plot to: {}'.format(args.output))

  if args.output_legend is not None and args.output_legend != '':
    if not os.path.exists(os.path.dirname(args.output_legend)):
      print(bcolors.FAIL + 'Error: legend output directory does not exist')
      exit(1)
    else:
      print('Outputting legend to: {}'.format(args.output_legend))

  # Arguments to forward
  benchmark_arguments = ' '.join(args.arguments)
  print('Measuring output of regex: {}'.format(args.yregex))
  print('Arguments for benchmark executable: {}'.format(benchmark_arguments))
  print('Environment variables: {}'.format(args.env))
  if (args.baseline): print('Baseline curve: {}'.format(args.baseline))

  if args.numa and NUMA_COMMAND == "":
    print('Note: Not running with NUMA since numactl is not installed or is not valid on this system')

  if args.jemalloc and JEMALLOC_COMMAND == "":
    print('Note: Not dynamically linking jemalloc because jemalloc could not be found')

  print('---------------------------------------------------')

  benchmark_config = {
    'xparam': args.xparam,
    'xvalues': xvalues,
    'lineparam': args.lineparam,
    'linevalues': lines,
    'args': benchmark_arguments,
    'numa': args.numa,
    'jemalloc': args.jemalloc,
    'env': args.env,
    'regex': args.yregex,
    'baseline': args.baseline
  }

  plot_config = {
    'xlabel': args.xlabel,
    'xscale': args.xscale,
    'ylabel': args.ylabel,
    'yscale': args.yscale,
    'line_labels': labels,
    'markers': markers,
    'colors': colors,
    'legend': args.legend,
    'xbreakers': xbreakers
  }

  plot_benchmarks(benchmark, benchmark_config, plot_config, args.output)

  # Create a seperate legend
  if args.output_legend is not None and args.output_legend != '':
    create_legend(args.output_legend)
    