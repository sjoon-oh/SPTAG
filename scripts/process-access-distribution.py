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

    distribution_file = sys.argv[1]

    # Prepare output file name
    export_file = distribution_file + ".processed"

    if not os.path.exists(distribution_file):
        print("File not found: {distribution_file}")
        sys.exit(1)

    traces = []
    with open(distribution_file, 'r') as f:
        lines = f.readlines()

        for line in lines:

            # Each line may hold multiple values
            # Split them in tab
            token = line.strip().split("\t")

            # Convert them into integer
            token = [int(x) for x in token]
            traces.append(token)

    # Convert them into numpy array
    traces = np.array(traces)

    # Now, we have traces
    # 1. Generate CDF, by second column
    cdf = generate_cdf(traces[:, 1])
    np.savetxt(export_file, cdf, fmt='%.3f', delimiter='\t')

    # 2. Generate summary
    summary = generate_cdf_summary(traces[:, 1]) 
    # Append the summary to the export file
    with open("summary-access-distribution.csv", 'a') as f:
        f.write(f"{distribution_file}\t{summary}\n")
    
