#!/bin/bash


# Test Mode 0 : SequentialRemoveTest:prefill=20K
# Test Mode 1 : ObjRetire:i50rm50:range=140:prefill=100
# Test Mode 2 : ObjRetire:i50rm50:range=1400:prefill=1000
# Test Mode 3 : ObjRetire:i50rm50:range=140K:prefill=100K
# Test Mode 4 : ObjRetire:i50rm50:range=2M:prefill=1M
# Test Mode 5 : ObjRetire:i50rm50:range=20M:prefill=10M
# Test Mode 6 : ObjRetire:g90i5rm5:range=140:prefill=100
# Test Mode 7 : ObjRetire:g90i5rm5:range=1400:prefill=1000
# Test Mode 8 : ObjRetire:g90i5rm5:range=140K:prefill=100K
# Test Mode 9 : ObjRetire:g90i5rm5:range=2M:prefill=1M
# Test Mode 10 : ObjRetire:g90i5rm5:range=20M:prefill=10M
# Test Mode 11 : ObjRetire:g100:range=140:prefill=100
# Test Mode 12 : ObjRetire:g100:range=1400:prefill=1000
# Test Mode 13 : ObjRetire:g100:range=140K:prefill=100K
# Test Mode 14 : ObjRetire:g100:range=2M:prefill=1M
# Test Mode 15 : ObjRetire:g100:range=20M:prefill=10M
# Test Mode 16 : ObjRetire:i49rm49rq2:range=140K:prefill=100K
# Test Mode 17 : ObjRetire:i50rm49rq1:range=2M:prefill=1M


make debug -j
set -o xtrace

./bin/debug/main -i 1 -m 6 -r 8 -t 1
./bin/debug/main -i 1 -m 8 -r 7 -t 1
./bin/debug/main -i 1 -m 8 -r 9 -t 1

NUM_THREADS=201 ./bin/debug/main -i 1 -m 6 -r 8 -t 200
NUM_THREADS=201 ./bin/debug/main -i 1 -m 8 -r 7 -t 200
NUM_THREADS=201 ./bin/debug/main -i 1 -m 8 -r 9 -t 200

./bin/debug/main -i 1 -m 6 -r 4 -t 1
./bin/debug/main -i 1 -m 8 -r 2 -t 1
./bin/debug/main -i 1 -m 8 -r 6 -t 1

NUM_THREADS=201 ./bin/debug/main -i 1 -m 6 -r 4 -t 200
NUM_THREADS=201 ./bin/debug/main -i 1 -m 8 -r 2 -t 200
NUM_THREADS=201 ./bin/debug/main -i 1 -m 8 -r 6 -t 200

./bin/debug/main -i 1 -m 6 -r 3 -t 1 -d tracker=RCU
./bin/debug/main -i 1 -m 6 -r 3 -t 1 -d tracker=Hazard
./bin/debug/main -i 1 -m 6 -r 3 -t 1 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 6 -r 3 -t 1 -d tracker=Range_new
./bin/debug/main -i 1 -m 6 -r 3 -t 1 -d tracker=HE

./bin/debug/main -i 1 -m 8 -r 1 -t 1 -d tracker=RCU
./bin/debug/main -i 1 -m 8 -r 1 -t 1 -d tracker=Hazard
./bin/debug/main -i 1 -m 8 -r 1 -t 1 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 8 -r 1 -t 1 -d tracker=Range_new
./bin/debug/main -i 1 -m 8 -r 1 -t 1 -d tracker=HE

./bin/debug/main -i 1 -m 8 -r 5 -t 1 -d tracker=RCU
./bin/debug/main -i 1 -m 8 -r 5 -t 1 -d tracker=Hazard
./bin/debug/main -i 1 -m 8 -r 5 -t 1 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 8 -r 5 -t 1 -d tracker=Range_new
./bin/debug/main -i 1 -m 8 -r 5 -t 1 -d tracker=HE

./bin/debug/main -i 1 -m 6 -r 3 -t 200 -d tracker=RCU
./bin/debug/main -i 1 -m 6 -r 3 -t 200 -d tracker=Hazard
./bin/debug/main -i 1 -m 6 -r 3 -t 200 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 6 -r 3 -t 200 -d tracker=Range_new
./bin/debug/main -i 1 -m 6 -r 3 -t 200 -d tracker=HE

./bin/debug/main -i 1 -m 8 -r 1 -t 200 -d tracker=RCU
./bin/debug/main -i 1 -m 8 -r 1 -t 200 -d tracker=Hazard
./bin/debug/main -i 1 -m 8 -r 1 -t 200 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 8 -r 1 -t 200 -d tracker=Range_new
./bin/debug/main -i 1 -m 8 -r 1 -t 200 -d tracker=HE

./bin/debug/main -i 1 -m 8 -r 5 -t 200 -d tracker=RCU
./bin/debug/main -i 1 -m 8 -r 5 -t 200 -d tracker=Hazard
./bin/debug/main -i 1 -m 8 -r 5 -t 200 -d tracker=HazardOpt
./bin/debug/main -i 1 -m 8 -r 5 -t 200 -d tracker=Range_new
./bin/debug/main -i 1 -m 8 -r 5 -t 200 -d tracker=HE