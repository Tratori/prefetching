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
lfbs = {
    "amd": 22.0,
    "intel": 10.0,
    "local": 22.0,
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
            int(result["config"]["prefetch_distance"]) for result in results
        ]
        runtime = [result["runtime"] for result in results]

        pointplot = sns.pointplot(x=prefetching_distances, y=runtime, label=id)
        # color = pointplot.get_lines()[0].get_color()  # Get the color of the pointplot
        # sns.lineplot(x=[10, 10], y=[0, 0.3], color=color)

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
    for id in ids:
        lfb_bench = import_benchmark(os.path.join("lfb_batched", filenames_batched[id]))
        results = lfb_bench["results"]
        results = sorted(results, key=lambda result: result["config"]["batch_size"])
        prefetching_distances = [result["config"]["batch_size"] for result in results]
        runtime = [result["runtime"] for result in results]
        runtime = [p[0] * p[1] for p in zip(runtime, prefetching_distances)]

        sns.pointplot(x=prefetching_distances, y=runtime, label=id)
    plt.title("LFB Size Benchmark - Batched")
    plt.xlabel("Batch Size")
    plt.ylabel("Runtime (s)")
    # plt.yscale("log")
    plt.show()


if __name__ == "__main__":
    plot_interleaved()
    plot_batched()
