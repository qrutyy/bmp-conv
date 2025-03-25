import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
import scipy.stats as stats

RESULTS_FILE = "tests/timing-results.dat"
PLOTS_PATH = "./tests/plots/"
MT_PP = f"{PLOTS_PATH}mt/"
ST_PP = f"{PLOTS_PATH}st/"

colors = ['green', 'red', 'blue', 'brown', 'purple', 'orange', 'pink']

plt.rcParams.update({'axes.titlesize': 14, 'axes.labelsize': 12, 'xtick.labelsize': 10, 'ytick.labelsize': 10})

df = pd.read_csv(RESULTS_FILE, sep=r"\s+", skiprows=2, names=["RunID", "FILTER", "THREADNUM", "MODE", "BLOCK_SIZE", "TIME"])
df.loc[df["THREADNUM"] == 1, "MODE"] = None  # Remove MODE for single-threaded tests

def clean_numeric(series):
    return pd.to_numeric(series, errors='coerce')

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
        "co": "Standard Convolution"
    }
    return mapping.get(fn, fn)

df["TIME"] = clean_numeric(df["TIME"])
df["BLOCK_SIZE"] = clean_numeric(df["BLOCK_SIZE"])

def compute_confidence_interval(data):
    """Compute the 95% confidence interval for a dataset."""
    if len(data) > 1:
        return stats.t.ppf(0.975, df=len(data) - 1) * stats.sem(data)
    return 0  # No confidence interval for a single data point

def plot_single_thread():
    single_thread_df = df[df["THREADNUM"] == 1]
    plt.figure(figsize=(14, 8), dpi=300)
    
    filters = single_thread_df["FILTER"].unique()
    means = []
    conf_intervals = []

    for f in filters:
        times = single_thread_df[single_thread_df["FILTER"] == f]["TIME"]
        means.append(times.mean())
        conf_intervals.append(compute_confidence_interval(times))

    plt.bar([convert_fn(f) for f in filters], means, yerr=conf_intervals, color=colors[:len(filters)], capsize=5)

    plt.xlabel("Filter")
    plt.ylabel("Execution Time (seconds)")
    plt.title("Execution Time for Different Filters (Single Thread)")
    plt.xticks(rotation=45)
    save_path = os.path.join(ST_PP, "all_filters_execution_time.png")
    plt.savefig(save_path, bbox_inches='tight')
    plt.close()
    print(f"Saved: {save_path}")

def plot_filter(filter_name):
    filter_df = df[(df["FILTER"] == filter_name) & (df["THREADNUM"] > 1)]
    if filter_df.empty:
        return
    
    plt.figure(figsize=(14, 8), dpi=300)
    con_filter_name = convert_fn(filter_name)
    
    block_sizes = sorted(filter_df["BLOCK_SIZE"].unique())
    modes = filter_df["MODE"].unique()
    x = np.arange(len(modes))
    width = 0.15  # Width of bars

    for i, block_size in enumerate(block_sizes):
        subset = filter_df[filter_df["BLOCK_SIZE"] == block_size].groupby("MODE")["TIME"]
        means = subset.mean()
        conf_intervals = subset.apply(compute_confidence_interval)

        plt.bar(x + i * width, means, yerr=conf_intervals, width=width, label=f"Block Size {int(block_size)}", capsize=5)

    plt.xlabel("Computation Mode")
    plt.ylabel("Execution Time (seconds)")
    plt.title(f"Execution Time vs Computation Mode for {con_filter_name}")
    plt.xticks(x + width / 2, modes)
    plt.legend()

    save_path = os.path.join(MT_PP, f"{filter_name}_execution_time_vs_mode.png")
    plt.savefig(save_path, bbox_inches='tight')
    plt.close()
    print(f"Saved: {save_path}")

plot_single_thread()
for filter_name in df["FILTER"].unique():
    plot_filter(filter_name)

print("Plotting complete.")

