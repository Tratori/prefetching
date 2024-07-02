import json
import os
import matplotlib.pyplot as plt
import seaborn as sns


from helper import *


ids = ["amd", "intel", "local"]
filenames_interleaving = {
    "amd": "lfb_size_simple_interleaving_amd.json",
    "intel": "lfb_size_simple_interleaving_intel.json",
    "local": "lfb_size_simple_interleaving_local.json",
}


def plot_interleaved():
    for id in ids:
        lfb_bench = import_benchmark(
            os.path.join("lfb_interleaved", filenames_interleaving[id])
        )
        results = lfb_bench["results"]
        results = sorted(
            results, key=lambda result: result["config"]["prefetch_distance"]
        )
        prefetching_distances = [
            result["config"]["prefetch_distance"] for result in results
        ]
        runtime = [result["runtime"] for result in results]

        sns.pointplot(x=prefetching_distances, y=runtime)
    plt.title("LFB Size Benchmark - Interleaved")
    plt.xlabel("Prefetch Distance")
    plt.ylabel("Runtime (s)")

    plt.show()


ids = ["amd", "intel", "local"]
filenames_batched = {
    "amd": "lfb_size_batched_amd.json",
    "intel": "lfb_size_batched_intel.json",
    "local": "lfb_size_batched_local.json",
}


def plot_batched():
    for id in ["local"]:
        lfb_bench = import_benchmark(os.path.join("lfb_batched", filenames_batched[id]))
        results = lfb_bench["results"]
        results = sorted(results, key=lambda result: result["config"]["batch_size"])
        prefetching_distances = [result["config"]["batch_size"] for result in results]
        runtime = [result["runtime"] for result in results]

        sns.pointplot(x=prefetching_distances, y=runtime)
    plt.title("LFB Size Benchmark - Batched")
    plt.xlabel("Batch Size")
    plt.ylabel("Runtime (s)")
    plt.show()


if __name__ == "__main__":
    plot_interleaved()
    plot_batched()
