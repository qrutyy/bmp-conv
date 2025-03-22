import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os

RESULTS_FILE = "timing-results.dat"
PLOTS_PATH = "./plots/"

colors = ['green', 'red', 'blue', 'brown', 'purple', 'orange', 'pink']

plt.rcParams.update({'axes.titlesize': 14, 'axes.labelsize': 12, 'xtick.labelsize': 10, 'ytick.labelsize': 10})

df = pd.read_csv(RESULTS_FILE, sep=r"\s+", names=["RunID", "FILTER", "THREADNUM", "MODE", "BLOCK_SIZE", "TIME"])
print(df.head())

def clean_numeric(series):
    return pd.to_numeric(series, errors='coerce')

df["TIME"] = clean_numeric(df["TIME"])
df["BLOCK_SIZE"] = clean_numeric(df["BLOCK_SIZE"])
df = df.dropna()

# Process filter pairs
def process_filter_pairs(df):
    pairs = []
    times = []
    
    for i in range(0, len(df) - 1, 2):
        filter_pair = f"{df.iloc[i]['FILTER']}-{df.iloc[i + 1]['FILTER']}"
        total_time = df.iloc[i]['TIME'] + df.iloc[i + 1]['TIME']
        pairs.append(filter_pair)
        times.append(total_time)
    
    return pd.DataFrame({"PAIR": pairs, "TIME": times})

pair_df = process_filter_pairs(df)
pair_avg_df = pair_df.groupby("PAIR")["TIME"].mean().reset_index()

def plot_filter_pairs():
    plt.figure(figsize=(12, 8), dpi=300)
    plt.bar(pair_avg_df["PAIR"], pair_avg_df["TIME"], color='blue')
    plt.xlabel("Filter Pairs")
    plt.ylabel("Average Execution Time (seconds)")
    plt.title("Average Execution Time for Filter Pairs")
    plt.xticks(rotation=45, ha='right')
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    os.makedirs(PLOTS_PATH, exist_ok=True)
    save_path = os.path.join(PLOTS_PATH, "filter_pairs_execution_time.png")
    plt.savefig(save_path, bbox_inches='tight')
    plt.close()
    print(f"Saved: {save_path}")

plot_filter_pairs()
print("Plotting complete.")

