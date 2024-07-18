import json
import os
import matplotlib.pyplot as plt
import seaborn as sns


from helper import *


pc_access_size_ids = ["pc_amd.json"]


def plot_memory_latencies():
    df = load_results_benchmark_directory_to_pandas(f"{DATA_DIR}/full_latency")
    # Assuming your DataFrame is named df
    # Ensure the data types are correct
    df["config.access_range"] = df["config.access_range"].astype(int)
    df["config.alloc_on_node"] = df["config.alloc_on_node"].astype(int)
    df["config.run_on_node"] = df["config.run_on_node"].astype(int)
    df["config.madvise_huge_pages"] = df["config.madvise_huge_pages"].astype(bool)
    df["latency_single"] = df["latency_single"].astype(float)

    # Get unique ids
    unique_ids = df["id"].unique()

    # Set up the matplotlib figure
    fig, axes = plt.subplots(len(unique_ids), 1, figsize=(12, 8 * len(unique_ids)))

    if len(unique_ids) == 1:
        axes = [axes]

    # Loop through each unique id and plot
    for ax, unique_id in zip(axes, unique_ids):
        # Filter the DataFrame for the current id
        df_id = df[df["id"] == unique_id]

        # Group by 'config.alloc_on_node', 'config.run_on_node', and 'config.madvise_huge_pages'
        grouped = df_id.groupby(
            ["config.alloc_on_node", "config.run_on_node", "config.madvise_huge_pages"]
        )

        for (alloc_on_node, run_on_node, madvise_huge_pages), group in grouped:
            # Sort the group by 'config.access_range'
            group_sorted = group.sort_values(by="config.access_range")

            # Plot 'config.access_range' vs 'latency_single'
            ax.plot(
                group_sorted["config.access_range"],
                group_sorted["latency_single"],
                marker="o",
                label=f"alloc_on_node={alloc_on_node}, run_on_node={run_on_node}, madvise_huge_pages={madvise_huge_pages}",
            )

        ax.set_title(f"Performance for {unique_id}")
        ax.set_xlabel("Access Range")
        ax.set_ylabel("Latency (Single)")
        ax.legend()
        ax.grid(True)
        ax.set_xscale("log")

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    plot_memory_latencies()
