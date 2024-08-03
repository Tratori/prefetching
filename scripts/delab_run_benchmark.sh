#!/bin/bash

# Execute
# ./delab_run_benchmark.sh <benchmark_executable_name> <run_name> <... additional args ...>
# prerequisites:
#  - https://github.com/Tratori/prefetching @ $HOME directory in DeLab.
#  - ubuntu22_04.sqsh in $HOME directory.
#       (basic dependencies (g++, clang, make, ...))
#  - <run_name> must be unique.


node_config=("cx04" "cx28" "nx06" "ca06")
declare -A nodenames
declare -A partitions
declare -A num_cpus
declare -A images
declare -A arch

images["x86_64"]="ubuntu22_04.sqsh"
images["aarch64"]="arm_ubuntu22_04.sqsh"

partitions["cx04"]="magic"
nodenames["cx04"]="cx04"
num_cpus["cx04"]="72"
arch["cx04"]="x86_64"

partitions["nx05"]="alchemy"
nodenames["nx05"]="nx05"
num_cpus["nx05"]="128"
arch["nx05"]="x86_64"

partitions["nx06"]="alchemy"
nodenames["nx06"]="nx06"
num_cpus["nx06"]="128"
arch["nx06"]="x86_64"


partitions["cx15"]="magic"
nodenames["cx15"]="cx15"
num_cpus["cx15"]="72"
arch["cx15"]="x86_64"


partitions["cx16"]="magic"
nodenames["cx16"]="cx16"
num_cpus["cx16"]="72"
arch["cx16"]="x86_64"


partitions["cx17"]="magic"
nodenames["cx17"]="cx17"
num_cpus["cx17"]="256"
arch["cx17"]="x86_64"


partitions["cx28"]="magic"
nodenames["cx28"]="cx28"
num_cpus["cx28"]="256"
arch["cx28"]="x86_64"


partitions["ca06"]="magic"
nodenames["ca06"]="ca06"
num_cpus["ca06"]="19" # TODO make this 48 somehow
arch["ca06"]="aarch64"


partitions["cp01"]="alchemy"
nodenames["cp01"]="cp01"

# Check if benchmark with same name was already executed.
if [ -d "$HOME/prefetching/results/${2}" ]; then
    echo "Benchmark-Run with name ${2} was already executed!"
    exit 1
fi

cd ${HOME}/prefetching
git reset --hard
git pull --recurse-submodules
git submodule update

mkdir -p $HOME/prefetching/results/${2}
git rev-parse HEAD > $HOME/prefetching/results/${2}/git_sha
echo "$@"  > $HOME/prefetching/results/${2}/run_arguments

for node_conf in ${node_config[@]}; do
    node=${nodenames[$node_conf]}
    RESULT_FILE="$HOME/prefetching/results/${2}/$node_conf"


    mkdir -p $HOME/prefetching/results/${2}/$node_conf

    echo "submitting task for config ${node_conf}"
    srun -A rabl --partition ${partitions[$node_conf]} -w $node -c ${num_cpus[$node_conf]} \
      --time=36:00:00 --container-image=${HOME}/${images[${arch[$node_conf]}]} \
      --container-mounts=${HOME}/prefetching:/prefetching  \
      bash /prefetching/scripts/delab_benchmark_pipeline.sh ${node_conf} ${1} ${2} ${@:3} &
done