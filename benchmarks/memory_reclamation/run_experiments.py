# Rideable 0 : Unused
# Rideable 1 : SortedUnorderedMap
# Rideable 2 : SortedUnorderedMapRC
# Rideable 3 : SortedUnorderedMapRCHP
# Rideable 4 : SortedUnorderedMapRCEBR
# Rideable 5 : SortedUnorderedMapRCIBR
# Rideable 6 : SortedUnorderedMapRCHyaline
# Rideable 7 : LinkList
# Rideable 8 : LinkedListRC
# Rideable 9 : LinkListRCHP
# Rideable 10 : LinkListRCEBR
# Rideable 11 : LinkListRCIBR
# Rideable 12 : LinkListRCHyaline
# Rideable 13 : NatarajanTree
# Rideable 14 : NatarajanTreeRC
# Rideable 15 : NatarajanTreeRCHP
# Rideable 16 : NatarajanTreeRCEBR
# Rideable 17 : NatarajanTreeRCIBR
# Rideable 18 : NatarajanTreeRCHyaline

# Test Mode 0 : SequentialRemoveTest:prefill=20K
# Test Mode 1 : ObjRetire:u50:range=200:prefill=100
# Test Mode 2 : ObjRetire:u50:range=2000:prefill=1000
# Test Mode 3 : ObjRetire:u50:range=200K:prefill=100K
# Test Mode 4 : ObjRetire:u50:range=2M:prefill=1M
# Test Mode 5 : ObjRetire:u50:range=20M:prefill=10M
# Test Mode 6 : ObjRetire:u10:range=200:prefill=100
# Test Mode 7 : ObjRetire:u10:range=2000:prefill=1000
# Test Mode 8 : ObjRetire:u10:range=200K:prefill=100K
# Test Mode 9 : ObjRetire:u10:range=2M:prefill=1M
# Test Mode 10 : ObjRetire:u10:range=20M:prefill=10M
# Test Mode 11 : ObjRetire:u1:range=200:prefill=100
# Test Mode 12 : ObjRetire:u1:range=2000:prefill=1000
# Test Mode 13 : ObjRetire:u1:range=200K:prefill=100K
# Test Mode 14 : ObjRetire:u1:range=2M:prefill=1M
# Test Mode 15 : ObjRetire:u1:range=20M:prefill=10M
# Test Mode 16 : ObjRetire:u50:range=8192:prefill=4096
# Test Mode 17 : ObjRetire:u10:range=8192:prefill=4096
# Test Mode 18 : ObjRetire:u1:range=8192:prefill=4096
# Test Mode 19 : ObjRetire:u50:range=200M:prefill=100M
# Test Mode 20 : ObjRetire:u10:range=200M:prefill=100M
# Test Mode 21 : ObjRetire:u1:range=200M:prefill=100M
# Test Mode 22 : SequentialRemoveTest:prefill=0
# Test Mode 23 : SequentialRemoveTest:prefill=1
# Test Mode 24 : ObjRetire:u50rq50:range=200K:prefill=100K
# Test Mode 25 : ObjRetire:u50rq50:range=2M:prefill=1M
# Test Mode 26 : ObjRetire:u50rq50:range=20M:prefill=10M
# Test Mode 27 : ObjRetire:u50rq50:range=200M:prefill=100M


import multiprocessing
import os
import subprocess
import argparse

import create_graphs as graph

datastructures = ['hashtable', 'list', 'bst']


def to_experiment_string(datastructure, workload: int) -> str:
    return f'exp-{datastructure}-{workload}'


def valid_command(cmd: str) -> bool:
    """
    Returns True if the given command returns a non-zero return code,
    i.e., is probably a valid command
    """
    DEVNULL = open(os.devnull, 'r+b', 0)
    returncode = subprocess.call(
        [cmd], stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL, shell=True)
    return returncode == 0


def convert(seconds):
    seconds = seconds % (24 * 3600)
    hour = seconds // 3600
    seconds %= 3600
    minutes = seconds // 60
    seconds %= 60
    return f'{hour}h:{minutes:02}m:{seconds:02}s'


def run(binary, preamble, datastructure, workload, tracker, runtime, count, outfile):
    # runtime in second
    num_runs = 0
    for th in threads:
        num_threads = max(32, th+1)
        cmd = f"NUM_THREADS={num_threads} {preamble} {binary} -i {runtime} -m {workload} -r {datastructure} -t {th} -v "
        if tracker != "":
            cmd += f"-d tracker={tracker}"
        if not count:
            print(cmd)
        for _ in range(repeats):
            if count:
                num_runs += 1
            else:
                os.system(f'{cmd} >> {outdir}{outfile}')
    return num_runs


if __name__ == "__main__":
    sizes = ['100', '1000', '100K', '1M', '10M', '100M']
    update_percents = [1, 10, 50]

    parser = argparse.ArgumentParser(
        description='concurrent deffered reference counting benchmarks')

    parser.add_argument('--datastructure', '-d', type=str, default='all',
                        help=f'datastructure to benchmark {[*datastructures, "all"]}')
    parser.add_argument('--size', '-s', type=str,
                        default='100', help=f'Size of datastructure {sizes}')
    parser.add_argument('--update_percent', '-u', type=int,
                        default=10, help=f'Update percentage, with the remainder being reads {update_percents}')
    parser.add_argument('--runtime', '-rt', type=int, default=5,
                        help='runtime of each experiment in seconds')
    parser.add_argument('--repeats', '-r', type=int, default=3,
                        help='number of times to repeat each experiment')
    parser.add_argument('--testmode', type=bool, default=False,
                        help='Test mode, only checks 3 thread counts')

    args = parser.parse_args()

    smr_datastructure = {'hashtable': 1,
                         'list': 7,
                         'bst': 13}
    rc_datastructures = {'hashtable': [3, 4, 5, 6],
                         'list': [9, 10, 11, 12],
                         'bst': [15, 16, 17, 18], }
    wl_num = {
        '100-50': 1,
        '1000-50': 2,
        '100K-50': 3,
        '1M-50': 4,
        '10M-50': 5,
        '100-10': 6,
        '1000-10': 7,
        '100K-10': 8,
        '1M-10': 9,
        '10M-10': 10,
        '100-1': 11,
        '1000-1': 12,
        '100K-1': 13,
        '1M-1': 14,
        '10M-1': 15,
        '100M-50': 19,
        '100M-10': 20,
        '100M-1': 21,
        '100K-50rq': 24,
        '1M-50rq': 25,
        '10M-50rq': 26,
        '100M-50rq': 27,
    }
    experiments = [("list", '1000-10'),
                   ("hashtable", '100K-10'),
                   ("bst", '100K-10'),
                   ("bst", '100M-10'),
                   ("bst", '100K-1'),
                   ("bst", '100K-50'),]

    graphs_only = False

    repeats = args.repeats
    runtime = args.runtime

    # Compute thread counts to use for experiments
    P = multiprocessing.cpu_count()
    if not args.testmode:
        p_step_size = P // 5
        threads = [1] + [k*p_step_size for k in range(1, 5)] + [P] + [
            P + k*p_step_size for k in range(1, 3)]
    else:
        threads = [1, P // 2, P, P + P // 2]

    trackers = ["HazardOpt -d emptyf=2", "Hyaline -d emptyf=2",
                "RCU -d epochf=10 -d emptyf=2", "Range_new -d epochf=40 -d emptyf=2"]

    exp_to_run = []
    if args.datastructure == 'all':
        for exp in experiments:
            exp_to_run.append(exp)
    else:
        ds = args.datastructure
        size = args.size
        up = args.update_percent
        if 'rq' in str(up):
            trackers_copy = []
            for tr in trackers:
                if 'HazardOpt' not in tr:
                    trackers_copy.append(tr)
            trackers = trackers_copy
        workload = f'{size}-{up}'
        if workload not in wl_num:
            print(f'invalid workload: must be one of {[*wl_num.keys()]}')
            print(f'you entered: {workload} (size-update_percent)')
            exit(1)
        exp_to_run.append((ds, workload))

    binary = "./bin/release/main"
    preamble = ""

    if valid_command('jemalloc-config --libdir'):
        preamble += "LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` "
    else:
        print('Note: Not dynamically linking jemalloc because jemalloc could not be found')
    if valid_command('numactl -i all echo'):
        preamble += " numactl -i all "
    else:
        print('Note: Not running with NUMA since numactl is not installed or is not valid on this system')

    num_experiments = 0
    estimated_time = 0

    outdir = 'results/'
    if not os.path.exists(outdir):
        os.mkdir(outdir)

    progress = 0

    if not graphs_only:
        for count in (1, 0):
            if count == 0:
                print(
                    f'Running {num_experiments} experiments in {convert(num_experiments*runtime)}')
                print(f'\tthread counts: {", ".join(map(str, threads))}')
                print(f'\truntime: {runtime}s')
                print(f'\trepeats: {repeats}')
            for (ds, workload) in exp_to_run:
                outfile = f'{to_experiment_string(ds, workload)}.out'
                os.system(f' > {outdir}{outfile}')
                for rc_ds in rc_datastructures[ds]:
                    num_experiments += run(binary, preamble, rc_ds,
                                           wl_num[workload], "", runtime, count, outfile)
                    if count == 0:
                        progress += len(threads)*repeats
                        print(f'{progress} out of {num_experiments}')
                for tr in trackers:
                    num_experiments += run(binary,
                                           preamble,
                                           smr_datastructure[ds],
                                           wl_num[workload],
                                           tr,
                                           runtime,
                                           count,
                                           outfile)
                    if count == 0:
                        progress += len(threads)*repeats
                        print(f'{progress} out of {num_experiments}')

    # create graphs
    for (datastructure, workload) in exp_to_run:
        graph.graph_results_from_file(datastructure, workload)
