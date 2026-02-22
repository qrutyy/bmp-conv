import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_FILE = os.path.join(SCRIPT_DIR, "logs", "cpu-timing-results.dat")
PLOTS_PATH = os.path.join(SCRIPT_DIR, "plots")

# Unified log columns: RunID ProcessNum Backend Mode Filter ThreadNum ComputeMode BlockSize Result
COLUMNS = ["RunID", "ProcessNum", "Backend", "Mode", "Filter", "ThreadNum", "ComputeMode", "BlockSize", "Result"]

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
df["Result"] = pd.to_numeric(df["Result"], errors="coerce")
df["BlockSize"] = pd.to_numeric(df["BlockSize"], errors="coerce")
df = df.dropna()

allowed_pairs = {
    ("gb", "sh"),
    ("sh", "gb"),
    ("mb", "sh"),
    ("sh", "mb"),
    ("gg", "sh"),
    ("sh", "gg"),
}


def process_filter_pairs(df):
    pairs = []
    times = []
    pair_order = {}

    for i in range(len(df) - 1):
        f1, f2 = df.iloc[i]["Filter"], df.iloc[i + 1]["Filter"]

        if (f1, f2) in allowed_pairs:
            filter_pair = f"{f1}-{f2}"
            total_time = df.iloc[i]["Result"] + df.iloc[i + 1]["Result"]

            if filter_pair not in pair_order:
                pair_order[filter_pair] = len(pair_order)

            pairs.append(filter_pair)
            times.append(total_time)

    pair_df = pd.DataFrame({"PAIR": pairs, "TIME": times})
    print(pair_df)
    return pair_df, pair_order


pair_df, pair_order = process_filter_pairs(df)
pair_avg_df = pair_df.groupby("PAIR")["TIME"].mean().reset_index()
print(pair_avg_df)
pair_avg_df["ORDER"] = pair_avg_df["PAIR"].map(pair_order)
pair_avg_df = pair_avg_df.sort_values(by="ORDER")


def plot_filter_pairs():
    plt.figure(figsize=(12, 8), dpi=300)
    print(pair_avg_df["PAIR"])
    plt.bar(pair_avg_df["PAIR"], pair_avg_df["TIME"], color="blue")
    plt.xlabel("Filter Pairs")
    plt.ylabel("Average Execution Time (seconds)")
    plt.title("Average Execution Time for Filter Combinations")
    plt.xticks(rotation=45, ha="right")
    plt.grid(axis="y", linestyle="--", alpha=0.7)

    os.makedirs(PLOTS_PATH, exist_ok=True)
    save_path = os.path.join(PLOTS_PATH, "filter_pairs_execution_time.png")
    plt.savefig(save_path, bbox_inches="tight")
    plt.close()
    print(f"Saved: {save_path}")


plot_filter_pairs()
print("Plotting complete.")
