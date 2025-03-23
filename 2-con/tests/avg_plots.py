import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os

RESULTS_FILE = "tests/timing-results.dat"
PLOTS_PATH = "./tests/plots/"

colors = ['green', 'red', 'blue', 'brown', 'purple', 'orange', 'pink']

plt.rcParams.update({'axes.titlesize': 14, 'axes.labelsize': 12, 'xtick.labelsize': 10, 'ytick.labelsize': 10})

df = pd.read_csv(RESULTS_FILE, sep=r"\s+", names=["RunID", "FILTER", "THREADNUM", "MODE", "BLOCK_SIZE", "TIME"])
print(df.head())

def clean_numeric(series):
    return pd.to_numeric(series, errors='coerce')

def convert_fn(fn):
    if fn == "mb":
        return "Motion Blur"
    elif fn == "bb":
        return "Basic Blur"
    elif fn == "gb":
        return "Gaussian Blur"
    elif fn == "em":
        return "Embos Filter"
    elif fn == "mm":
        return "Median Filter"
    elif fn == "sh":
        return "Sharpen Filter"
    elif fn == "gg":
        return "Big Gaussian Blur"
    else:
        return fn


df["TIME"] = clean_numeric(df["TIME"])
df["BLOCK_SIZE"] = clean_numeric(df["BLOCK_SIZE"])
df = df.dropna()

def plot_filter(filter_name):
    filter_df = df[df["FILTER"] == filter_name]
    plt.figure(figsize=(14, 8), dpi=300)
    
    block_sizes = sorted(filter_df["BLOCK_SIZE"].unique())
    modes = filter_df["MODE"].unique()
    x = np.arange(len(modes))
    width = 0.15  # Width of bars
    
    for i, block_size in enumerate(block_sizes):
        subset = filter_df[filter_df["BLOCK_SIZE"] == block_size].groupby("MODE")["TIME"].mean()
        plt.bar(x + i * width, subset.values, width=width, label=f"Block Size {int(block_size)}")

    con_filter_name = convert_fn(filter_name)

    plt.xlabel("Computation Mode")
    plt.ylabel("Execution Time (seconds)")
    plt.title(f"Execution Time vs Computation Mode for {con_filter_name}")
    plt.xticks(x + width / 2, modes)
    plt.legend()
    
    if not subset.empty:
        plt.ylim(0, max(subset.values) * 1.1)
    
    save_path = os.path.join(PLOTS_PATH, f"{filter_name}_execution_time_vs_mode.png")
    plt.savefig(save_path, bbox_inches='tight')
    plt.close()
    print(f"Saved: {save_path}")

for filter_name in df["FILTER"].unique():
    plot_filter(filter_name)

print("Plotting complete.")

