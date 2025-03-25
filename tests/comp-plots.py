import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os

RESULTS_FILE = "tests/timing-results.dat"
PLOTS_PATH = "./tests/plots/"

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
    names=["RunID", "FILTER", "THREADNUM", "MODE", "BLOCK_SIZE", "TIME"],
)
df["TIME"] = pd.to_numeric(df["TIME"], errors="coerce")
df["BLOCK_SIZE"] = pd.to_numeric(df["BLOCK_SIZE"], errors="coerce")
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
        f1, f2 = df.iloc[i]["FILTER"], df.iloc[i + 1]["FILTER"]

        if (f1, f2) in allowed_pairs:
            filter_pair = f"{f1}-{f2}"
            total_time = df.iloc[i]["TIME"] + df.iloc[i + 1]["TIME"]

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
