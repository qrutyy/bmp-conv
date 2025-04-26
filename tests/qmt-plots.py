import argparse
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
import re

TIMING_FILE = "tests/queue-timings.dat"
SUMMARY_FILE = "tests/queue-summary-results.dat"
PLOTS_PATH = "./tests/plots/q-mode/"

parser = argparse.ArgumentParser(
    description="Process timing data for a single mix and append aggregates."
)
parser.add_argument(
    "--mix", type=str, required=True, help="R,W,T mix used for this run (e.g., 1,2,1)"
)
parser.add_argument(
    "--no-plot",
    action="store_true",
    help="Skip generating the individual plot for this mix",
)
args = parser.parse_args()
rww_s = args.mix.replace(",", "-")  # Format mix for filenames/titles

# Plot Settings (only needed if generating per-mix plots)
if not args.no_plot:
    plt.rcParams.update(
        {
            "axes.titlesize": 18,
            "axes.labelsize": 16,
            "xtick.labelsize": 12,
            "ytick.labelsize": 12,
            "legend.fontsize": 14,
            "figure.figsize": (12, 8),
            "figure.dpi": 150,
        }
    )
    colors = plt.cm.get_cmap("tab10").colors

print(f"Reading data for mix {args.mix} from: {TIMING_FILE}")
try:
    df = pd.read_csv(
        TIMING_FILE,
        sep=r"\s+",
        skipinitialspace=True,
        header=None,
        names=["COMPUTE_MODE", "LOG_TAG", "TIME"],
        comment="#",
        skip_blank_lines=True,
    )
except FileNotFoundError:
    print(f"Error: Timings file not found at '{TIMING_FILE}'")
    exit(1)
except pd.errors.EmptyDataError:
    print(f"Error: Timings file '{TIMING_FILE}' is empty.")
    exit(1)  # Stop processing for this mix if no data
except Exception as e:
    print(f"Error reading file '{TIMING_FILE}': {e}")
    exit(1)

df["TIME"] = pd.to_numeric(df["TIME"], errors="coerce")
df.dropna(subset=["TIME"], inplace=True)

if df.empty:
    print(f"Error: No valid numeric data found in '{TIMING_FILE}' after cleaning.")
    exit(1)  # Stop processing for this mix

compute_modes_found = df["COMPUTE_MODE"].unique()
if len(compute_modes_found) == 0:
    print(f"Error: No COMPUTE_MODE found in data for mix {args.mix}.")
    exit(1)
# If multiple compute modes are somehow in the file, just use the first one
compute_mode = compute_modes_found[0]
print(f"Processing data for: Mix={rww_s}, Compute Mode={compute_mode}")


df_agg = df.groupby(["LOG_TAG"])["TIME"].agg(["mean", "std"]).reset_index()
df_agg["std"] = df_agg["std"].fillna(0)
# Add Mix and Compute Mode columns for the summary file
df_agg["MIX"] = rww_s
df_agg["COMPUTE_MODE"] = compute_mode

print("\nAggregated Data for this run:")
print(df_agg[["MIX", "COMPUTE_MODE", "LOG_TAG", "mean", "std"]])

print(f"Appending results to: {SUMMARY_FILE}")
try:
    # Check if file exists to determine if header is needed
    header_needed = (
        not os.path.exists(SUMMARY_FILE) or os.path.getsize(SUMMARY_FILE) == 0
    )

    # Append data, write header only if needed
    df_agg[["MIX", "COMPUTE_MODE", "LOG_TAG", "mean", "std"]].to_csv(
        SUMMARY_FILE,
        mode="a",  # Append mode
        sep="\t",  # Use tab separation for robustness
        header=header_needed,  # Write header only if file is new/empty
        index=False,  # Do not write pandas index
        float_format="%.6f",  # Format float precision
    )
    print("Successfully appended aggregated data.")
except Exception as e:
    print(f"Error writing to summary file '{SUMMARY_FILE}': {e}")

# Optional: Generate Per-Mix Plot
if not args.no_plot:
    print("\nGenerating individual plot for this mix...")

    def plot_average_bars(
        filter_type, data_agg_plot, current_compute_mode, rww_string_plot
    ):
        if filter_type == "q_block":
            tags_to_include = ["QPOP", "QPUSH"]
            plot_title = f"Average Queue Operations Time (Mode: {current_compute_mode}, Mix: {rww_string_plot})"
            ylabel = "Average Operation Time (seconds)"
            xlabel = "Queue Operation"
            filename_suffix = f"{current_compute_mode}_{rww_string_plot}_qblock_ops_avg"
        elif filter_type == "q_threads":
            tags_to_include = ["WORKER", "READER", "WRITER"]
            plot_title = f"Average Thread Execution Times (Mode: {current_compute_mode}, Mix: {rww_string_plot})"
            ylabel = "Average Execution Time (seconds)"

            xlabel = "Thread Role"
            filename_suffix = (
                f"{current_compute_mode}_{rww_string_plot}_thread_exec_avg"
            )
        else:
            print(f"Error: Unknown filter_type '{filter_type}'")
            return

        plot_df = data_agg_plot[data_agg_plot["LOG_TAG"].isin(tags_to_include)].copy()
        if plot_df.empty:
            print(
                f"No aggregated data for tags {tags_to_include}. Skipping '{filter_type}' plot."
            )
            return
        tags = sorted(plot_df["LOG_TAG"].unique())
        means = plot_df["mean"]
        stds = plot_df["std"]
        x = np.arange(len(tags))
        width = 0.6
        fig, ax = plt.subplots(figsize=(12, 8))  # Use fixed size from params
        ax.bar(
            x, means, width, yerr=stds, color=colors[: len(tags)], capsize=5, alpha=0.8
        )
        ax.set_ylabel(ylabel)
        ax.set_xlabel(xlabel)
        ax.set_title(plot_title)
        ax.set_xticks(x)
        ax.set_xticklabels(tags)
        ax.grid(axis="y", linestyle="--", alpha=0.7)
        plt.tight_layout()
        mode_plot_path = os.path.join(PLOTS_PATH, current_compute_mode)
        os.makedirs(mode_plot_path, exist_ok=True)
        save_path = os.path.join(mode_plot_path, f"{filename_suffix}.png")
        try:
            plt.savefig(save_path, bbox_inches="tight")
            print(f"Saved individual plot: {save_path}")
        except Exception as e:
            print(f"Error saving individual plot '{save_path}': {e}")
        finally:
            plt.close(fig)  # Ensure figure is closed

    plot_average_bars("q_block", df_agg, compute_mode, rww_s)
    plot_average_bars("q_threads", df_agg, compute_mode, rww_s)

print(f"\nFinished processing mix: {args.mix}")
