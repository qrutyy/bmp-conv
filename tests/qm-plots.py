import argparse
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os

RESULTS_FILE = "tests/queue-timings.dat"
PLOTS_PATH = "./tests/plots/q-mode/"

parser = argparse.ArgumentParser(description="Generate average plots from fio results.")
parser.add_argument("--mix", type=str, default="1,1,1", help="Thread roled mix")
args = parser.parse_args()

try:
    colors = plt.cm.viridis.colors
except AttributeError:
    colors = plt.cm.tab10.colors

plt.rcParams.update(
    {
        "axes.titlesize": 16,
        "axes.labelsize": 14,
        "xtick.labelsize": 12,
        "ytick.labelsize": 12,
        "figure.figsize": (12, 8),
        "figure.dpi": 150,
    }
)

try:
    df = pd.read_csv(
        RESULTS_FILE,
        sep=r"\s+",
        skipinitialspace=True,
        header=None,
        names=["LOG_TAG", "TIME"],
        comment="#",
        skip_blank_lines=True,
    )

except FileNotFoundError:
    print(f"Error: Results file not found at '{RESULTS_FILE}'")
    exit(1)
except pd.errors.EmptyDataError:
    print(
        f"Error: Results file '{RESULTS_FILE}' is empty or contains only comments/blank lines."
    )
    exit(1)
except Exception as e:
    print(f"Error reading file '{RESULTS_FILE}': {e}")
    exit(1)


df["TIME"] = pd.to_numeric(df["TIME"], errors="coerce")
df.dropna(subset=["TIME"], inplace=True)

if df.empty:
    print(f"Error: No valid numeric data found in '{RESULTS_FILE}' after cleaning.")
    exit(1)

print("Loaded DataFrame head:")
print(df.head())
print(f"\nTotal valid rows: {len(df)}")
print("Unique LOG_TAGs found:", df["LOG_TAG"].unique())
print("-" * 30)


def plot_filter_bars(filter_type, base_df):
    rww_s = args.mix.replace(",", "-")

    if filter_type == "q_block":
        tags_to_include = ["QPOP", "QPUSH"]
        plot_title = "Distribution of Queue Block Caused Operation Times (Individual Bars) - RWW Mix: {rww_s}"
        xlabel = "Queue Operation Type"
        ylabel = "Block Time (seconds)"
        filename_suffix = f"{rww_s}_qblock_ops_bars"

    elif filter_type == "q_threads":
        tags_to_include = ["WORKER", "READER", "WRITER"]
        plot_title = f"Distribution of Thread Execution Times (Individual Bars) - RWW Mix: {rww_s}"
        xlabel = "Thread Role"
        ylabel = "Execution Time (seconds)"
        filename_suffix = f"{rww_s}_thread_exec_bars"
    else:
        print(f"Error: Unknown filter_type '{filter_type}'")
        return

    filter_df = base_df[base_df["LOG_TAG"].isin(tags_to_include)].copy()

    if filter_df.empty:
        print(
            f"No data found for filter type '{filter_type}' with tags {tags_to_include}. Skipping plot."
        )
        return

    tags_to_plot = sorted(filter_df["LOG_TAG"].unique())
    if not tags_to_plot:
        print(f"No unique tags found to plot for filter type '{filter_type}'.")
        return

    print(f"Plotting individual bars for {filter_type}: Tags = {tags_to_plot}")

    plt.figure()
    ax = plt.gca()

    x_pos_map = {tag: i for i, tag in enumerate(tags_to_plot)}

    category_width = 0.8  # Total width allocated for bars within a category
    bar_width = 0

    for i, tag in enumerate(tags_to_plot):
        y_values = filter_df.loc[filter_df["LOG_TAG"] == tag, "TIME"].values
        num_points = len(y_values)

        if num_points == 0:
            continue

        x_category_center = x_pos_map[tag]

        bar_width = max(category_width / (num_points + 1), 0.01)  # Ensure non-zero

        start_x = x_category_center - category_width / 2 + bar_width / 2

        color = colors[i % len(colors)]

        for j, y_val in enumerate(y_values):
            x_bar_pos = start_x + j * (category_width / num_points)

            bar = ax.bar(
                x_bar_pos,
                y_val,
                width=bar_width * 0.95,
                color=color,
                label=tag,
                alpha=0.7,  # Add alpha for overlapping bars
            )

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(plot_title)

    ax.set_xticks(list(x_pos_map.values()))
    ax.set_xticklabels(list(x_pos_map.keys()))

    ax.grid(axis="y", linestyle="--", alpha=0.6)

    plt.tight_layout()

    os.makedirs(PLOTS_PATH, exist_ok=True)
    save_path = os.path.join(PLOTS_PATH, f"{filename_suffix}.png")
    try:
        plt.savefig(save_path, bbox_inches="tight")
        plt.close()
        print(f"Saved plot: {save_path}")
    except Exception as e:
        print(f"Error saving plot '{save_path}': {e}")
        plt.close()


os.makedirs(PLOTS_PATH, exist_ok=True)

plot_filter_bars("q_block", df)
plot_filter_bars("q_threads", df)

print("\nPlotting script finished.")
