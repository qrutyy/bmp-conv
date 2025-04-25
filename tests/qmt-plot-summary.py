import argparse
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
import re

SUMMARY_FILE = (
    "tests/queue-summary-results.dat"  # Input: Aggregated results for all mixes
)
PLOTS_PATH = "./tests/plots/"  # Base path for summary plots

plt.rcParams.update(
    {
        "axes.titlesize": 18,
        "axes.labelsize": 16,
        "xtick.labelsize": 11,  # Smaller tick labels
        "ytick.labelsize": 12,
        "legend.fontsize": 12,  # Smaller legend
        "figure.figsize": (16, 10),  # Potentially wider figure
        "figure.dpi": 150,
    }
)
# Use a colormap with more distinct colors if many mixes/tags
colors = plt.cm.get_cmap("tab20").colors

# --- Data Reading ---
print(f"Reading summary data from: {SUMMARY_FILE}")
try:
    df_summary = pd.read_csv(
        SUMMARY_FILE,
        sep="\t",
        skipinitialspace=True,
        header=0,  # Use the first row as header
        comment="#",
        skip_blank_lines=True,
    )
except FileNotFoundError:
    print(f"Error: Summary file not found at '{SUMMARY_FILE}'")
    exit(1)
except pd.errors.EmptyDataError:
    print(f"Error: Summary file '{SUMMARY_FILE}' is empty.")
    exit(1)
except Exception as e:
    print(f"Error reading summary file '{SUMMARY_FILE}': {e}")
    exit(1)

df_summary["MIX_SORT_KEY"] = df_summary["MIX"]
df_summary = df_summary.sort_values(by="MIX_SORT_KEY").drop(columns=["MIX_SORT_KEY"])

print("\nLoaded Summary DataFrame head:")
print(df_summary.head())
print(f"\nTotal aggregated rows: {len(df_summary)}")
print("Unique COMPUTE_MODEs found:", df_summary["COMPUTE_MODE"].unique())
print("Unique MIXes found:", df_summary["MIX"].unique())
print("Unique LOG_TAGs found:", df_summary["LOG_TAG"].unique())
print("-" * 30)


def plot_summary_comparison(compute_mode, filter_type, summary_data):
    if filter_type == "q_block":
        tags_to_include = ["QPOP", "QPUSH"]
        base_title = f"Comparison of Avg Queue Ops Times (Mode: {compute_mode})"
        ylabel = "Average Operation Time (seconds)"
        filename_suffix = f"{compute_mode}_qops_summary_compare"
    elif filter_type == "q_threads":
        tags_to_include = ["WORKER", "READER", "WRITER"]
        base_title = f"Comparison of Avg Thread Execution Times (Mode: {compute_mode})"
        ylabel = "Average Execution Time (seconds)"
        filename_suffix = f"{compute_mode}_thread_summary_compare"
    else:
        print(f"Error: Unknown filter_type '{filter_type}'")
        return

    # Filter data for the specific compute_mode and tags
    plot_df = summary_data[
        (summary_data["COMPUTE_MODE"] == compute_mode)
        & (summary_data["LOG_TAG"].isin(tags_to_include))
    ].copy()

    if plot_df.empty:
        print(
            f"No summary data found for compute_mode='{compute_mode}', filter_type='{filter_type}'. Skipping plot."
        )
        return

    # Use MIX as the main category on X-axis

    mixes = plot_df["MIX"].unique()
    tags = sorted(plot_df["LOG_TAG"].unique())

    if len(mixes) == 0 or len(tags) == 0:
        print(
            f"No unique mixes or tags found for {compute_mode}/{filter_type} in summary."
        )
        return

    print(
        f"\nPlotting summary comparison for {compute_mode}/{filter_type}: Mixes={mixes}, Tags={tags}"
    )

    x = np.arange(len(mixes))
    num_tags = len(tags)
    width = 0.8 / num_tags  # Adjust width based on number of tags per mix

    fig, ax = plt.subplots(figsize=(max(12, 1.5 * len(mixes)), 9))  # Adjust figure size

    for i, tag in enumerate(tags):
        means = []
        stds = []
        for mix in mixes:
            # Find the row for this specific mix and tag
            row = plot_df[(plot_df["MIX"] == mix) & (plot_df["LOG_TAG"] == tag)]
            means.append(row["mean"].iloc[0] if not row.empty else 0)
            stds.append(row["std"].iloc[0] if not row.empty else 0)

        position = x - (0.8 / 2) + (i + 0.5) * width
        ax.bar(
            position,
            means,
            width * 0.9,
            yerr=stds,
            label=tag,
            color=colors[i % len(colors)],
            capsize=4,
            alpha=0.8,
        )

    ax.set_xlabel("Reader-Worker-Writer Mix")
    ax.set_ylabel(ylabel)
    ax.set_title(base_title + " Across Mixes")
    ax.set_xticks(x)
    ax.set_xticklabels(mixes, rotation=45, ha="right")
    ax.legend(title="Log Tag / Role")
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    summary_plot_path = os.path.join(PLOTS_PATH, "summary")
    os.makedirs(summary_plot_path, exist_ok=True)

    save_path = os.path.join(summary_plot_path, f"{filename_suffix}.png")
    try:
        plt.savefig(save_path, bbox_inches="tight")
        print(f"Saved summary plot: {save_path}")
    except Exception as e:
        print(f"Error saving plot '{save_path}': {e}")
    finally:
        plt.close(fig)


os.makedirs(PLOTS_PATH, exist_ok=True)

compute_modes_found = df_summary["COMPUTE_MODE"].unique()
print(f"\nGenerating summary plots for compute modes: {list(compute_modes_found)}")

for mode in compute_modes_found:
    print(f"--- Processing summary plots for Compute Mode: {mode} ---")
    plot_summary_comparison(mode, "q_block", df_summary)
    plot_summary_comparison(mode, "q_threads", df_summary)

print("\nSummary plotting script finished.")
