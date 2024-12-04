# Process Distribution file
# Author : Sukjoon Oh (sjoon@kaist.ac.kr)

import numpy as np
import pandas as pd

from collections import Counter

def generate_cdf(trace):

    #
    # trace is 1D numpy array
    x = np.sort(trace)
    y = np.arange(1, len(x) + 1) / len(x)

    df = pd.DataFrame({'x': x, 'y': y})
    df = df.groupby('x', as_index=False).max()
    df = df.sort_values(by='x')

    # Use Counter to count occurrences
    count = Counter(x)
    count_of_1 = count[1]

    print(f"Count of 1: {count_of_1}, ({count_of_1 / len(x) * 100:.3f}%)")

    return df


def generate_disk_access_summary(trace):

    expected_total_disk_io = np.sum(trace[0]) + np.sum(trace[1])
    actual_total_disk_io = np.sum(trace[0])

    return "%d\t%d\t%lf" % (actual_total_disk_io, expected_total_disk_io, actual_total_disk_io / expected_total_disk_io)    


def generate_cdf_summary(trace):

    sorted_trace = np.sort(trace)

    # average, 1%ile, 50%ile, 99%ile, 99.9%ile
    mean = np.mean(sorted_trace)
    percentiles = np.percentile(sorted_trace, [1, 50, 99, 99.9])

    # Leave only 3 decimal points
    summary_str = "{:.3f}\t{:.3f}\t{:.3f}\t{:.3f}\t{:.3f}".format(
        mean, percentiles[0], percentiles[1], percentiles[2], percentiles[3])


    print(f"Summary: {summary_str}")

    return summary_str



if __name__ == '__main__':
    import sys
    import os

    if len(sys.argv) != 2:
        print("Usage: %s <distribution-file>" % sys.argv[0])
        sys.exit(1)

    perq_hit_ratio_file = sys.argv[1]

    # Prepare output file name
    export_file = perq_hit_ratio_file + ".processed"

    if not os.path.exists(perq_hit_ratio_file):
        print("File not found: {perq_hit_ratio_file}")
        sys.exit(1)

    data = pd.read_csv(perq_hit_ratio_file, delimiter='\t', header=None) 

    # First column: Hit number
    # Second column: Miss number
    # Third column: Hit ratio

    # Now, we have traces
    # 1. Generate CDF, by third column
    cdf = generate_cdf(data[2])
    cdf.to_csv(export_file, index=False, header=False, sep='\t')
    # np.savetxt(export_file, cdf, fmt='%.6f', delimiter='\t')

    # 2. Get summarized statistics
    summary = generate_cdf_summary(data[2])
    # Append the summary to the export file
    with open("summary-perq-hit-ratio.csv", 'a') as f:
        f.write(f"{perq_hit_ratio_file}\t{summary}\n")

    # 3. Get disk access summary
    disk_access_summary = generate_disk_access_summary(data)
    # Append the disk access summary to the export file
    with open("disk-access-summary-perq-hit-ratio.csv", 'a') as f:
        f.write(f"{perq_hit_ratio_file}\t{disk_access_summary}\n")



