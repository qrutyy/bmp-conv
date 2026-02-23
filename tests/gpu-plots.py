#!/usr/bin/env python3
"""
Generate plots for GPU benchmark results.
Reads timing-results.dat and plots rows where BLOCK_SIZE is in GPU block set (4,8,16,32,64,128).
Run from project root: python3 tests/gpu-plots.py
"""

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
import scipy.stats as stats

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_FILE = os.path.join(SCRIPT_DIR, "logs", "gpu-timing-results.dat")
PLOTS_PATH = os.path.join(SCRIPT_DIR, "plots", "gpu")

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

# Block sizes used in gpu-benchmark.sh — only these rows are treated as GPU runs
GPU_BLOCK_SIZES = (1, 4, 8, 16, 32, 64, 128)

FILTER_DISPLAY_NAMES = {
    "mb": "Motion Blur",
    "bb": "Basic Blur",
    "gb": "Gaussian Blur",
    "em": "Emboss",
    "mm": "Median",
    "sh": "Sharpen",
    "bo": "Box Blur",
    "mg": "Medium Gaussian",
    "gg": "Big Gaussian",
    "co": "Convolution",
}

plt.rcParams.update(
    {
        "axes.titlesize": 14,
        "axes.labelsize": 12,
        "xtick.labelsize": 10,
        "ytick.labelsize": 10,
    }
)


def load_gpu_df():
    if not os.path.isfile(RESULTS_FILE):
        raise FileNotFoundError(
            f"Results file not found: {RESULTS_FILE} (run from project root or tests/)"
        )
    df = pd.read_csv(
        RESULTS_FILE,
        sep=r"\s+",
        skiprows=1,
        names=COLUMNS,
    )
    df["Result"] = pd.to_numeric(df["Result"], errors="coerce")
    df["BlockSize"] = pd.to_numeric(df["BlockSize"], errors="coerce")
    df = df.dropna(subset=["Result", "BlockSize"])
    gpu = df[df["BlockSize"].isin(GPU_BLOCK_SIZES)].copy()
    return gpu


def confidence_interval_half(data):
    data = data.dropna()
    if len(data) < 2:
        return 0.0
    mean, sigma = np.mean(data), np.std(data)
    ci = stats.t.interval(0.95, len(data) - 1, loc=mean, scale=stats.sem(data))
    return (ci[1] - ci[0]) / 2.0


def plot_time_vs_block_size(gpu_df):
    """Time vs block size (work group size) per filter — main GPU benchmark plot."""
    os.makedirs(PLOTS_PATH, exist_ok=True)
    filters = sorted(gpu_df["Filter"].unique())
    if not filters:
        print(
            "No GPU data found (BlockSize in 4,8,16,32,64,128). Skip time vs block size."
        )
        return

    fig, ax = plt.subplots(figsize=(12, 7), dpi=150)
    block_order = sorted(gpu_df["BlockSize"].unique(), key=int)

    for f in filters:
        sub = gpu_df[gpu_df["Filter"] == f].groupby("BlockSize")["Result"]
        means = sub.mean().reindex(block_order)
        errs = sub.apply(confidence_interval_half).reindex(block_order)
        means = means.fillna(0)
        errs = errs.fillna(0)
        label = FILTER_DISPLAY_NAMES.get(f, f)
        ax.errorbar(
            block_order,
            means,
            yerr=errs,
            marker="o",
            capsize=4,
            label=label,
        )

    ax.set_xlabel("Work group size")
    ax.set_ylabel("Execution time (s)")
    ax.set_title("GPU: execution time vs block size (OpenCL work group)")
    ax.set_xticks(block_order)
    ax.legend(loc="best", fontsize=9)
    ax.grid(True, linestyle="--", alpha=0.6)
    fig.tight_layout()
    path = os.path.join(PLOTS_PATH, "gpu_time_vs_block_size.png")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {path}")


def plot_mean_time_per_filter(gpu_df):
    """Bar chart: mean execution time per filter (averaged over block sizes)."""
    if gpu_df.empty:
        return
    os.makedirs(PLOTS_PATH, exist_ok=True)
    by_filter = gpu_df.groupby("Filter")["Result"]
    means = by_filter.mean().sort_values()
    errs = by_filter.apply(confidence_interval_half).reindex(means.index)
    labels = [FILTER_DISPLAY_NAMES.get(f, f) for f in means.index]

    fig, ax = plt.subplots(figsize=(12, 6), dpi=150)
    x = np.arange(len(means))
    ax.bar(x, means.values, yerr=errs.values, capsize=5)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right")
    ax.set_ylabel("Mean execution time (s)")
    ax.set_title("GPU: mean execution time per filter (all block sizes)")
    fig.tight_layout()
    path = os.path.join(PLOTS_PATH, "gpu_mean_time_per_filter.png")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {path}")


def plot_per_filter_block_bars(gpu_df):
    """One bar chart per filter: time vs block size (for detailed per-filter view)."""
    if gpu_df.empty:
        return
    os.makedirs(PLOTS_PATH, exist_ok=True)
    subdir = os.path.join(PLOTS_PATH, "per_filter")
    os.makedirs(subdir, exist_ok=True)
    block_order = sorted(gpu_df["BlockSize"].unique(), key=int)

    for f in gpu_df["Filter"].unique():
        sub = gpu_df[gpu_df["Filter"] == f]
        by_block = sub.groupby("BlockSize")["Result"]
        means = by_block.mean().reindex(block_order).fillna(0)
        errs = by_block.apply(confidence_interval_half).reindex(block_order).fillna(0)

        fig, ax = plt.subplots(figsize=(8, 5), dpi=150)
        ax.bar(
            means.index.astype(int).astype(str),
            means.values,
            yerr=errs.values,
            capsize=5,
        )
        ax.set_xlabel("Block size")
        ax.set_ylabel("Execution time (s)")
        ax.set_title(f"GPU: {FILTER_DISPLAY_NAMES.get(f, f)} — time vs block size")
        fig.tight_layout()
        path = os.path.join(subdir, f"gpu_{f}_time_vs_block.png")
        fig.savefig(path, bbox_inches="tight")
        plt.close(fig)
        print(f"Saved: {path}")


def main():
    gpu_df = load_gpu_df()
    if gpu_df.empty:
        print(
            "No GPU benchmark rows in timing-results.dat (expected BLOCK_SIZE in 1,4,8,16,32,64,128)."
        )
        return
    plot_time_vs_block_size(gpu_df)
    plot_mean_time_per_filter(gpu_df)
    plot_per_filter_block_bars(gpu_df)
    print("GPU plotting complete.")


if __name__ == "__main__":
    main()
