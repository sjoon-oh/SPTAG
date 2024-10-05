#!/bin/bash

cd build && make -j
cd ..

mv ./Release/ssdserving ./Release/ssdserving-cache
rm trace/*.txt

# 
# Run Original
# dataset="spacev1b"
dataset="spacev1b"
configuration_file="custom-configs/${dataset}-search-only-log-mt.ini"

cache_size=(
    # "128mb:134217728"
    # "256mb:268435456"
    # "512mb:536870912"
    "1gb:1073741824"
    "2gb:2147483648"
    # "3gb:3221225472"
    "4gb:4294967296"
    "8gb:8589934592"
    "16gb:17179869184"
    "32gb:34359738368"
    "48gb:51539607552"
    # "52gb:55834574848"
    "56gb:60129542144"
    # "60gb:64424509440"
    "60.5gb:65498251264"
    "64gb:68719476736"
)

# Run original first.
./Release/ssdserving-original ${configuration_file} > trace/${dataset}-out-original-mt.txt

mv ./trace/cache-trace.csv ./trace/cache-trace-original-mt.csv 
mv ./trace/offset-access-trace.csv ./trace/offset-access-trace-original-mt.csv
mv ./trace/cache-lock-latency.csv ./trace/cache-lock-latency-original-mt.csv

# 
# Run cache-enabled.
for size in "${cache_size[@]}"; do

    IFS=':' read -r size_h size_i <<< "$size"

    echo "Running for cache size: ${size_h}(${size_i}bytes)"

    ./Release/ssdserving-cache ${configuration_file} ${size_i} > trace/${dataset}-out-${size_h}-mt.txt
    wait

    # mv cache-get-latency.csv cache-get-latency-${size_h}-mt.csv 
    # mv cache-set-latency.csv cache-set-latency-${size_h}-mt.csv 
    mv ./trace/cache-trace.csv ./trace/cache-trace-${size_h}-mt.csv 
    mv ./trace/offset-access-trace.csv ./trace/offset-access-trace-${size_h}-mt.csv
    mv ./trace/cache-lock-latency.csv ./trace/cache-lock-latency-${size_h}-mt.csv

    sleep 10
done

dir_name="round-$(date +'%Y-%m-%d_%H-%M-%S')"
mkdir -p trace/${dir_name}

mv trace/*.csv trace/${dir_name}
mv trace/*.txt trace/${dir_name}
