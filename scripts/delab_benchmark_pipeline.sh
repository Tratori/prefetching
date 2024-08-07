#!/bin/bash

# Execute
# ./delab_benchmark_pipeline.sh <node_name> <benchmark_executable_name> <run_name> <... additional args ...>

LOG_FILE="/prefetching/results/${3}/${1}/run_log.txt"
RESULT_FILE="/prefetching/results/${3}/${1}/${2}.json"
{
    cd /prefetching

    mkdir -p cmake-build-release-${1}
    cd cmake-build-release-${1}

    cmake -DCMAKE_BUILD_TYPE=Release ..
    if [ $? -ne 0 ]; then
        echo "cmake command failed" >&2
        exit 1
    fi

    make -j
    if [ $? -ne 0 ]; then
        echo "make command failed" >&2
        exit 1
    fi

    if ./benchmark/${2} --out="$RESULT_FILE" ${@:4}; then
        echo "success" >&2
    else
        echo "error" >&2
    fi
} >"$LOG_FILE" 2>&1