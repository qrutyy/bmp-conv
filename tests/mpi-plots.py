import argparse
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import pandas as pd
import os
import sys
from pathlib import Path
import warnings

# --- Configuration ---
SCRIPT_DIR = Path(__file__).resolve().parent
BASE_DIR = SCRIPT_DIR

# --- !!! Убедитесь, что это ИМЯ ФАЙЛА, который вы анализируете !!! ---
LOG_FILE = BASE_DIR / "timing-results.dat"
# --- End Configuration ---

PLOTS_PATH = BASE_DIR / "plots" / "mpi"

# --- Matplotlib settings (keep as before) ---
plt.rcParams.update(
    {
        "axes.titlesize": 18, "axes.labelsize": 16, "xtick.labelsize": 11,
        "ytick.labelsize": 12, "legend.fontsize": 10, "figure.figsize": (18, 10),
        "figure.dpi": 150,
    }
)
try:
    colors = matplotlib.colormaps["tab20"].colors
except AttributeError:
     print("Warning: Using deprecated plt.cm.get_cmap. Please update Matplotlib.")
     colors = plt.cm.get_cmap("tab20").colors
# --- End Matplotlib settings ---


# --- Data Reading and Cleaning ---
print(f"Attempting to read potentially inconsistent MPI data from: {LOG_FILE}")

if not LOG_FILE.is_file():
    print(f"Error: Log file not found at '{LOG_FILE}'")
    sys.exit(1)

# Define final column names we want
final_col_names = ["RunID", "Filter", "Process-num", "Mode", "Block-size", "Result"]
# Define temporary names for reading max 6 columns
read_col_names = ['col0', 'col1', 'col2', 'col3', 'col4', 'col5']

try:
    # Read the file, *ignoring* the actual header line, expect up to 6 fields
    # Use 'warn' for bad lines initially to see if there's other garbage
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        df_raw = pd.read_csv(
            LOG_FILE,
            sep=r"\s+",
            header=None,           # Treat first line as data for now
            names=read_col_names,  # Expect max 6 columns
            skiprows=1,            # Skip the actual header row in the file
            skipinitialspace=True,
            comment="#",
            skip_blank_lines=True,
            engine='python',
            on_bad_lines='warn'    # Warn about lines with wrong number of fields
        )
        if w:
            print("\n--- Warnings during initial read ---")
            for warning in w:
                print(warning.message)
            print("----------------------------------\n")


    if df_raw.empty:
        print("Error: DataFrame is empty after loading. Check log file content.")
        sys.exit(1)

    print(f"Read {len(df_raw)} data lines.")
    # print("Initial DataFrame head (up to 6 cols):")
    # print(df_raw.head())

    # --- Reconstruct the DataFrame ---
    print("Reconstructing DataFrame based on field count...")

    # Identify rows that originally had 6 fields (col5 is not NaN)
    has_run_id = df_raw['col5'].notna()

    # Create the correctly structured DataFrame
    df_structured = pd.DataFrame(columns=final_col_names)

    # Populate rows that had RunID (6 fields)
    df_structured['RunID']        = df_raw.loc[has_run_id, 'col0']
    df_structured['Filter']       = df_raw.loc[has_run_id, 'col1']
    df_structured['Process-num']  = df_raw.loc[has_run_id, 'col2']
    df_structured['Mode']         = df_raw.loc[has_run_id, 'col3']
    df_structured['Block-size']   = df_raw.loc[has_run_id, 'col4']
    df_structured['Result']       = df_raw.loc[has_run_id, 'col5']

    # Populate rows that were missing RunID (5 fields) - shift columns
    # RunID is initially NaN for these rows
    df_structured.loc[~has_run_id, 'Filter']       = df_raw.loc[~has_run_id, 'col0']
    df_structured.loc[~has_run_id, 'Process-num']  = df_raw.loc[~has_run_id, 'col1']
    df_structured.loc[~has_run_id, 'Mode']         = df_raw.loc[~has_run_id, 'col2']
    df_structured.loc[~has_run_id, 'Block-size']   = df_raw.loc[~has_run_id, 'col3']
    df_structured.loc[~has_run_id, 'Result']       = df_raw.loc[~has_run_id, 'col4']

    # Sort by original index to help with forward fill
    df_structured.sort_index(inplace=True)

    # print("DataFrame after structuring (before ffill):")
    # print(df_structured.head(10)) # Show more rows

    # Forward fill the missing RunID values
    df_structured['RunID'].ffill(inplace=True)

    # print("DataFrame after ffill:")
    # print(df_structured.head(10))

    # Drop rows where RunID might still be NaN (e.g., if the file started with a 5-field line)
    initial_len = len(df_structured)
    df_structured.dropna(subset=['RunID'], inplace=True)
    if len(df_structured) < initial_len:
        print(f"Dropped {initial_len - len(df_structured)} rows with missing RunID after ffill.")

    # --- Convert types ---
    print("Converting columns to appropriate types...")
    try:
        # Convert RunID, Process-num, Block-size first as they should be integers
        df_structured['RunID'] = pd.to_numeric(df_structured['RunID']).astype(int)
        df_structured['Process-num'] = pd.to_numeric(df_structured['Process-num']).astype(int)
        df_structured['Block-size'] = pd.to_numeric(df_structured['Block-size']).astype(int)
        # Convert Result to float
        df_structured['Result'] = pd.to_numeric(df_structured['Result'])
        # Filter and Mode remain strings/objects
        print("Type conversion successful.")
    except Exception as e:
        print(f"Error during numeric conversion after restructuring: {e}")
        print("Check the data structure and content after ffill.")
        # print(df_structured.info())
        # print(df_structured.head())
        sys.exit(1)

    # --- Drop duplicates ---
    # Assume the 5-field lines are duplicates of the preceding 6-field line (after ffill)
    # Keep the 'first' occurrence, which corresponds to the original 6-field line
    print("Dropping likely duplicate rows (those originally missing RunID)...")
    cols_to_check_duplicates = ['RunID', 'Filter', 'Process-num', 'Mode', 'Block-size', 'Result']
    initial_len = len(df_structured)
    df_clean = df_structured.drop_duplicates(subset=cols_to_check_duplicates, keep='first')
    print(f"Dropped {initial_len - len(df_clean)} duplicate rows.")

    df_final = df_clean.copy() # Use df_final going forward


except Exception as e:
    print(f"\n--- An error occurred during file processing ---")
    print(f"Error type: {type(e)}")
    print(f"Error message: {e}")
    sys.exit(1)

print("\nCleaned and Final DataFrame head:")
print(df_final.head())
print(f"\nTotal final rows: {len(df_final)}")
if df_final.empty:
    print("Error: DataFrame is empty after cleaning. No data to plot.")
    sys.exit(1)
print("Unique Process-num found:", sorted(df_final["Process-num"].unique()))


# --- Data Aggregation (Now uses the cleaned df_final) ---
print("\nAggregating results...")
try:
    df_agg = df_final.groupby(['Process-num', 'Mode', 'Block-size'])['Result'].agg(['mean', 'std']).reset_index()
    df_agg['std'] = df_agg['std'].fillna(0)
    df_agg.rename(columns={'mean': 'avg_time', 'std': 'std_dev'}, inplace=True)
except KeyError as e:
     print(f"Error: Column {e} not found during aggregation.")
     print("Available columns after cleaning:", df_final.columns)
     sys.exit(1)
except Exception as e:
    print(f"An unexpected error occurred during data aggregation: {e}")
    sys.exit(1)

print("\nAggregated MPI DataFrame head:")
print(df_agg.head())
print(f"\nTotal aggregated rows: {len(df_agg)}")
if df_agg.empty:
    print("Error: Aggregated DataFrame is empty. No data to plot.")
    sys.exit(1)
print("Unique Process-num found in aggregated data:", sorted(df_agg["Process-num"].unique()))
print("Unique Modes found:", df_agg["Mode"].unique())
print("Unique Block-sizes found:", sorted(df_agg["Block-size"].unique()))
print("-" * 30)


# --- Plotting Functions (No changes needed, use df_agg) ---
# (Функции plot_grouped_by_block_size и plot_grouped_by_proc_num остаются такими же,
#  так как они работают с уже агрегированным и чистым df_agg)
def plot_grouped_by_block_size(agg_data, base_plots_path):
    # ... (previous code) ...
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
        plot_df = agg_data[agg_data["Block-size"] == bs].copy()
        if plot_df.empty: continue
        print(f"  Plotting for Block Size: {bs}")

        x = np.arange(len(unique_modes))
        num_bars = len(unique_proc_nums)
        total_width = 0.8
        width = total_width / num_bars

        fig, ax = plt.subplots(figsize=(max(12, 2 * len(unique_modes) * num_bars), 9))

        for i, proc_num in enumerate(unique_proc_nums):
            means = []
            stds = []
            for mode in unique_modes:
                row = plot_df[(plot_df["Mode"] == mode) & (plot_df["Process-num"] == proc_num)]
                means.append(row["avg_time"].iloc[0] if not row.empty else 0)
                stds.append(row["std_dev"].iloc[0] if not row.empty else 0)

            position = x - (total_width / 2) + (i + 0.5) * width
            ax.bar(
                position, means, width * 0.95, yerr=stds,
                label=f"{proc_num} procs", color=colors[i % len(colors)],
                capsize=4, alpha=0.85
            )

        ax.set_xlabel("Computation Mode")
        ax.set_ylabel("Average Execution Time (seconds)")
        ax.set_title(f"MPI Performance Comparison (Block Size: {bs})")
        ax.set_xticks(x)
        ax.set_xticklabels(unique_modes)
        ax.legend(title="Process Number")
        ax.grid(axis='y', linestyle='--', alpha=0.7)
        plt.tight_layout()

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
    # ... (previous code) ...
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
        plot_df = agg_data[agg_data["Process-num"] == pn].copy()
        if plot_df.empty: continue
        print(f"  Plotting for Process Number: {pn}")

        x = np.arange(len(unique_modes))
        num_bars = len(unique_block_sizes)
        total_width = 0.8
        width = total_width / num_bars

        fig, ax = plt.subplots(figsize=(max(12, 2 * len(unique_modes) * num_bars), 9))

        for i, bs in enumerate(unique_block_sizes):
            means = []
            stds = []
            for mode in unique_modes:
                row = plot_df[(plot_df["Mode"] == mode) & (plot_df["Block-size"] == bs)]
                means.append(row["avg_time"].iloc[0] if not row.empty else 0)
                stds.append(row["std_dev"].iloc[0] if not row.empty else 0)

            position = x - (total_width / 2) + (i + 0.5) * width
            ax.bar(
                position, means, width * 0.95, yerr=stds,
                label=f"Block {bs}", color=colors[i % len(colors)],
                capsize=4, alpha=0.85
            )

        ax.set_xlabel("Computation Mode")
        ax.set_ylabel("Average Execution Time (seconds)")
        ax.set_title(f"MPI Performance Comparison (Processes: {pn})")
        ax.set_xticks(x)
        ax.set_xticklabels(unique_modes)
        ax.legend(title="Block Size")
        ax.grid(axis='y', linestyle='--', alpha=0.7)
        plt.tight_layout()

        filename = f"mpi_perf_proc_{pn}.png"
        save_path = plot_dir / filename
        try:
            plt.savefig(save_path)
            print(f"    Saved plot: {save_path}")
        except Exception as e:
            print(f"    Error saving plot '{save_path}': {e}")
        finally:
            plt.close(fig)
# --- End Plotting Functions ---


# --- Main Execution ---
os.makedirs(PLOTS_PATH, exist_ok=True)
# Plot using the aggregated data
if not df_agg.empty:
    plot_grouped_by_block_size(df_agg, PLOTS_PATH)
    plot_grouped_by_proc_num(df_agg, PLOTS_PATH)
    print("\nMPI plotting script finished.")
else:
    print("\nSkipping plotting as aggregated data is empty.")

