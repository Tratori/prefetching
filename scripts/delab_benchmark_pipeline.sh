#!/bin/bash

# Execute
# ./delab_benchmark_pipeline.sh <node_name> <benchmark_name>


cd /prefetching

mkdir -p cmake-build-release-${1}
cd cmake-build-release-${1}

cmake -DCMAKE_BUILD_TYPE=Release ..

make -j

./benchmark/${2}