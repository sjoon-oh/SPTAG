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

cd ${LOG_DIR}/reports


# List of files starts with given prefix
# DISTRIBUTION_FILES=($(find . -type f -name "access-distribution*.csv"))
# IFS=$'\n' DISTRIBUTION_FILES=($(sort <<<"${DISTRIBUTION_FILES[*]}"))

# LOCATION_FILES=($(find . -type f -name "access-location*.csv"))
# IFS=$'\n' LOCATION_FILES=($(sort <<<"${LOCATION_FILES[*]}"))

# HISTORY_FILES=($(find . -type f -name "access-history*.csv"))
# IFS=$'\n' HISTORY_FILES=($(sort <<<"${HISTORY_FILES[*]}"))

# PERQ_HIT_RATIO_FILES=($(find . -type f -name "access-perq-hit-ratio*.csv"))
# IFS=$'\n' PERQ_HIT_RATIO_FILES=($(sort <<<"${PERQ_HIT_RATIO_FILES[*]}"))

# GET_STATS_FILES=($(find . -type f -name "cache-get-elapsed*.csv"))
# IFS=$'\n' GET_STATS_FILES=($(sort <<<"${GET_STATS_FILES[*]}"))

# GET_ELAPSED_FILES=($(find . -type f -name "cache-get-stats*.csv"))
# IFS=$'\n' GET_ELAPSED_FILES=($(sort <<<"${GET_ELAPSED_FILES[*]}"))

# DELAY_STATS_FILES=($(find . -type f -name "cache-delay-elapsed*.csv"))
# IFS=$'\n' DELAY_STATS_FILES=($(sort <<<"${DELAY_STATS_FILES[*]}"))

# DELAY_ELAPSED_FILES=($(find . -type f -name "cache-delay-elapsed*.csv"))
# IFS=$'\n' DELAY_ELAPSED_FILES=($(sort <<<"${DELAY_ELAPSED_FILES[*]}"))

# List of files starts with given prefix
PERQ_HIT_RATIO_CDF_FILES=($(find . -type f -name "access-perq-hit-ratio*.processed"))
IFS=$'\n' PERQ_HIT_RATIO_CDF_FILES=($(sort <<<"${PERQ_HIT_RATIO_CDF_FILES[*]}"))

# 
# ----------------------------------------

CACHE_SIZE=(
    "1gb" "2gb" "4gb" "8gb" "16gb" "32gb"
    # "16gb" "1gb" "2gb" "4gb" "32gb" "8gb"
)

SPANN_MAX_SEARCH_NUM=(
    "48" "96" "192" "384"
    # "768" "1536" "3072" "6144"
)


# GNUPLOT_SCRIPT=../../../scripts/cdf-template.gp
# for search_num in ${SPANN_MAX_SEARCH_NUM[@]}; do

#     PERQ_HIT_RATIO_CDF_FILES=($(find . -type f -name "access-perq-hit-ratio*${search_num}.csv.processed"))
#     IFS=$'\n' PERQ_HIT_RATIO_CDF_FILES=($(sort <<<"${PERQ_HIT_RATIO_CDF_FILES[*]}"))

#     OUTPUT_FILE=access-perq-hit-ratio-cdf-${search_num}.png

#     # Prepare arguments
#     ARGUMENTS=""
#     for index in ${!CACHE_SIZE[@]}; do

#         # Append it to the arguments
#         ARGUMENTS+="arg_filename_${index}='${PERQ_HIT_RATIO_CDF_FILES[index]}'; "
#     done

#     ARGUMENTS+="arg_export_name='${OUTPUT_FILE}'"

#     # Check arguments
#     echo "Arguments: ${ARGUMENTS}"

#     gnuplot -e "${ARGUMENTS}" ${GNUPLOT_SCRIPT}

# done

GNUPLOT_SCRIPT=../../../scripts/histogram-template.gp
for search_num in ${SPANN_MAX_SEARCH_NUM[@]}; do

    ACCESS_HISTOGRAM_FILE=access-history-${search_num}.histogram.processed

    OUTPUT_FILE=access-history-${search_num}.png
    ARGUMENTS=""

    ARGUMENTS+="arg_filename_0='${ACCESS_HISTOGRAM_FILE}'; arg_export_name='${OUTPUT_FILE}'"

    gnuplot -e "${ARGUMENTS}" ${GNUPLOT_SCRIPT}
done





exit
