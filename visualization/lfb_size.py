import json
import os
import matplotlib.pyplot as plt
import seaborn as sns


from helper import *


if __name__ == "__main__":
    lfb_bench = import_benchmark("lfb_size_simple_interleaving.json")
    results = lfb_bench["results"]
    results = sorted(results, key=lambda result: result["config"]["prefetch_distance"])
    prefetching_distances = [
        result["config"]["prefetch_distance"] for result in results
    ]
    runtime = [result["runtime"] for result in results]

    sns.pointplot(x=prefetching_distances, y=runtime)
    plt.title("LFB Size Benchmark")
    plt.xlabel("Prefetch Distance")
    plt.ylabel("Runtime (s)")

    plt.show()
