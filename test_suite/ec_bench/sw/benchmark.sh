#!/bin/sh

./cpuid >benchmark.txt

RS_BLOCKS=1024
RS_PARAMS="-csv -size=1MiB"

RS_K=12
RS_M=4

./benchmark -blocks=$RS_BLOCKS -cpu=1 -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=2 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=4 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=8 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=12 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=16 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=20 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=24 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=28 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=32 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=40 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=48 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=56 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=64 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=72 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=80 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=88 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=96 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=104 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=112 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=120 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=128 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt

RS_K=8
RS_M=8

./benchmark -blocks=$RS_BLOCKS -cpu=1 -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=2 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=4 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=8 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=12 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=16 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=20 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=24 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=28 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=32 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=40 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=48 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=56 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=64 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=72 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=80 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=88 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=96 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=104 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=112 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=120 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=128 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt

RS_K=4
RS_M=4

./benchmark -blocks=$RS_BLOCKS -cpu=1 -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=2 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=4 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=8 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=12 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=16 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=20 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=24 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=28 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=32 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=40 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=48 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=56 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=64 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=72 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=80 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=88 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=96 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=104 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=112 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=120 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=128 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt

RS_PARAMS="-csv -size=1MiB -gfni=false"
RS_K=12
RS_M=4

./benchmark -blocks=$RS_BLOCKS -cpu=1 -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=2 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=4 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=8 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=12 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=16 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=20 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=24 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=28 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=32 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=40 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=48 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=56 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=64 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=72 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=80 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=88 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=96 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=104 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=112 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=120 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=128 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt

RS_BLOCKS=32
RS_PARAMS="-csv -progress=false"

RS_K=12
RS_M=4

./benchmark -blocks=$RS_BLOCKS -cpu=1 -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=2 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=4 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=8 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=12 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=16 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=20 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=24 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=28 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt
./benchmark -concurrent -cpu=32 -blocks=$RS_BLOCKS -k=$RS_K -m=$RS_M $RS_PARAMS >>benchmark.txt

cat /proc/cpuinfo >>benchmark.txt

