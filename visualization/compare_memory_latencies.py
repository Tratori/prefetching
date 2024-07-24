import os

import matplotlib.pyplot as plt
import seaborn as sns

from helper import import_benchmark


COMPARISON_DIR = "tinymembench_pc_comparison"
VERTICAL_LINES = [(32000, "L1d"), (512000, "L2"), (16000000, "L3")]


def single_views():
    tinymembench = import_benchmark(os.path.join(COMPARISON_DIR, "tinymembench.json"))
    tinymembench_modified = import_benchmark(
        os.path.join(COMPARISON_DIR, "tinymembench_modified.json")
    )
    pc = import_benchmark(os.path.join(COMPARISON_DIR, "pc.json"))
    print(pc)

    benchmarks = {
        "Tinymembench": tinymembench,
        "Modified Tinymembench": tinymembench_modified,
        "Pointer Chase": pc,
    }

    fig, axes = plt.subplots(1, 3, figsize=(15, 5), sharey=True, sharex=True)

    for ax, (title, benchmark) in zip(axes, benchmarks.items()):
        sizes = [entry["size"] for entry in benchmark["results"]]
        latencies = [
            entry["single_latency"] if entry["single_latency"] > 0.1 else 0.1
            for entry in benchmark["results"]
        ]

        sns.scatterplot(x=sizes, y=latencies, ax=ax, marker="x")
        ax.set_title(title)
        ax.set_xlabel("Size")
        ax.set_xscale("log")
        ax.set_yscale("log")

        if ax == axes[0]:
            ax.set_ylabel("Single Latency")

    for ax in axes:
        for line_position, label in VERTICAL_LINES:
            ax.axvline(x=line_position, color="red", linestyle="--")
            ax.text(
                line_position * 1.2,
                ax.get_ylim()[0] + 0.1,
                label,
                color="red",
                verticalalignment="bottom",
                size=14,
            )
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    single_views()
