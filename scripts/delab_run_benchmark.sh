#!/bin/bash

# Execute
# ./delab_run_benchmark.sh <benchmark_executable_name> <run_name> <... additional args ...>
# prerequisites:
#  - https://github.com/Tratori/prefetching @ $HOME directory in DeLab.
#  - ubuntu22_04.sqsh in $HOME directory.
#       (basic dependencies (g++, clang, make, ...))
#  - <run_name> must be unique.


node_config=("cx04" "cx28")
declare -A nodenames
declare -A partitions
declare -A num_cpus

partitions["cx04"]="magic"
nodenames["cx04"]="cx14"
num_cpus["cx04"]="72"

partitions["cx15"]="magic"
nodenames["cx15"]="cx15"
num_cpus["cx15"]="72"

partitions["cx16"]="magic"
nodenames["cx16"]="cx16"
num_cpus["cx16"]="72"

partitions["cx17"]="magic"
nodenames["cx17"]="cx17"
num_cpus["cx17"]="256"

partitions["cx28"]="magic"
nodenames["cx28"]="cx28"
num_cpus["cx28"]="256"

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
git submodule update

mkdir -p $HOME/prefetching/results/${2}
git rev-parse HEAD > $HOME/prefetching/results/${2}/git_sha

for node_conf in ${node_config[@]}; do
    node=${nodenames[$node_conf]}
    RESULT_FILE="$HOME/prefetching/results/${2}/$node_conf"


    mkdir -p $HOME/prefetching/results/${2}/$node_conf

    echo "submitting task for config ${node_conf}"
    srun -A rabl --partition ${partitions[$node_conf]} -w $node -c ${num_cpus[$node_conf]} \
      --time=6:00:00 --container-image=${HOME}/ubuntu22_04.sqsh \
      --container-mounts=${HOME}/prefetching:/prefetching  \
      /prefetching/scripts/delab_benchmark_pipeline.sh ${node_conf} ${1} ${2} ${@:3} &
done