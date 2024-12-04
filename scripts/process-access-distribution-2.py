# Process Distribution file
# Author : Sukjoon Oh (sjoon@kaist.ac.kr)

import numpy as np
import pandas as pd

from collections import Counter


if __name__ == '__main__':
    import sys
    import os

    if len(sys.argv) != 2:
        print("Usage: %s <distribution-file>" % sys.argv[0])
        sys.exit(1)

    # Series: access-history-LRU-XX
    raw_access_file = sys.argv[1]

    # Prepare output file name
    export_file = raw_access_file + ".histogram.processed"

    if not os.path.exists(raw_access_file):
        print("File not found: {raw_access_file}")
        sys.exit(1)

    traces = []
    with open(raw_access_file, 'r') as f:
        lines = f.readlines()

        for line in lines:

            # Each line may hold multiple values
            # Split them in tab
            token = line.strip().split("\t")

            # Convert them into integer
            token = [int(x) for x in token]
            traces.append(token)

    # Flatten the traces
    flattened_traces = [item for sublist in traces for item in sublist]

    print(f"Total number of items: {len(flattened_traces)}")

    counter = Counter(flattened_traces)

    # Sort the counter by the occurence in descending order
    sorted_counter = counter.most_common()

    # Export the sorted counter
    with open(export_file, 'w') as f:
        for item, count in sorted_counter:
            f.write(f"{item}\t{count}\n")


    # Append the summary to the export file
    # with open("summary-access-distribution.csv", 'a') as f:
    #     f.write(f"{raw_access_file}\t{summary}\n")
    
