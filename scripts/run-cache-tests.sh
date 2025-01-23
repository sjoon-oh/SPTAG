#!/bin/bash

RUN_START_TIME=$(date +'%Y-%m-%d_%H-%M-%S')

# Remove old log files
mkdir -p log

# List of cache policies

# CONFIG_PATH="configs/sift1m.ini"
# CONFIG_PATH="configs/sift1m.ini"
CONFIG_PATH="configs/spacev1b-extended.ini"

# Updated configuration path
TEMPLATE_CONFIG_PATH="configs/spacev1b-extended-template.ini"
# TEMPLATE_CONFIG_PATH="configs/spacev1b.ini"

TRACE_PATH=log/trace-${RUN_START_TIME}

mkdir -p ${TRACE_PATH}

CACHE_POLICIES=("LRU" "FIFO")
CACHE_SIZE=(
    "3.74gb:3745994342"
    "7.49gb:7491988684"
    "11.24gb:11237983028"
    "14.98gb:14983977372"
    "18.73gb:18729971712"
    "22.48gb:22475966054"
    "26.22gb:26221960396"
    "29.97gb:29967954740"
    "33.71gb:33713949082"
    "37.46gb:37459943424"
)

SPANN_MAX_SEARCH_NUM=(
    # "32"
    "96"
    # "48" "96" "192" "384"
    # "768" "1536" "3072" "6144"
)

# First, run the original test
# for search_num in "${SPANN_MAX_SEARCH_NUM[@]}"; do

#     cp ${CONFIG_PATH} ${TEMPLATE_CONFIG_PATH}
#     sed -i "/\[SearchSSDIndex\]/,/^$/ s/^\(InternalResultNum=\).*/\1${search_num}/" ${TEMPLATE_CONFIG_PATH}

#     printf "Running unmodified SPANN for ${search_num} searches\n"

#     OUT_FILE=${TRACE_PATH}/spacev1b-extended-original-${search_num}.out

#     ./ssdserving-precompiled ${TEMPLATE_CONFIG_PATH} > ${OUT_FILE}

#     # Extract summarized data
#     HEAD_LATENCY=$(grep -A 2 "Head Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')
#     EX_LATENCY=$(grep -A 2 "Ex Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')
#     TOTAL_LATENCY=$(grep -A 2 "Total Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')

#     echo -e "${OUT_FILE}\t${HEAD_LATENCY}" >> summary-original-head.csv
#     echo -e "${OUT_FILE}\t${EX_LATENCY}" >> summary-original-ex.csv
#     echo -e "${OUT_FILE}\t${TOTAL_LATENCY}" >> summary-original-total.csv

#     rm ${TEMPLATE_CONFIG_PATH}
# done

# exit

for cache_policy in "${CACHE_POLICIES[@]}"; do

    SUMMARY_FILE_PREFIX=${TRACE_PATH}/summary-${RUN_START_TIME}

    for search_num in "${SPANN_MAX_SEARCH_NUM[@]}"; do
        for cache_size in "${CACHE_SIZE[@]}"; do

            IFS=':' read -r SIZE_HUMAN_READABLE SIZE_IN_BYTES <<< "$cache_size"
            printf "Running ${cache_policy} cache tests for cache size: ${SIZE_HUMAN_READABLE} (${SIZE_IN_BYTES} Bytes) for ${search_num} searches\n" 

            cp ${CONFIG_PATH} ${TEMPLATE_CONFIG_PATH}
            sed -i "/\[SearchSSDIndex\]/,/^$/ s/^\(InternalResultNum=\).*/\1${search_num}/" ${TEMPLATE_CONFIG_PATH}

            OUT_FILE=${TRACE_PATH}/spacev1b-extended-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.out

            # Run the script
            ./Release/ssdserving ${TEMPLATE_CONFIG_PATH} \
                --cache-policy "${cache_policy}" \
                --cache-size "${SIZE_IN_BYTES}" \
                > ${OUT_FILE}
            
            # Move the log files
            mv access-distribution.csv ${TRACE_PATH}/access-distribution-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            mv access-location.csv ${TRACE_PATH}/access-location-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            mv access-history.csv ${TRACE_PATH}/access-history-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            mv access-perq-hit-ratio.csv ${TRACE_PATH}/access-perq-hit-ratio-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            
            mv cache-get-stats.csv ${TRACE_PATH}/cache-get-stats-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            mv cache-delay-stats.csv ${TRACE_PATH}/cache-delay-stats-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv

            mv cache-get-elapsed.csv ${TRACE_PATH}/cache-get-elapsed-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv
            mv cache-delay-elapsed.csv ${TRACE_PATH}/cache-delay-elapsed-${cache_policy}-${SIZE_HUMAN_READABLE}-${search_num}.csv

            # Extract summarized data
            HEAD_LATENCY=$(grep -A 2 "Head Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')
            EX_LATENCY=$(grep -A 2 "Ex Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')
            TOTAL_LATENCY=$(grep -A 2 "Total Latency Distribution:" ${OUT_FILE} | tail -n 1 | sed 's/^\[1\] //;s/  */\t/g')

            echo -e "${OUT_FILE}\t${HEAD_LATENCY}" >> ${SUMMARY_FILE_PREFIX}-head.csv
            echo -e "${OUT_FILE}\t${EX_LATENCY}" >> ${SUMMARY_FILE_PREFIX}-ex.csv
            echo -e "${OUT_FILE}\t${TOTAL_LATENCY}" >> ${SUMMARY_FILE_PREFIX}-total.csv

            rm ${TEMPLATE_CONFIG_PATH}

        done

    done
done




