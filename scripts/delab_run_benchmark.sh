#!/bin/bash

# Execute
# ./delab_run_benchmark.sh <benchmark_executable_name> <run_name>
# prerequisites:
#  - https://github.com/Tratori/prefetching @ $HOME directory in DeLab.
#  - ubuntu22_04.sqsh in $HOME directory.
#       (basic dependencies (g++, clang, make, ...))
#  - <benchmark_name> must be unique.


node_config=("cx15" "ca06" "cp01")
declare -A nodenames
declare -A partitions

partitions["cx17"]="magic"
nodenames["cx17"]="cx17"

partitions["cx16"]="magic"
nodenames["cx16"]="cx16"

partitions["cx15"]="magic"
nodenames["cx15"]="cx15"

partitions["ca06"]="magic"
nodenames["ca06"]="ca06"

partitions["cp01"]="alchemy"
nodenames["cp01"]="cp01"

# Check if benchmark with same name was already executed.
if [ -d "$HOME/prefetching/results/${2}" ]; then
    echo "Benchmark-Run with name ${2} was already executed!"
    exit 1
fi

cd ${HOME}/prefetching
git reset --hard
git pull


mkdir -p $HOME/prefetching/results/${2}
git rev-parse HEAD > $HOME/prefetching/results/${2}

for node_conf in ${node_config[@]}; do
    node=${nodenames[$node_conf]}
    RESULT_FILE="$HOME/prefetching/results/${2}/$node_conf"


    mkdir -p $HOME/prefetching/results/${2}/$node_conf

    echo "submitting task for config ${node_conf}"
    srun -A rabl --partition ${partitions[$node_conf]} -w $node -c 16 --mem-per-cpu 1024 \
      --time=6:00:00 --container-image=${HOME}/ubuntu22_04.sqsh \
      --container-mounts=${HOME}/prefetching:/prefetching  \
      /prefetching/scripts/delab_benchmark_pipeline.sh ${node_conf} ${1} ${2}  &
done