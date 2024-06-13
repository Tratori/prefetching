#!/bin/bash

# Execute
# ./delab_benchmark_pipeline.sh <node_name> <benchmark_executable_name> <run_name>

LOG_FILE="/prefetching/results/${2}/${1}/build_log.txt"

{
    cd /prefetching

    mkdir -p cmake-build-release-${1}
    cd cmake-build-release-${1}

    cmake -DCMAKE_BUILD_TYPE=Release ..

    make -j

    if ./benchmark/${2}; then
        echo "success" >&2
    else
        echo "error" >&2
    fi
} >"$LOG_FILE" 2>&1