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


pc_ids = ["pc_local.json", "pc_amd.json", "pc_intel.json"]
def plot_pc():
    for id in pc_ids:
        lfb_bench = import_benchmark(os.path.join("pc_benchmark", id))
        results = lfb_bench["results"]
        results = sorted(
            results, key=lambda result: result["config"]["num_parallel_pc"]
        )
        num_parallel_pc = [result["config"]["num_parallel_pc"] for result in results]
        runtime = [result["runtime"] for result in results]
        if id == "pc_intel.json":
            runtime = [p[0] / p[1] for p in zip(runtime, num_parallel_pc)]

        sns.pointplot(x=num_parallel_pc, y=runtime, label=id)
    plt.title("LFB Size Benchmark - Pointer Chasing")
    plt.xlabel("Parallel Pointer Chases")
    plt.ylabel("Runtime (s)")
    # plt.yscale("log")
    plt.show()


pc_access_size_ids = ["pc_amd.json"]


def plot_pc_access_size():
    df = load_results_benchmark_directory_to_pandas(
        f"{DATA_DIR}/pc_varying_access_size"
    )
    print(df)
    df["config.accessed_cache_lines"] = df["config.accessed_cache_lines"].astype(int)
    df["config.num_parallel_pc"] = df["config.num_parallel_pc"].astype(int)
    df["runtime"] = df["runtime"].astype(float)

    unique_ids = df["id"].unique()

    fig, axes = plt.subplots(len(unique_ids), 1, figsize=(12, 8 * len(unique_ids)))

    if len(unique_ids) == 1:
        axes = [axes]

    # Loop through each unique id and plot
    for ax, unique_id in zip(axes, unique_ids):
        # Filter the DataFrame for the current id
        df_id = df[df["id"] == unique_id]

        # Get unique accessed_cache_lines
        accessed_cache_lines = df_id["config.accessed_cache_lines"].unique()

        # Loop through each accessed_cache_line and plot
        for line in accessed_cache_lines:
            df_line = df_id[df_id["config.accessed_cache_lines"] == line]
            df_line = df_line.sort_values(by="config.num_parallel_pc")

            ax.plot(
                df_line["config.num_parallel_pc"],
                df_line["runtime"],
                marker="o",
                label=f"Cache Lines: {line}",
            )

        ax.set_title(f"Performance for {unique_id}")
        ax.set_xlabel("Number of Parallel PCs")
        ax.set_ylabel("Runtime")
        ax.legend()
        ax.grid(True)

    plt.tight_layout()
    plt.subplots_adjust(hspace=0.3)  # Increase the space between subplots
    plt.show()


if __name__ == "__main__":
    plot_pc_access_size()
    # plot_pc()
    # plot_interleaved()
    # plot_batched()
