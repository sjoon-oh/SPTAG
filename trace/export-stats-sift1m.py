# Sukjoon Oh,
# Refinement

import argparse
import math
import numpy as np
import pandas as pd

import sys, os

from pathlib import Path

# 
# Get files
def get_csv_files_with_prefix(directory, prefix, ext):
    
    # List to store matching file names
    matching_files = []

    # Loop through all files in the specified directory
    for filename in os.listdir(directory):
        
        # Check if the file starts with the specified prefix and ends with .csv
        if filename.startswith(prefix) and filename.endswith(ext):
            matching_files.append(filename)
            
    return matching_files


def analysis_cache_trace_csv(filename):

    data = pd.read_csv(filename, delimiter='\t', header=None)   # Load

    hit_counts = data[0]
    miss_counts = data[1]
    evict_counts = data[2]
    curr_size = data[3]
    per_query_hit_ratio = data[4]

    # Must extract over 10000

    hit_counts = np.sort(hit_counts[10000:])
    miss_counts = np.sort(miss_counts[10000:])
    evict_counts = np.sort(evict_counts[10000:])
    curr_size = np.sort(curr_size[10000:])
    per_query_hit_ratio = np.sort(per_query_hit_ratio[10000:])

    analysis_targets = [
        {
            "extract-name": "per-query-hit-ratio", 
            "data": per_query_hit_ratio, 
            "summary-name": "",
            "summary-str": ""
        }
    ]

    for target in analysis_targets:

        values = target["data"]

        x_val = values
        y_val = np.arange(1, len(values) + 1) / len(values)

        cdf_df = pd.DataFrame({'val': x_val, 'cdf': y_val})
        cdf_df = cdf_df.groupby('val', as_index=False).max()
        cdf_df = cdf_df.sort_values(by='val')

        raw_filename = Path(filename).stem
        csv_path = f"extract-{target['extract-name']}-{raw_filename}.csv"
        cdf_df.to_csv(csv_path, index=False, header=False, sep='\t')

        print(f"Exported: {csv_path}")

        # if (len(values) > 0):
        lat_avg = np.mean(values)
        lat_50 = values[int(len(values) * 0.5)]
        lat_90 = values[int(len(values) * 0.90)]
        lat_95 = values[int(len(values) * 0.95)]
        lat_99 = values[int(len(values) * 0.99)]
        lat_999 = values[int(len(values) * 0.999)]

        target["summary-name"] = f"{raw_filename}"
        target["summary-str"] = f"{lat_avg}\t{lat_50}\t{lat_90}\t{lat_95}\t{lat_99}\t{lat_999}"

        del target["data"]
    
    return analysis_targets


def analysis_cache_lock_latency_csv(filename):

    data = pd.read_csv(filename, delimiter='\t', header=None)   # Load

    wait_latency = data[0]
    hold_latency = data[1]

    wait_latency = np.sort(wait_latency)
    hold_latency = np.sort(hold_latency)

    analysis_targets = [
        {
            "extract-name": "wait-lock-latency", 
            "data": wait_latency, 
            "summary-name": "",
            "summary-str": ""
        },
        {
            "extract-name": "hold-lock-latency", 
            "data": hold_latency, 
            "summary-name": "",
            "summary-str": ""
        }

    ]

    for target in analysis_targets:

        values = target["data"]

        x_val = values
        y_val = np.arange(1, len(values) + 1) / len(values)

        cdf_df = pd.DataFrame({'val': x_val, 'cdf': y_val})
        cdf_df = cdf_df.groupby('val', as_index=False).max()
        cdf_df = cdf_df.sort_values(by='val')

        raw_filename = Path(filename).stem
        csv_path = f"extract-{target['extract-name']}-{raw_filename}.csv"
        cdf_df.to_csv(csv_path, index=False, header=False, sep='\t')

        print(f"Exported: {csv_path}")

        lat_avg = np.mean(values)
        lat_50 = values[int(len(values) * 0.5)]
        lat_90 = values[int(len(values) * 0.90)]
        lat_95 = values[int(len(values) * 0.95)]
        lat_99 = values[int(len(values) * 0.99)]
        lat_999 = values[int(len(values) * 0.999)]

        target["summary-name"] = f"{raw_filename}-{target['extract-name']}"
        target["summary-str"] = f"{lat_avg}\t{lat_50}\t{lat_90}\t{lat_95}\t{lat_99}\t{lat_999}"

        del target["data"]
    
    return analysis_targets


def analysis_search_raw_csv(filename):

    data = pd.read_csv(filename, delimiter='\t', header=None)   # Load

    disk_latency = data[1]

    disk_latency = np.sort(disk_latency)

    analysis_targets = [
        {
            "extract-name": "disk-search", 
            "data": disk_latency, 
            "summary-name": "",
            "summary-str": ""
        }
    ]

    for target in analysis_targets:

        values = target["data"]

        x_val = values
        y_val = np.arange(1, len(values) + 1) / len(values)

        cdf_df = pd.DataFrame({'val': x_val, 'cdf': y_val})
        cdf_df = cdf_df.groupby('val', as_index=False).max()
        cdf_df = cdf_df.sort_values(by='val')

        raw_filename = Path(filename).stem
        csv_path = f"extract-{target['extract-name']}-{raw_filename}.csv"
        cdf_df.to_csv(csv_path, index=False, header=False, sep='\t')

        print(f"Exported: {csv_path}")

        lat_avg = np.mean(values)
        lat_50 = values[int(len(values) * 0.5)]
        lat_90 = values[int(len(values) * 0.90)]
        lat_95 = values[int(len(values) * 0.95)]
        lat_99 = values[int(len(values) * 0.99)]
        lat_999 = values[int(len(values) * 0.999)]

        target["summary-name"] = f"{raw_filename}"
        target["summary-str"] = f"{lat_avg}\t{lat_50}\t{lat_90}\t{lat_95}\t{lat_99}\t{lat_999}"

        del target["data"]
    
    return analysis_targets



# 
# 
if __name__ == "__main__":
    
    fprefix_cache_trace = "cache-trace"
    fprefix_cache_search_lock = "cache-lock-latency-search"
    fprefix_cache_refresh_lock = "cache-lock-latency-refresh"
    fprefix_search_trace_raw = "search-trace-raw"

    file_pref = {}

    file_pref[fprefix_cache_trace] = {
        "file-list": get_csv_files_with_prefix(".", fprefix_cache_trace, ".csv"),
        "out-prefix": fprefix_cache_trace,
        "handler": analysis_cache_trace_csv
    }

    file_pref[fprefix_cache_search_lock] = {
        "file-list": get_csv_files_with_prefix(".", fprefix_cache_search_lock, ".csv"),
        "out-prefix": fprefix_cache_search_lock,
        "handler": analysis_cache_lock_latency_csv
    }

    file_pref[fprefix_cache_refresh_lock] = {
        "file-list": get_csv_files_with_prefix(".", fprefix_cache_refresh_lock, ".csv"),
        "out-prefix": fprefix_cache_refresh_lock,
        "handler": analysis_cache_lock_latency_csv
    }

    file_pref[fprefix_search_trace_raw] = {
        "file-list": get_csv_files_with_prefix(".", fprefix_search_trace_raw, ".csv"),
        "out-prefix": fprefix_search_trace_raw,
        "handler": analysis_search_raw_csv
    }
    
    for fpref in file_pref.values():
        
        summary = []
        for filename in fpref["file-list"]:
            try:
                summary.append(fpref["handler"](filename))
            except Exception as e:
                # This will catch any other exceptions
                print(f"File: {filename}")
                print(f"An error occurred: {e}")

        # Extract summary
        out_fname = f"{fpref['out-prefix']}-summary.txt"
        with open(f"{out_fname}", "w") as f:
            for fileitem in summary:
                for single_summary in fileitem:
                    f.write(f"{single_summary['summary-name']}\t{single_summary['summary-str']}\n")



