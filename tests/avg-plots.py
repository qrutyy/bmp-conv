import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
import scipy.stats as stats
import argparse

parser = argparse.ArgumentParser(
    prog="avg_plots.py",
    description="Generate average execution time plots",
    epilog="Text at the bottom of help",
)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_FILE = os.path.join(SCRIPT_DIR, "logs", "cpu-timing-results.dat")
PLOTS_PATH = os.path.join(SCRIPT_DIR, "plots")
MT_PP = os.path.join(PLOTS_PATH, "mt")
ST_PP = os.path.join(PLOTS_PATH, "st")

# Unified log columns: RunID ProcessNum Backend Mode Filter ThreadNum ComputeMode BlockSize Result
COLUMNS = [
    "RunID",
    "ProcessNum",
    "Backend",
    "Mode",
    "Filter",
    "ThreadNum",
    "ComputeMode",
    "BlockSize",
    "Result",
]

colors = ["green", "red", "blue", "brown", "purple", "orange", "pink"]

plt.rcParams.update(
    {
        "axes.titlesize": 14,
        "axes.labelsize": 12,
        "xtick.labelsize": 10,
        "ytick.labelsize": 10,
    }
)

df = pd.read_csv(
    RESULTS_FILE,
    sep=r"\s+",
    skiprows=1,
    names=COLUMNS,
)
# Normal/CPU single-thread: no compute mode label
df.loc[df["ThreadNum"] == 1, "ComputeMode"] = "single_thread"
df.loc[df["ThreadNum"] == 1, "BlockSize"] = 0


def clean_numeric(series):
    return pd.to_numeric(series, errors="coerce")


def convert_fn(fn):
    mapping = {
        "mb": "Motion Blur",
        "bb": "Basic Blur",
        "gb": "Gaussian Blur",
        "em": "Emboss Filter",
        "mm": "Median Filter",
        "sh": "Sharpen Filter",
        "bo": "Box Blur",
        "mg": "Medium Gaussian Blur",
        "gg": "Big Gaussian Blur",
        "co": "Standard Convolution",
    }
    return mapping.get(fn, fn)


df["Result"] = clean_numeric(df["Result"])
df["BlockSize"] = clean_numeric(df["BlockSize"])


def compute_confidence_interval(data):
    data = data.dropna()
    if len(data) > 1:
        mean, sigma = np.mean(data), np.std(data)
        conf_int = stats.t.interval(
            0.95, len(data) - 1, loc=mean, scale=stats.sem(data)
        )
        return (conf_int[1] - conf_int[0]) / 2.0
    return 0


def plot_single_thread():
    single_thread_df = df[df["ThreadNum"] == 1]
    plt.figure(figsize=(14, 8), dpi=300)

    filters = single_thread_df["Filter"].unique()
    means = []
    conf_intervals = []

    for f in filters:
        times = single_thread_df[single_thread_df["Filter"] == f]["Result"]
        means.append(times.mean())
        conf_intervals.append(compute_confidence_interval(times))

    plt.bar(
        [convert_fn(f) for f in filters],
        means,
        yerr=conf_intervals,
        color=colors[: len(filters)],
        capsize=5,
    )

    plt.xlabel("Filter")
    plt.ylabel("Execution Time (seconds)")
    plt.title("Execution Time for Different Filters (Single Thread)")
    plt.xticks(rotation=45)
    save_path = os.path.join(ST_PP, "all_filters_execution_time.png")
    plt.savefig(save_path, bbox_inches="tight")
    plt.close()
    print(f"Saved: {save_path}")


def plot_filter(filter_name):
    filter_df = df[(df["Filter"] == filter_name) & (df["ThreadNum"] > 1)].copy()
    if filter_df.empty:
        print(f"No multi-threaded data found for filter: {filter_name}")
        return

    plt.figure(figsize=(14, 8), dpi=300)
    con_filter_name = convert_fn(filter_name)

    block_sizes = sorted(filter_df["BlockSize"].unique())
    canonical_modes = ["by_row", "by_column", "by_grid"]

    modes_in_data = filter_df["ComputeMode"].unique()
    modes_to_plot = [m for m in canonical_modes if m in modes_in_data]

    if not modes_to_plot:
        print(f"No valid modes found in data for filter: {filter_name}")
        plt.close()
        return

    x = np.arange(len(modes_to_plot))
    num_block_sizes = len(block_sizes)
    total_width_per_group = 0.8
    width = total_width_per_group / num_block_sizes if num_block_sizes > 0 else 0.15
    start_offset = -(total_width_per_group / 2) + (width / 2)

    for i, block_size in enumerate(block_sizes):
        subset = filter_df[filter_df["BlockSize"] == block_size].groupby("ComputeMode")[
            "Result"
        ]
        means = subset.mean().reindex(modes_to_plot)
        conf_intervals = subset.apply(compute_confidence_interval).reindex(
            modes_to_plot
        )

        plt.bar(
            x + start_offset + i * width,
            means.fillna(0),
            yerr=conf_intervals.fillna(0),
            width=width,
            label=f"Block Size {int(block_size)}",
            capsize=5,
        )

    plt.xlabel("Computation Mode")
    plt.ylabel("Execution Time (seconds)")
    plt.title(f"Execution Time vs Computation Mode for {con_filter_name}")
    plt.xticks(x, modes_to_plot)
    plt.legend(title="Block Size")
    plt.tight_layout()

    os.makedirs(MT_PP, exist_ok=True)
    save_path = os.path.join(MT_PP, f"{filter_name}_execution_time_vs_mode.png")
    plt.savefig(save_path, bbox_inches="tight")
    plt.close()
    print(f"Saved: {save_path}")


os.makedirs(MT_PP, exist_ok=True)
os.makedirs(ST_PP, exist_ok=True)

plot_single_thread()
for filter_name in df["Filter"].unique():
    if df[(df["Filter"] == filter_name) & (df["ThreadNum"] > 1)].empty:
        continue
    plot_filter(filter_name)

print("Plotting complete.")
