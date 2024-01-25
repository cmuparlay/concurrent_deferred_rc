import os.path
import matplotlib as mpl
import statistics as st
import matplotlib as mpl
import multiprocessing
import json
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd

mpl.use('Agg')
mpl.rcParams['grid.linestyle'] = ':'
mpl.rcParams.update({'font.size': 18})

import matplotlib.pyplot as plt
#plt.style.use('ggplot')
import sys
import math
import re
import copy


names = {
    'Hazard' : 'HP (slow)',
    'HazardOpt' : 'HP',
    'RCU' : 'EBR',
    'Range_new'    : 'IBR',
    'HE' : 'HE',
    'NIL' : 'No MM',
    'DEBRA' : 'DEBRA',
    'RSQ' : 'HE-RSQ',
    'RCUShared' : 'EBR-shared-queue',
    'Hyaline' : 'Hyaline',
    'RC'  : 'RC',
    'RCHP'  : 'RC (HP)',
    'RCEBR' : 'RC (EBR)',
    'RCHE' : 'RC (HE)',
    'RCIBR' : 'RC (IBR)',
    'RCHyaline' : 'RC (Hyaline)',
}

colors = {
    'RCU' : 'C0',
    'RC'  : 'C1',
    'Range_new'    : 'C2',
    'Hazard' : 'C3',
    'HazardOpt' : 'C8',
    'HE' : 'C7',
    'NIL' : 'C6',
    'RCHP'  : 'C5',
    'RCEBR' : 'tab:purple',
    'RCIBR' : 'C9',
    'DEBRA' : 'C8',
    'RSQ' : 'C9',
    'RCUShared' : 'C9',
    'Hyaline' : 'C3',
    'RCHyaline' : 'C1',
}

markers = {
    'RCU' : 'o',
    'RC'  : 'v',
    'Range_new'    : '^',
    'Hazard' : 's',
    'HazardOpt' : '*',
    'HE' : 'x',
    'NIL' : '1',
    'RCHP'  : 'D',
    'RCEBR' : 'P',
    'RCIBR' : '<',
    'DEBRA' : '>',
    'RSQ' : '<',
    'RCUShared' : '<',
    'Hyaline' : 's',
    'RCHyaline' : 'v',
}


indir = 'results/'


def avg(l):
  s = 0.0
  for x in l:
    s += x;
  return s/len(l)


def to_string_3(bench, mm, th, result_type):
  return str(mm) + ', ' + str(bench) + ', ' + str(th) + ', ' + str(result_type)

def to_string_benchmark(d):
  return str(d['ds']) + ', ' + 'size:' +  str(d['size']) + ', ' + str(d['workload'])

def to_string2(ds, mm, size, workload, th, result_type):
  return str(mm) + ', ' + str(ds) + ', ' + 'size:' +  str(size) + ', ' + str(workload) + ', ' + str(th) + ', ' + str(result_type)

def to_string(d):
  return to_string2(d['ds'], d['mm'], d['size'], d['workload'], d['th'], d['result_type'])

def to_string_list(int_list):
  return [str(x) for x in int_list]

def firstInt(s):
  for t in s.split():
    try:
      return int(t)
    except ValueError:
      pass
  print(s)
  print('no number in string')

def getAllInts(s):
  ints = []
  for t in s.split():
    try:
      ints.append(int(t))
    except ValueError:
      pass
  return ints

def firstFloat(s):
  for t in s.split():
    try:
      return float(t)
    except ValueError:
      pass
  print('no number in string')

def getAllFloats(s):
  floats = []
  for t in s.split():
    try:
      floats.append(float(t))
    except ValueError:
      pass
  return floats

def add_result(results, key_dict, val):
  key = to_string(key_dict)
  if key not in results:
    results[key] = []
  results[key].append(val)

def add_benchmark(benchmarks, key_dict):
  key = to_string_benchmark(key_dict)
  if key not in benchmarks:
    benchmarks.append(key)

def format_name(name):
  return name

def create_graph(exp_name, results, stddev, bench, memory_managers, threads, graph_title, yaxis, printLegend=False, errorbar=False, onlyrc=False):
  outdir = 'graphs/'
  if not os.path.exists(outdir):
    os.mkdir(outdir)
  
  this_graph_title = graph_title + '-' + exp_name
  print(this_graph_title)

  series = {}
  error = {}
  for alg in memory_managers:
    if yaxis != 'throughput' and alg == 'NIL':
      continue
    if onlyrc and not alg.startswith('RC'):
      continue
    if to_string_3(bench, alg, threads[0], yaxis) not in results:
      continue
    series[alg] = []
    error[alg] = []
    for th in threads:
      if to_string_3(bench, alg, th, yaxis) not in results:
        del series[alg]
        del error[alg]
        break
      if yaxis == 'throughput':
        series[alg].append(results[to_string_3(bench, alg, th, yaxis)])
      else:
        series[alg].append(results[to_string_3(bench, alg, th, yaxis)]/1000.0)
      error[alg].append(stddev[to_string_3(bench, alg, th, yaxis)])

  mn = 99999999
  mx = 0

  # create plot
  fig, axs = plt.subplots(figsize=(6.0, 4.5))
  opacity = 0.8
  rects = {}
   
  offset = 0
  for alg in series:
    if errorbar:
      rects[alg] = axs.errorbar(threads, series[alg], 
                                error[alg], capsize=3, 
                                alpha=opacity,
                                color=colors[alg],
                                markersize=10,
                                marker='o',
                                label=names[alg])
    else:
      rects[alg] = axs.plot(threads, series[alg],
                            alpha=opacity,
                            color=colors[alg],
                            markersize=10,
                            marker=markers[alg],
                            label=names[alg])
  
  if yaxis == 'throughput':
    axs.set(xlabel='Number of threads', ylabel='Throughput (Mop/s)')
  elif yaxis == 'allocated':
    axs.set(xlabel='Number of threads', ylabel='Number of allocated nodes (Thousands)')
  elif yaxis == 'retired':
    axs.set(xlabel='Number of threads', ylabel='Extra nodes (Thousands)')

  plt.axvline(multiprocessing.cpu_count(), linestyle='--', color='grey')

  legend_x = 1
  legend_y = 0.5 
  plt.xticks(threads)

  plt.grid()
  plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y))
  plt.title(this_graph_title)
  plt.savefig(outdir+this_graph_title.replace(' ', '-').replace(':', '-').replace('%','')+'.png', bbox_inches='tight')
  plt.close('all')



def readFile(filename):
  print(filename)
  resultsRaw = {}
  key = {}
  val = {}

  results = {}
  threads = []
  benchmarks = []
  memory_managers = []

  with open(indir + filename) as f:
    for line in f:
      if 'Testing:' in line:
        line = line.replace('LinkList', 'LinkedList')
        num = getAllInts(line)
        key['th'] = num[0]
        if num[0] not in threads:
          threads.append(num[0])
        ds = line.split()[-1]
        if 'RC' in ds:
          key['ds'] = ds[:ds.find('RC')]
          key['mm'] = ds[ds.find('RC'):]
        else:
          key['ds'] = ds
      if 'Prefilled' in line:
        key['size'] = firstInt(line)
      if 'tracker = ' in line:
        key['mm'] = line.replace('\"', '').split()[-1]
      if 'Gets:' in line:
        key['workload'] = line.strip()
      if 'Average allocated nodes:' in line:
        val['allocated'] = firstFloat(line)
      if 'Throughput: ' in line:
        val['throughput'] = firstFloat(line)
        val['retired'] = val['allocated'] - key['size']
        if key['ds'] == 'NatarajanTree':
          val['retired'] -= (key['size']+5)
        for result_type in val:
          key['result_type'] = result_type
          add_result(resultsRaw, key, val[result_type])
        if key['mm'] not in memory_managers:
          memory_managers.append(key['mm'])
        key['result_type'] = ''
        key['mm'] = ''
        add_benchmark(benchmarks, key)

  for key in resultsRaw:
    results[key] = resultsRaw[key][0:]
  
  return {
    'results': results,
    'threads': threads,
    'benchmarks': benchmarks,
    'memory_managers': memory_managers,
  }


def convert(exp_name):
  convert_ds = {'hashtable':'SortedUnorderedMap',
                'list':'LinkedList',
                'bst':'NatarajanTree',
  }
  ds = convert_ds[exp_name.split('-')[1]]
  size = exp_name.split('-')[2].replace('M', '000K').replace('K','000')
  tmp = exp_name.split('-')[3]
  if 'rq' in tmp:
    rq = int(tmp.replace('rq', ''))
    up = 100 - rq
    gets = 0
  else:
    up = int(tmp)
    rq = 0
    gets = 100 - up
  return ds + ', size:' + str(size) + ', Gets:' + str(gets) + ' Updates:' + str(up) + ' RQs: ' + str(rq)

def parse_experiment_data(results):
  """
  Returns a json containing a readable version of the results 
  """
  nice_data = {
    'allocations': {},
    'throughput': {},
    'retired': {},
    'metadata': {
      'benchmarks': results['benchmarks'],
      'memory_managers': results['memory_managers'],
      'threads': results['threads'],
    }
  }
  for (key, value) in results['results'].items():
    split_key = key.split(', ') # [memory_manager, data_structure, size, gets updates RQs, threads, result_type]
    memory_manager = split_key[0]
    data_structure = split_key[1]
    threads = split_key[4]
    result_type = split_key[5]
    nice_data['metadata']['data_structures'] = data_structure
    if result_type == 'allocated':
      exp_data_location = nice_data['allocations'] 
    elif result_type == 'throughput':
      exp_data_location = nice_data['throughput']
    elif result_type == 'retired':
      exp_data_location = nice_data['retired']
    else:
      raise Exception('Unknown result type')
    
    if memory_manager not in exp_data_location:
      exp_data_location[memory_manager] = {}
    
    exp_data_location[memory_manager][threads] = value
  
  return nice_data

def get_value(data):
  ret = {}
  for (key, value) in data.items():
    ret[key] = value[0]
  return ret

def get_error(data):
  ret = {}
  for (key, value) in data.items():
    ret[key] = value[1]
  return ret


def exp_data_to_dataframe(exp_data):
  df_data = []
  for (memory_manager, thread_data) in exp_data.items():
    for (threads, data) in thread_data.items():
      for value in data:
        df_data.append({
          'memory_manager': memory_manager,
          'threads': threads,
          'value': value,
        })
  df = pd.DataFrame(df_data)
  print(df)
  return df

exp_type_to_yaxis = {
  'throughput': 'Throughput (Mop/s)',
  'allocations': 'Extra nodes (Thousands)',
  'retired': '???',
}

def graph_experiment(exp_type, data, metadata):
  sns.set_theme(style="whitegrid")
  sns.set(font_scale=1.2)

  data = exp_data_to_dataframe(data)

  cur_plot = sns.lineplot(data=data, x='threads', y='value', hue='memory_manager', style='memory_manager', markers=True, dashes=False)
  cur_plot.set_title(f'{exp_type} - {metadata["data_structures"]}')
  cur_plot.set_xlabel('Number of threads')
  cur_plot.set_ylabel(exp_type_to_yaxis[exp_type])
  plt.savefig(f'graphs/{exp_type}.png')
  plt.clf()


def graph_results_from_file(exp_name, filename_tag):

  results = readFile(f'{exp_name}{filename_tag}.out')
  experiment_data = parse_experiment_data(results)
  with open(f'results/{exp_name}{filename_tag}.json', 'w') as f:
    f.write(json.dumps(experiment_data, indent=4))
  
  for exp_type in ['allocations', 'throughput', 'retired']:
    graph_experiment(exp_type, experiment_data[exp_type], experiment_data['metadata'])

  # memory_managers = ['NIL', 'HazardOpt', 'RCU', 'DEBRA', 'Hazard', 'Range_new', 'HE', 'Hyaline', 'RC', 'RCHP', 'RSQ', 'RCUShared', 'RCEBR', 'RCIBR', 'RCHyaline']

  # create_graph(exp_name+filename_tag, results, stddev, convert(exp_name), memory_managers, threads, 'throughput', 'throughput', False, False)
  # create_graph(exp_name+filename_tag, results, stddev, convert(exp_name), memory_managers, threads, 'memory', 'retired', False, False)
