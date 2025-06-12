import argparse
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import pandas as pd
import os
import sys
from pathlib import Path
import warnings
import traceback

try:
    SCRIPT_DIR = Path(__file__).resolve().parent
except NameError:
    SCRIPT_DIR = Path(".").resolve()
    print(
        f"Warning: '__file__' not defined. Assuming script directory is: {SCRIPT_DIR}"
    )

BASE_DIR = SCRIPT_DIR
LOG_FILE = BASE_DIR / "timing-results.dat"
PLOTS_PATH = BASE_DIR / "plots" / "mpi" / "1gb-image"

plt.rcParams.update(
    {
        "axes.titlesize": 18,
        "axes.labelsize": 16,
        "xtick.labelsize": 11,
        "ytick.labelsize": 12,
        "legend.fontsize": 10,
        "figure.figsize": (18, 10),  # Adjusted for potentially many bars
        "figure.dpi": 150,
    }
)
try:
    colors = matplotlib.colormaps["tab20"].colors
except AttributeError:
    print("Warning: Using deprecated plt.cm.get_cmap. Consider updating Matplotlib.")
    # Fallback for older Matplotlib versions
    colors = plt.cm.get_cmap("tab20").colors


print(f"Running Python plotting script")
print(f"Attempting to read MPI data from: {LOG_FILE}")

if not LOG_FILE.is_file():
    print(f"Error: Log file not found at '{LOG_FILE}'")
    sys.exit(1)

final_col_names = [
    "RunID",
    "Process-num",
    "Filter",
    "ThreadNum_logged",
    "Mode",
    "Block-size",
    "Result",
]

df_final = pd.DataFrame()  # Initialize df_final in outer scope

try:
    print("\n--- Reading and Parsing Log File ---")
    df_raw = pd.read_csv(
        LOG_FILE,
        sep=r"\s+",  # Use regex for whitespace separation
        header=None,  # No header row in the data itself
        names=final_col_names,  # Use the defined column names
        skiprows=1,  # Skip the actual header line in the file
        skipinitialspace=True,
        comment="#",  # Ignore lines starting with #
        skip_blank_lines=True,
        engine="python",  # Good for regex separator
        # on_bad_lines="warn" # Removed, assuming consistent columns now
    )

    if df_raw.empty:
        print(
            "Error: DataFrame is empty after loading. Check log file content and parsing parameters."
        )
        sys.exit(1)

    print(f"Read {len(df_raw)} raw data lines.")

    print("\n--- Cleaning Data ---")

    print("Converting columns to appropriate numeric types...")
    conversion_errors = False
    try:
        # Use errors='coerce' to turn non-numeric values into NaN temporarily
        df_raw["RunID"] = pd.to_numeric(df_raw["RunID"], errors="coerce")
        df_raw["Process-num"] = pd.to_numeric(df_raw["Process-num"], errors="coerce")
        # Filter and ThreadNum_logged are also numeric in the example
        df_raw["ThreadNum_logged"] = pd.to_numeric(
            df_raw["ThreadNum_logged"], errors="coerce"
        )
        df_raw["Block-size"] = pd.to_numeric(df_raw["Block-size"], errors="coerce")
        df_raw["Result"] = pd.to_numeric(df_raw["Result"], errors="coerce")

        # Check for NaNs introduced by coercion (indicates bad data)
        if df_raw.isnull().any().any():
            print("Warning: Non-numeric data found during conversion. Affected rows:")
            nan_cols = df_raw.columns[df_raw.isnull().any()].tolist()
            print(f"Columns with NaNs: {nan_cols}")
            print(df_raw[df_raw.isnull().any(axis=1)])
            nan_rows = len(df_raw[df_raw.isnull().any(axis=1)])
            df_raw.dropna(inplace=True)
            print(
                f"Dropped {nan_rows} rows containing non-numeric data in expected numeric columns."
            )
            conversion_errors = True  # Mark that we dropped data

        # Convert integer columns to int type *after* handling NaNs
        df_raw["RunID"] = df_raw["RunID"].astype(int)
        df_raw["Process-num"] = df_raw["Process-num"].astype(int)
        df_raw["ThreadNum_logged"] = df_raw["ThreadNum_logged"].astype(
            int
        )  # Convert new column
        df_raw["Block-size"] = df_raw["Block-size"].astype(int)

        print("Type conversion successful.")
        if conversion_errors:
            print("(Note: Some rows were dropped due to conversion errors.)")

    except Exception as e:
        print(f"\n--- Error during numeric conversion ---")
        print(f"Error type: {type(e)}")
        print(f"Error message: {e}")
        print("\nDataFrame state just before error:")
        print(df_raw.head())
        print(df_raw.info())
        print(
            "\nCheck the data file for unexpected non-numeric values in these columns."
        )
        traceback.print_exc()
        sys.exit(1)

    print("Skipping duplicate row drop (no longer needed after removing ffill).")

    df_final = df_raw.copy()  # Use df_final from here on

except Exception as e:
    print(f"\n--- An unexpected error occurred during file processing ---")
    print(f"Error type: {type(e)}")
    print(f"Error message: {e}")
    traceback.print_exc()
    print("\nCheck the data file format and the script's parsing logic.")
    sys.exit(1)

if df_final.empty:
    print(
        "\nError: DataFrame is empty after cleaning. No data to plot. Check intermediate steps."
    )
    sys.exit(1)

print("\n--- Cleaned and Final DataFrame ---")
print("Head:")
print(df_final.head())
print(f"\nTotal final rows: {len(df_final)}")
print("Unique Process-num found:", sorted(df_final["Process-num"].unique()))
print("Unique Modes found:", sorted(df_final["Mode"].unique()))
print("Unique Block-sizes found:", sorted(df_final["Block-size"].unique()))
print("Unique ThreadNum_logged found:", sorted(df_final["ThreadNum_logged"].unique()))
print("Unique Filters found:", df_final["Filter"].unique())


print("\n--- Aggregating Results ---")
df_agg = pd.DataFrame()  # Initialize df_agg
try:
    grouping_cols = ["Process-num", "Mode", "Block-size"]
    print(f"Grouping by: {grouping_cols}")

    df_agg = (
        df_final.groupby(grouping_cols)["Result"]
        .agg(["mean", "std"])
        .reset_index()  # Turn grouped indices back into columns
    )
    # Handle cases with only one run (std will be NaN)
    df_agg["std"] = df_agg["std"].fillna(0)
    # Rename columns for clarity
    df_agg.rename(columns={"mean": "avg_time", "std": "std_dev"}, inplace=True)

    print("Aggregation successful.")

except KeyError as e:
    print(f"\n--- Error during aggregation ---")
    print(
        f"Error: Column '{e}' not found. This usually means a parsing/cleaning step failed."
    )
    print("Available columns in df_final:", df_final.columns.tolist())
    sys.exit(1)
except Exception as e:
    print(f"\n--- An unexpected error occurred during data aggregation ---")
    print(f"Error type: {type(e)}")
    print(f"Error message: {e}")
    traceback.print_exc()
    sys.exit(1)

if df_agg.empty:
    print(
        "Error: Aggregated DataFrame is empty. No data to plot. Check grouping columns."
    )
    sys.exit(1)

print("\n--- Aggregated MPI DataFrame ---")
print("Head:")
print(df_agg.head())
print(f"\nTotal aggregated rows: {len(df_agg)}")
print("Unique Process-num in aggregated data:", sorted(df_agg["Process-num"].unique()))
print("Unique Modes found:", sorted(df_agg["Mode"].unique()))
print("Unique Block-sizes found:", sorted(df_agg["Block-size"].unique()))
print("-" * 30)


def plot_grouped_by_block_size(agg_data, base_plots_path):
    """Generates plots where each plot shows results for a single block size."""
    unique_block_sizes = sorted(agg_data["Block-size"].unique())
    unique_modes = sorted(agg_data["Mode"].unique())
    unique_proc_nums = sorted(agg_data["Process-num"].unique())

    if not unique_block_sizes or not unique_modes or not unique_proc_nums:
        print("Warning: Not enough unique dimension values to plot by block size.")
        return

    plot_dir = base_plots_path / "by_block_size"
    os.makedirs(plot_dir, exist_ok=True)
    print(f"\nGenerating plots grouped by Block Size into {plot_dir}")

    for bs in unique_block_sizes:
        plot_df = agg_data[
            agg_data["Block-size"] == bs
        ].copy()  # Filter data for this block size
        if plot_df.empty:
            print(f"  Skipping Block Size: {bs} (no data)")
            continue
        print(f"  Plotting for Block Size: {bs}")

        # Prepare for grouped bar chart
        x = np.arange(len(unique_modes))  # Positions for modes on x-axis
        num_bars = len(
            unique_proc_nums
        )  # Number of bars in each group (one per proc_num)
        total_width = 0.8  # Total width allocated for each group of bars
        width = total_width / num_bars  # Width of a single bar

        # Create plot
        fig, ax = plt.subplots(
            figsize=(max(12, 1.5 * len(unique_modes) * num_bars), 9)
        )  # Dynamic width

        # Iterate through each process number to create a bar within each mode group
        for i, proc_num in enumerate(unique_proc_nums):
            means = []
            stds = []
            for mode in unique_modes:
                row = plot_df[
                    (plot_df["Mode"] == mode) & (plot_df["Process-num"] == proc_num)
                ]
                # Use .iloc[0] safely after checking if row is empty
                means.append(row["avg_time"].iloc[0] if not row.empty else 0)
                stds.append(row["std_dev"].iloc[0] if not row.empty else 0)

            # Calculate position for this proc_num's bar within the group
            position = x - (total_width / 2) + (i + 0.5) * width

            ax.bar(
                position,
                means,
                width * 0.95,  # Slightly reduce width for spacing
                yerr=stds,
                label=f"{proc_num} procs",
                color=colors[i % len(colors)],  # Cycle through colors
                capsize=4,  # Error bar cap size
                alpha=0.85,  # Slight transparency
            )

        ax.set_xlabel("Computation Mode & Process Number")
        ax.set_ylabel("Average Execution Time (seconds)")
        ax.set_title(f"MPI Performance Comparison (Block Size: {bs})")
        ax.set_xticks(x)  # Set tick positions to center of groups
        ax.set_xticklabels(unique_modes)  # Set tick labels to mode names
        ax.legend(
            title="Process Number", bbox_to_anchor=(1.02, 1), loc="upper left"
        )  # Move legend outside
        ax.grid(axis="y", linestyle="--", alpha=0.7)
        plt.tight_layout(rect=[0, 0, 0.95, 1])  # Adjust layout to make space for legend

        filename = f"mpi_perf_block_{bs}.png"
        save_path = plot_dir / filename
        try:
            plt.savefig(save_path)
            print(f"    Saved plot: {save_path}")
        except Exception as e:
            print(f"    Error saving plot '{save_path}': {e}")
        finally:
            plt.close(fig)


def plot_grouped_by_proc_num(agg_data, base_plots_path):
    """Generates plots where each plot shows results for a single process number."""
    unique_block_sizes = sorted(agg_data["Block-size"].unique())
    unique_modes = sorted(agg_data["Mode"].unique())
    unique_proc_nums = sorted(agg_data["Process-num"].unique())

    if not unique_block_sizes or not unique_modes or not unique_proc_nums:
        print("Warning: Not enough unique dimension values to plot by process number.")
        return

    plot_dir = base_plots_path / "by_proc_num"
    os.makedirs(plot_dir, exist_ok=True)
    print(f"\nGenerating plots grouped by Process Number into {plot_dir}")

    for pn in unique_proc_nums:
        plot_df = agg_data[
            agg_data["Process-num"] == pn
        ].copy()  # Filter data for this process number
        if plot_df.empty:
            print(f"  Skipping Process Number: {pn} (no data)")
            continue
        print(f"  Plotting for Process Number: {pn}")

        # Prepare for grouped bar chart
        x = np.arange(len(unique_modes))  # Positions for modes on x-axis
        num_bars = len(
            unique_block_sizes
        )  # Number of bars in each group (one per block size)
        total_width = 0.8  # Total width allocated for each group of bars
        width = total_width / num_bars  # Width of a single bar

        fig, ax = plt.subplots(
            figsize=(max(12, 1.5 * len(unique_modes) * num_bars), 9)
        )  # Dynamic width

        # Iterate through each block size to create a bar within each mode group
        for i, bs in enumerate(unique_block_sizes):
            means = []
            stds = []
            for mode in unique_modes:
                row = plot_df[(plot_df["Mode"] == mode) & (plot_df["Block-size"] == bs)]
                means.append(row["avg_time"].iloc[0] if not row.empty else 0)
                stds.append(row["std_dev"].iloc[0] if not row.empty else 0)

            # Calculate position for this block size's bar within the group
            position = x - (total_width / 2) + (i + 0.5) * width

            ax.bar(
                position,
                means,
                width * 0.95,  # Slightly reduce width for spacing
                yerr=stds,
                label=f"Block {bs}",
                color=colors[i % len(colors)],  # Cycle through colors
                capsize=4,
                alpha=0.85,
            )

        ax.set_xlabel("Computation Mode & Process Number")
        ax.set_ylabel("Average Execution Time (seconds)")
        ax.set_title(f"MPI Performance Comparison (Processes: {pn})")
        ax.set_xticks(x)
        ax.set_xticklabels(unique_modes)
        ax.legend(
            title="Block Size", bbox_to_anchor=(1.02, 1), loc="upper left"
        )  # Move legend outside
        ax.grid(axis="y", linestyle="--", alpha=0.7)
        plt.tight_layout(rect=[0, 0, 0.95, 1])  # Adjust layout for legend

        filename = f"mpi_perf_proc_{pn}.png"
        save_path = plot_dir / filename
        try:
            plt.savefig(save_path)
            print(f"    Saved plot: {save_path}")
        except Exception as e:
            print(f"    Error saving plot '{save_path}': {e}")
        finally:
            plt.close(fig)


print("\n--- Generating Plots ---")
os.makedirs(PLOTS_PATH, exist_ok=True)

if not df_agg.empty:
    plot_grouped_by_block_size(df_agg, PLOTS_PATH)
    plot_grouped_by_proc_num(df_agg, PLOTS_PATH)
    print("\nMPI plotting script finished successfully.")
else:
    print("\nSkipping plotting as aggregated data is empty or aggregation failed.")
