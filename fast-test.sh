#!/bin/bash

# cd build && make -j
# cd ..

# gdb -ex=run --args ./Release/ssdserving custom-configs/sift1m-search-only.ini 1073741824
# exit

# mv ./Release/ssdserving ./Release/ssdserving-cache
rm trace/*.txt
rm trace/*.csv

exec_binary=(
    # "ssdserving-cache-corrlru-4-2"
    # "ssdserving-cache-corrlru-4-4"
    # "ssdserving-cache-corrlru-4-8"
    # "ssdserving-cache-corrlru-8-2"
    # "ssdserving-cache-corrlru-8-4"
    # "ssdserving-cache-corrlru-8-8"
    # "ssdserving-cache-fifo"
    # "ssdserving-cache-lfu"
    # "ssdserving-cache-lru"
    "ssdserving"
)

cache_size=(
    # "32mb:33554432"
    # "64mb:67108864"
    # "128mb:134217728"
    # "256mb:268435456"
    # "512mb:536870912"
    "1gb:1073741824"
    "2gb:2147483648"
    "4gb:4294967296"
    "8gb:8589934592"
    "16gb:17179869184"
    "32gb:34359738368"
    "48gb:51539607552"
    # "56gb:60129542144"
    # "60gb:64424509440"
    # "60.5gb:64961380352"
    # "60.6gb:65068754534"
    # "60.7gb:65176128716"
    # "60.8gb:65283502900"
    # "60.9gb:65390877082"
    # "61.0gb:65498251264"
    # "61.1gb:65605625446"
    # "61.2gb:65712999628"
    # "64gb:68719476736"
)

# 65766146048

thread_num=(
    "1"
    # "2"
    # "4"
    # "8"
    # "16"
)

# Run original first.
# ./Release/ssdserving-original ${configuration_file} > trace/${dataset}-out-original-mt.txt

# mv ./trace/cache-trace.csv ./trace/cache-trace-original-mt.csv 
# mv ./trace/offset-access-trace.csv ./trace/offset-access-trace-original-mt.csv
# mv ./trace/cache-lock-latency.csv ./trace/cache-lock-latency-original-mt.csv

# 
# Run cache-enabled.
for bin in "${exec_binary[@]}"; do
    for nthread in "${thread_num[@]}"; do
        for size in "${cache_size[@]}"; do

            IFS=':' read -r size_h size_i <<< "$size"

            echo "Running for cache size: ${size_h}(${size_i}bytes)"

            ./Release/${bin} custom-configs/spacev1b-search-only-log-mt-t${nthread}.ini ${size_i} > trace/spacev1b-out-${size_h}-mt-${nthread}.txt
            # ./Release/${bin} custom-configs/sift1m-search-only-mt-t${nthread}.ini ${size_i} > trace/sift1m-out-${size_h}-mt-${nthread}.txt
            
            # gdb -ex=run --args ./Release/ssdserving custom-configs/sift1m-search-only-mt-t${nthread}.ini ${size_i}

            wait
             
            mv ./trace/search-trace-raw.csv ./trace/search-trace-raw-${size_h}-mt-${nthread}.csv 
            mv ./trace/cache-trace.csv ./trace/cache-trace-${size_h}-mt-${nthread}.csv 
            mv ./trace/offset-access-trace.csv ./trace/offset-access-trace-${size_h}-mt-${nthread}.csv
            mv ./trace/cache-lock-latency-search.csv ./trace/cache-lock-latency-search-${size_h}-mt-${nthread}.csv
            mv ./trace/cache-lock-latency-refresh.csv ./trace/cache-lock-latency-refresh-${size_h}-mt-${nthread}.csv

            sleep 10
        done
    done

    dir_name="${bin}-$(date +'%Y-%m-%d_%H-%M-%S')"
    mkdir -p trace/${dir_name}

    rm trace/offset-access*
    rm trace/layout-info.csv

    rm trace/cache-lock-latency*

    mv trace/*.csv trace/${dir_name}
    mv trace/*.txt trace/${dir_name}


    cp trace/*.py trace/${dir_name}

    cd trace/${dir_name}
    python3 export-stats-spacev1b.py
    cd -

done
