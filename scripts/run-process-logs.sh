#!/bin/bash

# This script processes the logs in the logs directory and generates a report
# 
# First parm is the directory where the logs are stored
LOG_DIR=$1

# Check if the log directory exists
if [ ! -d "$LOG_DIR" ]; then
    echo "Directory $LOG_DIR does not exist"
    exit 1
fi

echo "Processing logs in $LOG_DIR"

cd ${LOG_DIR}

rm report/*


# List of files starts with given prefix
DISTRIBUTION_FILES=($(find . -type f -name "access-distribution*.csv"))
IFS=$'\n' DISTRIBUTION_FILES=($(sort <<<"${DISTRIBUTION_FILES[*]}"))

LOCATION_FILES=($(find . -type f -name "access-location*.csv"))
IFS=$'\n' LOCATION_FILES=($(sort <<<"${LOCATION_FILES[*]}"))

HISTORY_FILES=($(find . -type f -name "access-history*.csv"))
IFS=$'\n' HISTORY_FILES=($(sort <<<"${HISTORY_FILES[*]}"))

PERQ_HIT_RATIO_FILES=($(find . -type f -name "access-perq-hit-ratio*.csv"))
IFS=$'\n' PERQ_HIT_RATIO_FILES=($(sort <<<"${PERQ_HIT_RATIO_FILES[*]}"))

GET_STATS_FILES=($(find . -type f -name "cache-get-elapsed*.csv"))
IFS=$'\n' GET_STATS_FILES=($(sort <<<"${GET_STATS_FILES[*]}"))

GET_ELAPSED_FILES=($(find . -type f -name "cache-get-stats*.csv"))
IFS=$'\n' GET_ELAPSED_FILES=($(sort <<<"${GET_ELAPSED_FILES[*]}"))

DELAY_STATS_FILES=($(find . -type f -name "cache-delay-elapsed*.csv"))
IFS=$'\n' DELAY_STATS_FILES=($(sort <<<"${DELAY_STATS_FILES[*]}"))

DELAY_ELAPSED_FILES=($(find . -type f -name "cache-delay-elapsed*.csv"))
IFS=$'\n' DELAY_ELAPSED_FILES=($(sort <<<"${DELAY_ELAPSED_FILES[*]}"))

# 
# ----------------------------------------
mkdir -p reports

# PROCESS_SCRIPT=scripts/process-access-distribution.py

# for file in ${DISTRIBUTION_FILES[@]}; do

#     printf "Processing access distribution ${file}...\n"
#     python3.12 ../../${PROCESS_SCRIPT} ${file}

#     printf "\n"
# done

# PROCESS_SCRIPT=scripts/process-perq-hit-ratio.py

# for file in ${PERQ_HIT_RATIO_FILES[@]}; do

#     printf "Processing perq hit ratio ${file}...\n"
#     python3.12 ../../${PROCESS_SCRIPT} ${file}

#     printf "\n"
# done

PROCESS_SCRIPT=scripts/process-access-distribution-2.py

for file in ${HISTORY_FILES[@]}; do

    printf "Processing perq hit ratio ${file}...\n"
    python3.12 ../../${PROCESS_SCRIPT} ${file}

    printf "\n"
done

mv ./*.processed reports/
